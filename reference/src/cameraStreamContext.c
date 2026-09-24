//------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2026 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
//
//------------------------------ tabstop = 4 ----------------------------------

#include "cameraStreamContext.h"
#include "barton-core-reference-io.h"
#include "cameraMediaServer.h"
#include <stdio.h>

// The SIGINT handler may only touch an async-signal-safe flag, so the blocking wait loop polls
// it on this interval to observe Ctrl+C promptly without the handler taking locks.
#define CAMERA_SIGINT_POLL_INTERVAL_USEC (200 * G_TIME_SPAN_MILLISECOND)

struct CameraStreamContext
{
    GMutex mutex;
    GCond cond;

    // Lifecycle control, guarded by mutex.
    gboolean teardown;     // Ctrl+C or a media pipeline stop
    gboolean sessionEnded; // the camera ended the session
    gchar *errorMessage;

    CameraDeviceSession *session; // not owned

    // Output sink: exactly one is active. mediaServer serves fragmented MP4 over HTTP; outFile
    // records it. Both are fed by cameraStreamContextPushBuffer.
    const gchar *filePath; // not owned; record target, or NULL to serve
    const gchar *serveHost;
    guint16 servePort;
    CameraMediaServer *mediaServer;
    FILE *outFile;

    // Async-signal-safe Ctrl+C flag owned by the command; polled here (not owned).
    const volatile sig_atomic_t *interrupt;
};

CameraStreamContext *cameraStreamContextCreate(CameraDeviceSession *session,
                                               const gchar *filePath,
                                               const gchar *serveHost,
                                               guint16 servePort,
                                               const volatile sig_atomic_t *interrupt)
{
    CameraStreamContext *ctx = g_new0(CameraStreamContext, 1);
    g_mutex_init(&ctx->mutex);
    g_cond_init(&ctx->cond);
    ctx->session = session;
    ctx->filePath = filePath;
    ctx->serveHost = serveHost;
    ctx->servePort = servePort;
    ctx->interrupt = interrupt;

    return ctx;
}

void cameraStreamContextDestroy(CameraStreamContext *ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    if (ctx->mediaServer != NULL)
    {
        cameraMediaServerDestroy(ctx->mediaServer);
    }

    if (ctx->outFile != NULL)
    {
        fclose(ctx->outFile);
    }

    g_free(ctx->errorMessage);
    g_mutex_clear(&ctx->mutex);
    g_cond_clear(&ctx->cond);
    g_free(ctx);
}

CameraDeviceSession *cameraStreamContextGetSession(CameraStreamContext *ctx)
{
    return ctx != NULL ? ctx->session : NULL;
}

void cameraStreamContextLock(CameraStreamContext *ctx)
{
    g_mutex_lock(&ctx->mutex);
}

void cameraStreamContextUnlock(CameraStreamContext *ctx)
{
    g_mutex_unlock(&ctx->mutex);
}

void cameraStreamContextWake(CameraStreamContext *ctx)
{
    g_cond_signal(&ctx->cond);
}

bool cameraStreamContextWaitFlag(CameraStreamContext *ctx, gboolean *flag, gint timeoutSeconds)
{
    gint64 deadline = g_get_monotonic_time() + (gint64) timeoutSeconds * G_USEC_PER_SEC;
    g_mutex_lock(&ctx->mutex);

    while (!*flag && !ctx->teardown && !ctx->sessionEnded)
    {
        if (*ctx->interrupt)
        {
            ctx->teardown = TRUE;

            break;
        }

        gint64 now = g_get_monotonic_time();

        if (now >= deadline)
        {
            g_mutex_unlock(&ctx->mutex);

            return FALSE;
        }

        // Wake periodically to poll the async-signal-safe SIGINT flag, since the signal handler
        // cannot safely take the mutex or signal the condition variable.
        gint64 wakeup = now + CAMERA_SIGINT_POLL_INTERVAL_USEC;

        if (wakeup > deadline)
        {
            wakeup = deadline;
        }

        g_cond_wait_until(&ctx->cond, &ctx->mutex, wakeup);
    }

    gboolean result = *flag;
    g_mutex_unlock(&ctx->mutex);

    return result;
}

bool cameraStreamContextTornDown(CameraStreamContext *ctx)
{
    g_mutex_lock(&ctx->mutex);
    gboolean tornDown = ctx->teardown || ctx->sessionEnded;
    g_mutex_unlock(&ctx->mutex);

    return tornDown;
}

void cameraStreamContextOnSessionEnded(const gchar *message, gpointer ctxPtr)
{
    CameraStreamContext *ctx = (CameraStreamContext *) ctxPtr;
    g_mutex_lock(&ctx->mutex);
    g_free(ctx->errorMessage);
    ctx->errorMessage = g_strdup(message);
    ctx->sessionEnded = TRUE;
    g_cond_signal(&ctx->cond);
    g_mutex_unlock(&ctx->mutex);
}

void cameraStreamContextRequestTeardown(CameraStreamContext *ctx)
{
    g_mutex_lock(&ctx->mutex);
    ctx->teardown = TRUE;
    g_cond_signal(&ctx->cond);
    g_mutex_unlock(&ctx->mutex);
}

void cameraStreamContextPushBuffer(CameraStreamContext *ctx, const guint8 *data, gsize size, gboolean isHeader)
{
    g_mutex_lock(&ctx->mutex);
    CameraMediaServer *server = ctx->mediaServer;
    FILE *outFile = ctx->outFile;
    g_mutex_unlock(&ctx->mutex);

    // Serve mode: fan the fragmented-MP4 buffer out to viewers. Record mode: append it to the
    // file. (outFile is only closed in cameraStreamContextDestroy, after the media client -- and
    // therefore this callback -- has stopped, so the pointer stays valid for the write here.)
    if (server != NULL)
    {
        cameraMediaServerPushBuffer(server, data, size, isHeader);
    }

    if (outFile != NULL)
    {
        fwrite(data, 1, size, outFile);
        fflush(outFile);
    }
}

bool cameraStreamContextStartSink(CameraStreamContext *ctx,
                                  CameraStreamViewerHandler viewerHandler,
                                  gpointer viewerUserData)
{
    if (ctx->filePath != NULL)
    {
        ctx->outFile = fopen(ctx->filePath, "wb");

        if (ctx->outFile == NULL)
        {
            emitError("[camera-stream] Failed to open %s for writing\n", ctx->filePath);

            return false;
        }

        emitOutput("[camera-stream] Recording camera video to %s\n", ctx->filePath);

        return true;
    }

    CameraMediaServer *mediaServer = cameraMediaServerCreate(ctx->serveHost, ctx->servePort);

    if (mediaServer == NULL)
    {
        emitError("[camera-stream] Failed to start the media server\n");

        return false;
    }

    g_mutex_lock(&ctx->mutex);
    ctx->mediaServer = mediaServer;
    g_mutex_unlock(&ctx->mutex);

    if (viewerHandler != NULL)
    {
        cameraMediaServerSetOnViewer(mediaServer, viewerHandler, viewerUserData);
    }

    emitOutput("[camera-stream] Serving camera video at %s\n", cameraMediaServerGetUrl(mediaServer));

    return true;
}

void cameraStreamContextAwaitTeardown(CameraStreamContext *ctx)
{
    if (ctx->filePath != NULL)
    {
        emitOutput("[camera-stream] Media flowing, recording to %s. Press Ctrl+C to stop.\n", ctx->filePath);
    }
    else
    {
        emitOutput("[camera-stream] Media flowing at %s. Press Ctrl+C to stop.\n",
                   cameraMediaServerGetUrl(ctx->mediaServer));
    }

    // waitFlag returns FALSE on timeout, so loop to re-arm it rather than silently stopping a
    // long-lived stream once the timeout elapses.
    while (!cameraStreamContextTornDown(ctx))
    {
        cameraStreamContextWaitFlag(ctx, &ctx->teardown, 3600);
    }

    // Snapshot the session-ended state under the lock; cameraStreamContextOnSessionEnded mutates
    // these fields from the session status-callback thread.
    g_mutex_lock(&ctx->mutex);
    gboolean sessionEnded = ctx->sessionEnded;
    g_autofree gchar *errorMessage = g_strdup(ctx->errorMessage);
    g_mutex_unlock(&ctx->mutex);

    if (sessionEnded)
    {
        emitOutput("[camera-stream] Session ended: %s\n", errorMessage != NULL ? errorMessage : "unknown reason");
    }
    else
    {
        emitOutput("\n[camera-stream] Stopping stream...\n");
    }

    // Stop the media server from invoking the backend's new-viewer callback before the caller
    // tears the backend (and its media client) down.
    if (ctx->mediaServer != NULL)
    {
        cameraMediaServerSetOnViewer(ctx->mediaServer, NULL, NULL);
    }
}
