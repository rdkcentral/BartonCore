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

/*
 * Protocol-agnostic context shared by the camera stream command and its technology backends.
 *
 * Owns everything that is common to every streaming technology: the device session, the output
 * sink (an HTTP media server or a record file), cross-thread teardown signaling, and the wait
 * loop that observes Ctrl+C and camera-ended-session events. Backends reach the session and push
 * muxed media buffers through this interface; none of the technology-specific streaming state
 * lives here.
 */

#pragma once

#include "cameraDeviceSession.h"
#include <glib.h>
#include <signal.h>
#include <stdbool.h>

typedef struct CameraStreamContext CameraStreamContext;

/**
 * Callback invoked (on the media server thread) when a new viewer connects, letting a backend
 * that can honor a keyframe request issue one. Only serve mode calls it.
 *
 * @param userData opaque user data supplied to cameraStreamContextStartSink
 */
typedef void (*CameraStreamViewerHandler)(gpointer userData);

/**
 * Create the shared context.
 *
 * @param session    the camera device session (not owned; the caller frees it after the context)
 * @param filePath   record target, or NULL to serve over HTTP
 * @param serveHost  serve host (ignored in record mode)
 * @param servePort  serve port (ignored in record mode)
 * @param interrupt  an async-signal-safe flag set by the command's SIGINT handler; the wait loop
 *                   polls it to observe Ctrl+C (not owned)
 * @return the context, or NULL on error
 */
CameraStreamContext *cameraStreamContextCreate(CameraDeviceSession *session,
                                               const gchar *filePath,
                                               const gchar *serveHost,
                                               guint16 servePort,
                                               const volatile sig_atomic_t *interrupt);

/**
 * Destroy the context, closing the record file if one is open.
 *
 * @param ctx the context (may be NULL)
 */
void cameraStreamContextDestroy(CameraStreamContext *ctx);

/**
 * @param ctx the context
 * @return the device session
 */
CameraDeviceSession *cameraStreamContextGetSession(CameraStreamContext *ctx);

/**
 * Take/release the shared lock guarding the context's control flags and any backend readiness
 * flags waited on via cameraStreamContextWaitFlag. Backends must flip such flags under this lock
 * and then call cameraStreamContextWake.
 *
 * @param ctx the context
 */
void cameraStreamContextLock(CameraStreamContext *ctx);
void cameraStreamContextUnlock(CameraStreamContext *ctx);

/**
 * Wake any thread blocked in cameraStreamContextWaitFlag.
 *
 * @param ctx the context
 */
void cameraStreamContextWake(CameraStreamContext *ctx);

/**
 * Block until @p flag becomes TRUE, the camera ends the session, or Ctrl+C is pressed. @p flag is
 * guarded by the context lock; a backend flips it under the lock and calls cameraStreamContextWake.
 *
 * @param ctx            the context
 * @param flag           the boolean to wait on
 * @param timeoutSeconds maximum wait
 * @return the final value of @p flag
 */
bool cameraStreamContextWaitFlag(CameraStreamContext *ctx, gboolean *flag, gint timeoutSeconds);

/**
 * @param ctx the context
 * @return true once Ctrl+C, a pipeline stop, or a camera session-end has been observed
 */
bool cameraStreamContextTornDown(CameraStreamContext *ctx);

/**
 * Device-session status callback (matches CameraDeviceOnSessionError): records the message and
 * marks the session ended so the wait loop returns.
 *
 * @param message human-readable reason (caller does not free)
 * @param ctx     the context (as gpointer)
 */
void cameraStreamContextOnSessionEnded(const gchar *message, gpointer ctx);

/**
 * Request a graceful teardown (used as a media client's stop/error callback).
 *
 * @param ctx the context
 */
void cameraStreamContextRequestTeardown(CameraStreamContext *ctx);

/**
 * Push one muxed fragmented-MP4 buffer to the active sink (HTTP server or record file). Backends
 * wire their media client's buffer callback to this.
 *
 * @param ctx      the context
 * @param data     buffer bytes
 * @param size     number of bytes
 * @param isHeader TRUE for init-segment buffers
 */
void cameraStreamContextPushBuffer(CameraStreamContext *ctx, const guint8 *data, gsize size, gboolean isHeader);

/**
 * Start the output sink: open the record file, or start the HTTP media server. @p viewerHandler
 * (may be NULL) hooks the media server's new-viewer callback for protocols that can honor a
 * keyframe request; it is cleared automatically by cameraStreamContextAwaitTeardown.
 *
 * @param ctx             the context
 * @param viewerHandler   new-viewer handler, or NULL
 * @param viewerUserData  opaque data for @p viewerHandler
 * @return true on success
 */
bool cameraStreamContextStartSink(CameraStreamContext *ctx,
                                  CameraStreamViewerHandler viewerHandler,
                                  gpointer viewerUserData);

/**
 * Announce that media is flowing, then block until Ctrl+C or the camera ends the session, and
 * finally sever the media server's callbacks so late invocations become no-ops. Backends call
 * this once their media pipeline is running.
 *
 * @param ctx the context
 */
void cameraStreamContextAwaitTeardown(CameraStreamContext *ctx);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(CameraStreamContext, cameraStreamContextDestroy)
