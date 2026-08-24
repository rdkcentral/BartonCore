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

#include "cameraOnvifBackend.h"
#include "barton-core-reference-io.h"
#include "cameraRtspClient.h"
#include <curl/curl.h>
#include <stdio.h>

typedef struct
{
    CameraStreamBackend base; // must be first so a CameraStreamBackend* aliases this struct
    CameraStreamContext *ctx; // set for the duration of run()

    // Interim credentials and optional snapshot target, from the command line.
    gchar *user;
    gchar *pass;
    gchar *snapshotPath;

    // The RTSP media URL and snapshot URL arrive asynchronously as device-session resource events.
    gchar *mediaUrl;
    gboolean mediaUrlReady;
    gchar *snapshotUrl;
    gboolean snapshotUrlReady;
} CameraOnvifBackend;

// ============================================================================
// Device-session callbacks (userData is the backend)
// ============================================================================

static void onMediaUrl(const gchar *url, gpointer userData)
{
    CameraOnvifBackend *self = (CameraOnvifBackend *) userData;
    cameraStreamContextLock(self->ctx);
    g_free(self->mediaUrl);
    self->mediaUrl = g_strdup(url);
    self->mediaUrlReady = TRUE;
    cameraStreamContextWake(self->ctx);
    cameraStreamContextUnlock(self->ctx);
}

static void onSnapshotUrl(const gchar *url, gpointer userData)
{
    CameraOnvifBackend *self = (CameraOnvifBackend *) userData;
    cameraStreamContextLock(self->ctx);
    g_free(self->snapshotUrl);
    self->snapshotUrl = g_strdup(url);
    self->snapshotUrlReady = TRUE;
    cameraStreamContextWake(self->ctx);
    cameraStreamContextUnlock(self->ctx);
}

// ============================================================================
// RTSP client callbacks (userData is the backend)
// ============================================================================

static void onRtspClosed(gpointer userData)
{
    CameraOnvifBackend *self = (CameraOnvifBackend *) userData;
    cameraStreamContextRequestTeardown(self->ctx);
}

static void onRtspBuffer(const guint8 *data, gsize size, gboolean isHeader, gpointer userData)
{
    CameraOnvifBackend *self = (CameraOnvifBackend *) userData;
    cameraStreamContextPushBuffer(self->ctx, data, size, isHeader);
}

// ============================================================================
// Snapshot
// ============================================================================

// libcurl write callback: append the received snapshot bytes to the open output file.
static size_t snapshotWriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    FILE *out = (FILE *) userdata;

    // libcurl expects the number of *bytes* consumed; fwrite returns the item count, so scale by size.
    return fwrite(ptr, size, nmemb, out) * size;
}

// Perform the snapshot HTTP GET to `url`, writing the body to `out`. Credentials (if any) are
// supplied via HTTP Basic/Digest negotiation (CURLAUTH_ANY) -- the ONVIF-spec-compliant path that
// standards-based cameras and the mock use. Cameras that don't require snapshot auth (e.g. the mock)
// never issue a WWW-Authenticate challenge, so nothing is sent and the GET simply succeeds. Returns
// true only on a 2xx HTTP response: a completed transfer that returns e.g. 401/403 HTML is a
// failure, not a success.
static bool snapshotFetch(const gchar *url, const gchar *user, const gchar *pass, FILE *out)
{
    CURL *curl = curl_easy_init();

    if (curl == NULL)
    {
        return false;
    }

    bool haveCreds = (user != NULL && user[0] != '\0') || (pass != NULL && pass[0] != '\0');

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, snapshotWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, out);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

    // Restrict to HTTP(S), including across redirects, so a malicious snapshot URL cannot coerce
    // libcurl into file://, scp://, or other schemes (local file read / SSRF).
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, (long) (CURLPROTO_HTTP | CURLPROTO_HTTPS));
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, (long) (CURLPROTO_HTTP | CURLPROTO_HTTPS));

    // Be permissive about TLS: IP cameras routinely serve their snapshot endpoint over HTTPS
    // with a self-signed or hostname-mismatched certificate (e.g. Reolink redirects http snapshot
    // requests to https). This is a developer reference app, so disable peer/host verification
    // rather than failing the capture with "SSL peer certificate ... was not OK".
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    // The snapshot URL is credential-free per the ONVIF spec, but the HTTP GET still needs the
    // same device credentials for basic/digest auth. CURLAUTH_ANY lets the server negotiate.
    if (haveCreds)
    {
        curl_easy_setopt(curl, CURLOPT_HTTPAUTH, (long) CURLAUTH_ANY);
        curl_easy_setopt(curl, CURLOPT_USERNAME, user != NULL ? user : "");
        curl_easy_setopt(curl, CURLOPT_PASSWORD, pass != NULL ? pass : "");
    }

    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
    {
        emitError("[camera-stream] Snapshot download failed: %s\n", curl_easy_strerror(res));
        return false;
    }

    // A completed transfer can still be an auth/error page (401/403 HTML). Only treat 2xx as success.
    // httpCode is 0 for non-HTTP schemes (e.g. file://), which we also accept.
    if (httpCode != 0 && (httpCode < 200 || httpCode >= 300))
    {
        emitError("[camera-stream] Snapshot download returned HTTP %ld\n", httpCode);
        return false;
    }

    return true;
}

// Take a picture: execute the takePicture -> getSnapshotUrl springboard, wait for the snapshotUrl
// event, then download the JPEG to snapshotPath over HTTP (using the same credentials for
// basic/digest auth). Returns false on any failure; the caller treats a failed snapshot as
// non-fatal for the video stream.
static bool captureSnapshot(CameraOnvifBackend *self, CameraStreamContext *ctx, CameraDeviceSession *session)
{
    emitOutput("[camera-stream] Taking a picture...\n");

    if (!cameraDeviceSessionOnvifRequestSnapshotUrl(session))
    {
        emitError("[camera-stream] Failed to request the snapshot URL\n");

        return false;
    }

    if (!cameraStreamContextWaitFlag(ctx, &self->snapshotUrlReady, 15))
    {
        emitError("[camera-stream] Timeout waiting for the snapshot URL\n");

        return false;
    }

    if (cameraStreamContextTornDown(ctx))
    {
        return false;
    }

    g_autofree gchar *snapshotUrl = NULL;
    cameraStreamContextLock(ctx);
    snapshotUrl = g_strdup(self->snapshotUrl);
    cameraStreamContextUnlock(ctx);

    if (snapshotUrl == NULL)
    {
        emitError("[camera-stream] The camera did not report a snapshot URL\n");

        return false;
    }

    emitOutput("[camera-stream] Snapshot URL: %s\n", snapshotUrl);

    FILE *out = fopen(self->snapshotPath, "wb");

    if (out == NULL)
    {
        emitError("[camera-stream] Failed to open %s for writing\n", self->snapshotPath);

        return false;
    }

    bool ok = snapshotFetch(snapshotUrl, self->user, self->pass, out);

    fclose(out);

    if (ok)
    {
        emitOutput("[camera-stream] Saved snapshot to %s\n", self->snapshotPath);
    }

    return ok;
}

// ============================================================================
// Backend vfuncs
// ============================================================================

static bool onvifRun(CameraStreamBackend *base, CameraStreamContext *ctx)
{
    CameraOnvifBackend *self = (CameraOnvifBackend *) base;
    self->ctx = ctx;
    CameraDeviceSession *session = cameraStreamContextGetSession(ctx);

    // Deliver the camera's media/snapshot URLs to this backend.
    cameraDeviceSessionSetOnvifCallbacks(session, onMediaUrl, onSnapshotUrl, self);

    emitOutput("[camera-stream] ONVIF/RTSP camera.\n");

    // Interim auth model: credentials are supplied via --user/--pass and written to the onvif
    // endpoint so the driver can authenticate its SOAP/RTSP calls. This primitive per-device
    // credential mechanism is expected to be reworked with future configuration support.
    if ((self->user != NULL && self->user[0] != '\0') || (self->pass != NULL && self->pass[0] != '\0'))
    {
        emitOutput("[camera-stream] Applying credentials...\n");

        if (!cameraDeviceSessionOnvifSetCredentials(session, self->user, self->pass))
        {
            emitError("[camera-stream] Failed to write ONVIF credentials\n");

            return false;
        }
    }

    g_autofree gchar *authRequired = cameraDeviceSessionOnvifReadAuthRequired(session);

    if (g_strcmp0(authRequired, "true") == 0 && (self->user == NULL || self->user[0] == '\0'))
    {
        emitOutput("[camera-stream] Note: camera reports authRequired=true but no --user/--pass was "
                   "supplied; the stream may be rejected.\n");
    }

    // Optional still capture, independent of the video stream. A failed snapshot is non-fatal.
    if (self->snapshotPath != NULL)
    {
        if (!captureSnapshot(self, ctx, session))
        {
            emitError("[camera-stream] Snapshot capture failed; continuing with the video stream\n");
        }

        if (cameraStreamContextTornDown(ctx))
        {
            return false;
        }
    }

    // Request the RTSP media URL; the driver fetches it on-demand via ONVIF SOAP and emits it as a
    // mediaUrl event delivered through onMediaUrl.
    emitOutput("[camera-stream] Requesting RTSP media URL...\n");

    if (!cameraDeviceSessionOnvifRequestMediaUrl(session))
    {
        emitError("[camera-stream] Failed to request the RTSP media URL\n");

        return false;
    }

    if (!cameraStreamContextWaitFlag(ctx, &self->mediaUrlReady, 15))
    {
        emitError("[camera-stream] Timeout waiting for the RTSP media URL\n");

        return false;
    }

    if (cameraStreamContextTornDown(ctx))
    {
        return false;
    }

    g_autofree gchar *rtspUrl = NULL;
    cameraStreamContextLock(ctx);
    rtspUrl = g_strdup(self->mediaUrl);
    cameraStreamContextUnlock(ctx);

    if (rtspUrl == NULL)
    {
        emitError("[camera-stream] The camera did not report an RTSP media URL\n");

        return false;
    }

    emitOutput("[camera-stream] RTSP media URL: %s\n", rtspUrl);

    // Start the sink before the client so the HTTP server receives the pipeline's first
    // (init-segment) buffers. The RTSP source cannot honor a keyframe request, so no viewer hook.
    if (!cameraStreamContextStartSink(ctx, NULL, NULL))
    {
        return false;
    }

    g_autoptr(CameraRtspClient) rtsp = cameraRtspClientCreate(onRtspClosed, onRtspBuffer, self);

    if (rtsp == NULL)
    {
        emitError("[camera-stream] Failed to create the RTSP client\n");

        return false;
    }

    if (!cameraRtspClientStart(rtsp, rtspUrl, self->user, self->pass))
    {
        emitError("[camera-stream] Failed to start the RTSP client\n");

        return false;
    }

    cameraStreamContextAwaitTeardown(ctx);

    return true;
}

static void onvifDestroy(CameraStreamBackend *base)
{
    CameraOnvifBackend *self = (CameraOnvifBackend *) base;

    g_free(self->user);
    g_free(self->pass);
    g_free(self->snapshotPath);
    g_free(self->mediaUrl);
    g_free(self->snapshotUrl);
    g_free(self);
}

CameraStreamBackend *cameraOnvifBackendCreate(const CameraStreamOptions *options)
{
    CameraOnvifBackend *self = g_new0(CameraOnvifBackend, 1);
    self->base.run = onvifRun;
    self->base.destroy = onvifDestroy;

    if (options != NULL)
    {
        self->user = g_strdup(options->user);
        self->pass = g_strdup(options->pass);
        self->snapshotPath = g_strdup(options->snapshotPath);
    }

    return &self->base;
}
