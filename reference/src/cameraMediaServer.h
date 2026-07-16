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
 * Minimal HTTP server that streams the camera's fragmented-MP4 video to a
 * browser <video> element over a single long-lived GET.
 *
 * The reference app is the in-container WebRTC peer to the camera (direct UDP,
 * no TURN); it muxes the received H.264 into fragmented MP4 and feeds the bytes
 * here via cameraMediaServerPushBuffer(). Each connecting browser first receives
 * the cached init segment (ftyp + moov) and then the live fragments, so a viewer
 * that joins mid-stream still gets a decodable stream. Served on loopback and
 * reached from the developer's machine through a forwarded port.
 */

#pragma once

#include <glib.h>
#include <stdbool.h>

typedef struct _CameraMediaServer CameraMediaServer;

/**
 * Invoked (on the server thread) each time a viewer connects to the stream. Handlers should be
 * quick and must not call back into the server. Typically used to request a fresh keyframe so a
 * newly-connected viewer can begin decoding without delay.
 *
 * @param userData caller context registered with cameraMediaServerSetOnViewer()
 */
typedef void (*CameraMediaServerOnViewer)(gpointer userData);

/**
 * Create and start the HTTP media server.
 *
 * @param bindHost  loopback address to bind (e.g. "127.0.0.1")
 * @param port      TCP port to listen on
 * @return the server, or NULL on error
 */
CameraMediaServer *cameraMediaServerCreate(const gchar *bindHost, guint16 port);

/**
 * Register a callback invoked whenever a new viewer connects to the live stream.
 *
 * @param server    the server
 * @param onViewer  callback (may be NULL to clear)
 * @param userData  context passed to the callback
 */
void cameraMediaServerSetOnViewer(CameraMediaServer *server, CameraMediaServerOnViewer onViewer, gpointer userData);

/**
 * Feed one muxed fragmented-MP4 buffer to all connected viewers.
 *
 * @param server    the server
 * @param data      buffer bytes
 * @param size      number of bytes
 * @param isHeader  TRUE for init-segment buffers (ftyp/moov), which are cached and
 *                  replayed to viewers that connect later; FALSE for live fragments
 */
void cameraMediaServerPushBuffer(CameraMediaServer *server, const guint8 *data, gsize size, gboolean isHeader);

/**
 * Get the URL a browser should open (e.g. "http://127.0.0.1:8088").
 *
 * @param server the server
 * @return the URL (owned by the server)
 */
const gchar *cameraMediaServerGetUrl(CameraMediaServer *server);

/**
 * Stop the server and release all resources, closing any open viewer connections.
 *
 * @param server the server (may be NULL)
 */
void cameraMediaServerDestroy(CameraMediaServer *server);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(CameraMediaServer, cameraMediaServerDestroy)
