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
 * RTSP client backed by GStreamer for the ONVIF camera stream path.
 *
 * Wraps an rtspsrc pipeline that pulls the camera's H.264 over RTSP and muxes it
 * into fragmented MP4 (delivered buffer-by-buffer for HTTP streaming or file
 * recording). No decode. This mirrors the WebRTC client's downstream mux/appsink
 * chain so both protocols feed the same media-server / file sink; only the media
 * source (rtspsrc vs webrtcbin) differs.
 */

#pragma once

#include <glib.h>
#include <stdbool.h>

typedef struct _CameraRtspClient CameraRtspClient;

/**
 * Callback invoked when the pipeline stops on its own — e.g. the RTSP source
 * failed (bus ERROR) or the stream reached end-of-stream (bus EOS). Lets the
 * caller tear the session down gracefully instead of leaking it.
 *
 * @param userData  opaque user data
 */
typedef void (*CameraRtspOnClosed)(gpointer userData);

/**
 * Callback invoked for each muxed fragmented-MP4 buffer produced from the RTSP video.
 *
 * @param data      buffer bytes (caller does not free)
 * @param size      number of bytes
 * @param isHeader  TRUE for init-segment buffers (ftyp/moov)
 * @param userData  opaque user data
 */
typedef void (*CameraRtspOnMediaBuffer)(const guint8 *data, gsize size, gboolean isHeader, gpointer userData);

/**
 * Create a new RTSP client. The received H.264 is muxed into fragmented MP4 and delivered
 * buffer-by-buffer via @p onBuffer; the caller decides where it goes (a file or an HTTP server).
 *
 * @param onClosed  callback when the pipeline stops unexpectedly (error / EOS)
 * @param onBuffer  callback for each muxed fragmented-MP4 buffer
 * @param userData  opaque data passed to callbacks
 * @return the client, or NULL on error
 */
CameraRtspClient *
cameraRtspClientCreate(CameraRtspOnClosed onClosed, CameraRtspOnMediaBuffer onBuffer, gpointer userData);

/**
 * Start pulling the given RTSP URL. Optional credentials are applied to rtspsrc for RTSP
 * digest/basic auth; pass NULL/empty to stream without credentials. Must be called once.
 *
 * @param client   the client
 * @param rtspUrl  the rtsp:// URL to pull
 * @param user     the RTSP username, or NULL/empty for none
 * @param pass     the RTSP password, or NULL/empty for none
 * @return true on success
 */
bool cameraRtspClientStart(CameraRtspClient *client, const gchar *rtspUrl, const gchar *user, const gchar *pass);

/**
 * Stop and destroy the client, releasing all resources.
 *
 * @param client the client (may be NULL)
 */
void cameraRtspClientDestroy(CameraRtspClient *client);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(CameraRtspClient, cameraRtspClientDestroy)
