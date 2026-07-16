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
 * WebRTC client backed by GStreamer for the camera stream command.
 *
 * Wraps a webrtcbin pipeline: creates local SDP offers, applies remote SDP
 * answers, exchanges ICE candidates, and muxes the received H.264 into a file
 * or fragmented MP4 (delivered buffer-by-buffer for HTTP streaming). No decode.
 */

#pragma once

#include <glib.h>
#include <stdbool.h>

typedef struct _CameraWebrtcClient CameraWebrtcClient;

/**
 * Negotiated video configuration for the active stream. SDP-derived fields (codec,
 * bitrate, profile, RTP payload) are known once the remote description is set;
 * resolution, frame rate, and pixel format are known once decoded frames flow.
 * Unknown numeric fields are -1 and unknown string fields are empty.
 */
typedef struct
{
    gchar codec[16];          // e.g. "H264"
    gint payloadType;         // RTP payload type
    gint clockRate;           // RTP clock rate (Hz)
    gchar profileLevelId[16]; // H.264 profile-level-id, e.g. "42e01f"
    gint bitrateKbps;         // signaled bandwidth (SDP b=AS)

    gint width;            // pixels
    gint height;           // pixels
    gint framerateNum;     // frame rate numerator
    gint framerateDen;     // frame rate denominator
    gchar pixelFormat[16]; // e.g. "I420"
} CameraWebrtcVideoConfig;

/**
 * Callback invoked when the local SDP offer is ready.
 *
 * @param sdp   the SDP offer string (caller does not free)
 * @param userData  opaque user data
 */
typedef void (*CameraWebrtcOnOfferReady)(const gchar *sdp, gpointer userData);

/**
 * Callback invoked when a local ICE candidate is generated.
 *
 * @param mlineIndex  the m-line index
 * @param candidate   the candidate string
 * @param userData    opaque user data
 */
typedef void (*CameraWebrtcOnIceCandidate)(guint mlineIndex, const gchar *candidate, gpointer userData);

/**
 * Callback invoked when the pipeline stops on its own — e.g. the render window
 * was closed (bus ERROR) or the stream reached end-of-stream (bus EOS). Lets the
 * caller tear the session down gracefully instead of leaking it.
 *
 * @param userData  opaque user data
 */
typedef void (*CameraWebrtcOnClosed)(gpointer userData);

/**
 * Callback invoked for each muxed fragmented-MP4 buffer produced from the received video.
 *
 * @param data      buffer bytes (caller does not free)
 * @param size      number of bytes
 * @param isHeader  TRUE for init-segment buffers (ftyp/moov)
 * @param userData  opaque user data
 */
typedef void (*CameraWebrtcOnMediaBuffer)(const guint8 *data, gsize size, gboolean isHeader, gpointer userData);

/**
 * Create a new WebRTC client. The received H.264 is muxed into fragmented MP4 and delivered
 * buffer-by-buffer via @p onBuffer; the caller decides where it goes (a file or an HTTP server).
 *
 * @param onOffer   callback when local SDP offer is ready
 * @param onIce     callback when local ICE candidate is generated
 * @param onClosed  callback when the pipeline stops unexpectedly (error / EOS)
 * @param onBuffer  callback for each muxed fragmented-MP4 buffer
 * @param userData  opaque data passed to callbacks
 * @return the client, or NULL on error
 */
CameraWebrtcClient *cameraWebrtcClientCreate(CameraWebrtcOnOfferReady onOffer,
                                             CameraWebrtcOnIceCandidate onIce,
                                             CameraWebrtcOnClosed onClosed,
                                             CameraWebrtcOnMediaBuffer onBuffer,
                                             gpointer userData);

/**
 * Start the pipeline and trigger negotiation (creates the SDP offer).
 *
 * @param client the client
 * @return true on success
 */
bool cameraWebrtcClientStart(CameraWebrtcClient *client);

/**
 * Select the negotiation role. In answerer mode (SolicitOffer flow) the client does not create an
 * offer; instead, when the camera's offer is delivered via cameraWebrtcClientSetRemoteSdp(), the
 * client creates and emits an answer through the onOffer callback. Must be called before
 * cameraWebrtcClientStart(). The default (offerer) creates the offer.
 *
 * @param client   the client
 * @param answerer true for the answerer role, false (default) for the offerer role
 */
void cameraWebrtcClientSetAnswerer(CameraWebrtcClient *client, bool answerer);

/**
 * Set the remote SDP answer on the client.
 *
 * @param client the client
 * @param sdp    the SDP answer string
 * @return true on success
 */
bool cameraWebrtcClientSetRemoteSdp(CameraWebrtcClient *client, const gchar *sdp);

/**
 * Add a remote ICE candidate to the client.
 *
 * @param client     the client
 * @param mlineIndex the m-line index
 * @param candidate  the candidate string
 */
void cameraWebrtcClientAddIceCandidate(CameraWebrtcClient *client, guint mlineIndex, const gchar *candidate);

/**
 * Copy the current negotiated video configuration into @p config. SDP-derived fields
 * are populated once the remote description is set; resolution and frame rate are
 * populated once decoded frames begin flowing (unknown fields are -1 / empty).
 *
 * @param client the client
 * @param config receives the current configuration
 */
void cameraWebrtcClientGetVideoConfig(CameraWebrtcClient *client, CameraWebrtcVideoConfig *config);

/**
 * Request an immediate keyframe from the camera (via an RTCP PLI). Useful at stream start and
 * whenever a new viewer connects so a decoder can begin without waiting for the camera's periodic
 * keyframe. Safe to call at any time; a no-op until media is flowing.
 *
 * @param client the client (may be NULL)
 */
void cameraWebrtcClientRequestKeyframe(CameraWebrtcClient *client);

/**
 * Stop and destroy the client, releasing all resources.
 *
 * @param client the client (may be NULL)
 */
void cameraWebrtcClientDestroy(CameraWebrtcClient *client);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(CameraWebrtcClient, cameraWebrtcClientDestroy)
