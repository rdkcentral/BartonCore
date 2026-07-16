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
 * Camera session flows over the Barton client interface for the camera stream
 * command.
 *
 * Owns the camera/webrtc resource URIs for a single device: creating and
 * destroying the camera session, starting the stream, relaying the local SDP
 * offer and ICE candidates to the camera, and subscribing to resource-updated
 * events to deliver the remote SDP answer, remote ICE candidates, and session
 * status back to the caller via callbacks.
 */

#pragma once

#include "barton-core-client.h"
#include <glib.h>
#include <stdbool.h>

typedef struct _CameraDeviceSession CameraDeviceSession;

/**
 * Callback invoked when the camera's remote SDP answer arrives.
 *
 * @param sdp       the remote SDP answer string (caller does not free)
 * @param userData  opaque user data
 */
typedef void (*CameraDeviceOnRemoteSdp)(const gchar *sdp, gpointer userData);

/**
 * Callback invoked when the camera's remote ICE candidates arrive.
 *
 * @param jsonCandidates  a JSON array string of candidate strings (caller does not free)
 * @param userData        opaque user data
 */
typedef void (*CameraDeviceOnRemoteIce)(const gchar *jsonCandidates, gpointer userData);

/**
 * Callback invoked when the camera reports the session ended in error.
 *
 * @param message   a human-readable error message (caller does not free)
 * @param userData  opaque user data
 */
typedef void (*CameraDeviceOnSessionError)(const gchar *message, gpointer userData);

/**
 * Create a camera device session and subscribe to resource-updated events.
 *
 * @param client       the Barton client
 * @param deviceId     the camera device id
 * @param onRemoteSdp  callback for the remote SDP answer
 * @param onRemoteIce  callback for remote ICE candidates
 * @param onError      callback for a session-ended-in-error status
 * @param userData     opaque data passed to callbacks
 * @return the session, or NULL on error
 */
CameraDeviceSession *cameraDeviceSessionCreate(BCoreClient *client,
                                               const gchar *deviceId,
                                               CameraDeviceOnRemoteSdp onRemoteSdp,
                                               CameraDeviceOnRemoteIce onRemoteIce,
                                               CameraDeviceOnSessionError onError,
                                               gpointer userData);

/**
 * Create the camera session (allocates a session id on the camera).
 *
 * @param session the session
 * @return true on success
 */
bool cameraDeviceSessionOpen(CameraDeviceSession *session);

/**
 * Start the camera stream for the open session.
 *
 * @param session the session
 * @return true on success
 */
bool cameraDeviceSessionStartStream(CameraDeviceSession *session);

/**
 * Get the active protocol reported by the stream execute (e.g. "webrtc"), or NULL if
 * the stream has not started or the driver did not report one. The session owns the string.
 *
 * @param session the session
 * @return the protocol identifier, or NULL
 */
const gchar *cameraDeviceSessionGetProtocol(CameraDeviceSession *session);

/**
 * Get the entry-point resource URI reported by the stream execute, or NULL if unavailable.
 * The session owns the string.
 *
 * @param session the session
 * @return the entry-point URI, or NULL
 */
const gchar *cameraDeviceSessionGetEntryPoint(CameraDeviceSession *session);

/**
 * Get the negotiation role for the active protocol ("offerer" or "answerer"), or NULL if
 * unavailable. For WebRTC this is read from the webrtc endpoint's negotiationRole resource on first
 * request and cached. The session owns the string.
 *
 * @param session the session
 * @return the role string, or NULL
 */
const gchar *cameraDeviceSessionGetRole(CameraDeviceSession *session);

/**
 * Send the local SDP offer to the camera.
 *
 * @param session the session
 * @param sdp     the local SDP offer string
 * @return true on success
 */
bool cameraDeviceSessionSendOffer(CameraDeviceSession *session, const gchar *sdp);

/**
 * Send local ICE candidates to the camera.
 *
 * @param session         the session
 * @param jsonCandidates  a JSON array string of candidate strings
 * @return true on success
 */
bool cameraDeviceSessionSendIceCandidates(CameraDeviceSession *session, const gchar *jsonCandidates);

/**
 * Unsubscribe from events and free the session. If the session was successfully
 * opened, this first ends it on the device (EndSession) so it is never leaked.
 *
 * @param session the session (may be NULL)
 */
void cameraDeviceSessionDestroy(CameraDeviceSession *session);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(CameraDeviceSession, cameraDeviceSessionDestroy)
