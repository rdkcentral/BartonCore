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

#include "cameraWebrtcBackend.h"
#include "barton-core-reference-io.h"
#include "cameraWebrtcClient.h"
#include <jsonHelper/jsonHelper.h>

typedef struct
{
    CameraStreamBackend base; // must be first so a CameraStreamBackend* aliases this struct
    CameraStreamContext *ctx; // set for the duration of run()

    // Local SDP offer/answer produced by the WebRTC client.
    gchar *localSdpOffer;
    gboolean offerReady;
    GQueue localIceCandidates; // candidate strings buffered until trickle sending is enabled

    // Remote SDP answer/offer and ICE candidates delivered by the camera via device-session events.
    gchar *remoteSdp;
    gboolean remoteSdpReady;
    GQueue remoteIceCandidates; // JSON array strings buffered until inbound trickle feeding is enabled

    // Trickle ICE gates: once the session is established and the remote description is set,
    // candidates flow immediately in both directions; until then they are buffered above.
    gboolean iceSendEnabled;
    gboolean remoteIceFeedEnabled;

    // Access pointer for callbacks; owned by run()'s g_autoptr and cleared (under the context lock)
    // before that scope frees it, so a late callback sees NULL rather than a freed client.
    CameraWebrtcClient *webrtc;
} CameraWebrtcBackend;

// ============================================================================
// Helpers
// ============================================================================

// Wrap a single ICE candidate in the one-element JSON array the camera expects, so each candidate
// can be trickled to the camera as its own message.
static gchar *buildSingleIceCandidateJson(const gchar *candidate)
{
    scoped_cJSON *array = cJSON_CreateArray();
    cJSON_AddItemToArray(array, cJSON_CreateString(candidate));

    // cJSON prints into a malloc'd buffer; copy it into a glib buffer (freed with cJSON_free) so
    // the caller can free the result with g_free.
    char *raw = cJSON_PrintUnformatted(array);
    gchar *result = g_strdup(raw);
    cJSON_free(raw);

    return result;
}

// Send one local ICE candidate to the camera as its own offerIceCandidates message.
static void sendLocalIceCandidate(CameraDeviceSession *session, const gchar *candidate)
{
    g_autofree gchar *json = buildSingleIceCandidateJson(candidate);
    cameraDeviceSessionSendIceCandidates(session, json);
}

// Send any candidates gathered before trickle sending was enabled, one message each.
static void flushBufferedLocalIce(CameraWebrtcBackend *self, CameraDeviceSession *session)
{
    for (;;)
    {
        cameraStreamContextLock(self->ctx);
        gchar *candidate = g_queue_pop_head(&self->localIceCandidates);
        cameraStreamContextUnlock(self->ctx);

        if (candidate == NULL)
        {
            break;
        }

        g_autofree gchar *owned = candidate;
        sendLocalIceCandidate(session, owned);
    }
}

// Parse the camera's remote ICE JSON array and feed each candidate to the client.
static void feedRemoteIceCandidates(CameraWebrtcClient *client, const gchar *jsonCandidates)
{
    scoped_cJSON *array = cJSON_Parse(jsonCandidates);

    if (!cJSON_IsArray(array))
    {
        return;
    }

    cJSON *item = NULL;
    cJSON_ArrayForEach(item, array)
    {
        if (!cJSON_IsString(item))
        {
            continue;
        }

        // GStreamer's webrtcbin expects the bare "candidate:..." value; the camera sends
        // candidates with the SDP "a=" attribute prefix.
        const gchar *value = item->valuestring;

        if (g_str_has_prefix(value, "a="))
        {
            value += 2;
        }

        // Skip the empty end-of-candidates marker the camera includes.
        if (*value != '\0')
        {
            cameraWebrtcClientAddIceCandidate(client, 0, value);
        }
    }
}

// Feed any remote candidates that arrived before inbound trickle feeding was enabled; the callback
// delivers the rest live for the remainder of the session.
static void flushBufferedRemoteIce(CameraWebrtcBackend *self, CameraWebrtcClient *client)
{
    for (;;)
    {
        cameraStreamContextLock(self->ctx);
        gchar *json = g_queue_pop_head(&self->remoteIceCandidates);
        cameraStreamContextUnlock(self->ctx);

        if (json == NULL)
        {
            break;
        }

        g_autofree gchar *owned = json;
        feedRemoteIceCandidates(client, owned);
    }
}

// Pretty-print the negotiated stream configuration. Only fields the pipeline can report are shown;
// this is a passthrough (no-decode) pipeline, so resolution and frame rate are omitted.
static void printNegotiatedConfig(const CameraWebrtcVideoConfig *cfg)
{
    emitOutput("[camera-stream] Negotiated stream configuration:\n");

    if (cfg->codec[0] != '\0')
    {
        if (cfg->profileLevelId[0] != '\0')
        {
            emitOutput("[camera-stream]     Codec       : %s (profile-level-id %s)\n", cfg->codec, cfg->profileLevelId);
        }
        else
        {
            emitOutput("[camera-stream]     Codec       : %s\n", cfg->codec);
        }
    }

    if (cfg->bitrateKbps > 0)
    {
        emitOutput("[camera-stream]     Bit rate    : %d kbps\n", cfg->bitrateKbps);
    }

    if (cfg->payloadType >= 0 && cfg->clockRate > 0)
    {
        emitOutput("[camera-stream]     RTP payload : %d @ %d Hz\n", cfg->payloadType, cfg->clockRate);
    }

    if (cfg->width > 0 && cfg->height > 0)
    {
        emitOutput("[camera-stream]     Resolution  : %dx%d\n", cfg->width, cfg->height);
    }

    if (cfg->framerateNum > 0 && cfg->framerateDen > 0)
    {
        emitOutput("[camera-stream]     Frame rate  : %g fps\n",
                   (double) cfg->framerateNum / (double) cfg->framerateDen);
    }
}

// ============================================================================
// WebRTC client callbacks (userData is the backend)
// ============================================================================

static void onLocalOfferReady(const gchar *sdp, gpointer userData)
{
    CameraWebrtcBackend *self = (CameraWebrtcBackend *) userData;
    cameraStreamContextLock(self->ctx);
    g_free(self->localSdpOffer);
    self->localSdpOffer = g_strdup(sdp);
    self->offerReady = TRUE;
    cameraStreamContextWake(self->ctx);
    cameraStreamContextUnlock(self->ctx);
}

static void onLocalIceCandidate(guint mlineIndex, const gchar *candidate, gpointer userData)
{
    (void) mlineIndex;
    CameraWebrtcBackend *self = (CameraWebrtcBackend *) userData;

    cameraStreamContextLock(self->ctx);
    gboolean sendNow = self->iceSendEnabled;

    if (!sendNow)
    {
        // The camera session isn't established yet; buffer until trickle sending is enabled.
        g_queue_push_tail(&self->localIceCandidates, g_strdup(candidate));
        cameraStreamContextWake(self->ctx);
    }

    cameraStreamContextUnlock(self->ctx);

    // Trickle: send this candidate to the camera immediately as its own message.
    if (sendNow)
    {
        sendLocalIceCandidate(cameraStreamContextGetSession(self->ctx), candidate);
    }
}

// The WebRTC client stopped on its own (pipeline error or EOS): request the same graceful teardown
// as Ctrl+C so the session is ended on the camera rather than leaked.
static void onWebrtcClosed(gpointer userData)
{
    CameraWebrtcBackend *self = (CameraWebrtcBackend *) userData;
    cameraStreamContextRequestTeardown(self->ctx);
}

static void onWebrtcBuffer(const guint8 *data, gsize size, gboolean isHeader, gpointer userData)
{
    CameraWebrtcBackend *self = (CameraWebrtcBackend *) userData;
    cameraStreamContextPushBuffer(self->ctx, data, size, isHeader);
}

// A new viewer connected: request a fresh keyframe so it can begin decoding immediately. Reads the
// client under the context lock so a concurrent teardown (which clears self->webrtc under the same
// lock) cannot race with the request.
static void onViewerConnected(gpointer userData)
{
    CameraWebrtcBackend *self = (CameraWebrtcBackend *) userData;

    cameraStreamContextLock(self->ctx);

    if (self->webrtc != NULL)
    {
        cameraWebrtcClientRequestKeyframe(self->webrtc);
    }

    cameraStreamContextUnlock(self->ctx);
}

// ============================================================================
// Device-session callbacks (userData is the backend)
// ============================================================================

static void onRemoteSdp(const gchar *sdp, gpointer userData)
{
    CameraWebrtcBackend *self = (CameraWebrtcBackend *) userData;
    cameraStreamContextLock(self->ctx);
    g_free(self->remoteSdp);
    self->remoteSdp = g_strdup(sdp);
    self->remoteSdpReady = TRUE;
    cameraStreamContextWake(self->ctx);
    cameraStreamContextUnlock(self->ctx);
}

static void onRemoteIce(const gchar *jsonCandidates, gpointer userData)
{
    CameraWebrtcBackend *self = (CameraWebrtcBackend *) userData;

    cameraStreamContextLock(self->ctx);
    gboolean feedNow = self->remoteIceFeedEnabled;
    CameraWebrtcClient *webrtc = self->webrtc;

    if (!feedNow)
    {
        // The remote SDP isn't set yet; buffer until inbound trickle feeding is enabled.
        g_queue_push_tail(&self->remoteIceCandidates, g_strdup(jsonCandidates));
        cameraStreamContextWake(self->ctx);
    }

    cameraStreamContextUnlock(self->ctx);

    // Trickle: feed these candidates to the WebRTC client the moment they arrive.
    if (feedNow)
    {
        feedRemoteIceCandidates(webrtc, jsonCandidates);
    }
}

// ============================================================================
// Signaling
// ============================================================================

// Answerer (SolicitOffer flow): request the camera's offer, set it as the remote description
// (which makes the client create our answer), and send the answer back.
static bool runAnswererSignaling(CameraWebrtcBackend *self,
                                 CameraStreamContext *ctx,
                                 CameraDeviceSession *session,
                                 CameraWebrtcClient *webrtc)
{
    emitOutput("[camera-stream] Requesting camera SDP offer...\n");

    if (!cameraDeviceSessionSendOffer(session, ""))
    {
        emitError("[camera-stream] Failed to request camera SDP offer\n");

        return false;
    }

    emitOutput("[camera-stream] Waiting for camera SDP offer...\n");

    if (!cameraStreamContextWaitFlag(ctx, &self->remoteSdpReady, 30))
    {
        emitError("[camera-stream] Timeout waiting for camera SDP offer\n");

        return false;
    }

    if (cameraStreamContextTornDown(ctx))
    {
        return false;
    }

    g_autofree gchar *remoteOffer = NULL;
    cameraStreamContextLock(ctx);
    remoteOffer = g_strdup(self->remoteSdp);
    cameraStreamContextUnlock(ctx);

    emitOutput("[camera-stream] Offer received, creating answer...\n");

    if (!cameraWebrtcClientSetRemoteSdp(webrtc, remoteOffer))
    {
        emitError("[camera-stream] Failed to set remote SDP offer\n");

        return false;
    }

    // The client produces the answer asynchronously (create-answer).
    if (!cameraStreamContextWaitFlag(ctx, &self->offerReady, 10))
    {
        emitError("[camera-stream] Timeout waiting for local SDP answer\n");

        return false;
    }

    if (cameraStreamContextTornDown(ctx))
    {
        return false;
    }

    g_autofree gchar *localAnswer = NULL;
    cameraStreamContextLock(ctx);
    localAnswer = g_strdup(self->localSdpOffer);
    cameraStreamContextUnlock(ctx);

    emitOutput("[camera-stream] Sending SDP answer to camera...\n");

    if (!cameraDeviceSessionSendOffer(session, localAnswer))
    {
        emitError("[camera-stream] Failed to send SDP answer\n");

        return false;
    }

    return true;
}

// Offerer (ProvideOffer flow): we create the offer and the camera answers.
static bool runOffererSignaling(CameraWebrtcBackend *self,
                                CameraStreamContext *ctx,
                                CameraDeviceSession *session,
                                CameraWebrtcClient *webrtc)
{
    emitOutput("[camera-stream] Waiting for local SDP offer...\n");

    if (!cameraStreamContextWaitFlag(ctx, &self->offerReady, 10))
    {
        emitError("[camera-stream] Timeout waiting for SDP offer\n");

        return false;
    }

    if (cameraStreamContextTornDown(ctx))
    {
        return false;
    }

    g_autofree gchar *localOffer = NULL;
    cameraStreamContextLock(ctx);
    localOffer = g_strdup(self->localSdpOffer);
    cameraStreamContextUnlock(ctx);

    emitOutput("[camera-stream] Sending SDP offer to camera...\n");

    if (!cameraDeviceSessionSendOffer(session, localOffer))
    {
        emitError("[camera-stream] Failed to send SDP offer\n");

        return false;
    }

    emitOutput("[camera-stream] SDP offer sent, waiting for answer...\n");

    if (!cameraStreamContextWaitFlag(ctx, &self->remoteSdpReady, 30))
    {
        emitError("[camera-stream] Timeout waiting for remote SDP answer\n");

        return false;
    }

    if (cameraStreamContextTornDown(ctx))
    {
        return false;
    }

    g_autofree gchar *remoteAnswer = NULL;
    cameraStreamContextLock(ctx);
    remoteAnswer = g_strdup(self->remoteSdp);
    cameraStreamContextUnlock(ctx);

    emitOutput("[camera-stream] Answer received, setting remote SDP...\n");

    if (!cameraWebrtcClientSetRemoteSdp(webrtc, remoteAnswer))
    {
        emitError("[camera-stream] Failed to set remote SDP\n");

        return false;
    }

    return true;
}

// ============================================================================
// Backend vfuncs
// ============================================================================

static bool webrtcRun(CameraStreamBackend *base, CameraStreamContext *ctx)
{
    CameraWebrtcBackend *self = (CameraWebrtcBackend *) base;
    self->ctx = ctx;
    CameraDeviceSession *session = cameraStreamContextGetSession(ctx);

    // Deliver the camera's remote SDP and ICE candidates to this backend.
    cameraDeviceSessionSetWebrtcCallbacks(session, onRemoteSdp, onRemoteIce, self);

    emitOutput("[camera-stream] Setting up WebRTC...\n");

    // Start the sink before the client so the HTTP server receives the pipeline's first
    // (init-segment) buffers; the viewer handler requests keyframes for newly-connected browsers.
    if (!cameraStreamContextStartSink(ctx, onViewerConnected, self))
    {
        return false;
    }

    g_autoptr(CameraWebrtcClient) webrtc =
        cameraWebrtcClientCreate(onLocalOfferReady, onLocalIceCandidate, onWebrtcClosed, onWebrtcBuffer, self);

    if (webrtc == NULL)
    {
        emitError("[camera-stream] Failed to create WebRTC client\n");

        return false;
    }

    // Determine our negotiation role by inverting the camera's. negotiationRole reports the
    // CAMERA's role: when the camera is the 'offerer' it provides the offer and we answer it; when
    // the camera is the 'answerer' we create the offer. Must be set before the peer negotiates.
    const gchar *role = cameraDeviceSessionGetRole(session);

    if (g_strcmp0(role, "offerer") != 0 && g_strcmp0(role, "answerer") != 0)
    {
        emitError("[camera-stream] could not determine the camera's negotiation role (got '%s')\n",
                  role != NULL ? role : "(null)");

        return false;
    }

    gboolean answerer = (g_strcmp0(role, "offerer") == 0);
    cameraWebrtcClientSetAnswerer(webrtc, answerer);

    // Start the client (in offerer mode this triggers negotiation -> creates the SDP offer).
    if (!cameraWebrtcClientStart(webrtc))
    {
        emitError("[camera-stream] Failed to start WebRTC client\n");

        return false;
    }

    bool signaled =
        answerer ? runAnswererSignaling(self, ctx, session, webrtc) : runOffererSignaling(self, ctx, session, webrtc);

    if (!signaled)
    {
        return false;
    }

    // Enable trickle ICE in both directions now that the session is established and the remote
    // description is set. Candidates gathered/received earlier are flushed here; later ones are
    // trickled by the callbacks the moment they arrive.
    emitOutput("[camera-stream] Trickling ICE candidates...\n");
    cameraStreamContextLock(ctx);
    self->iceSendEnabled = TRUE;
    self->webrtc = webrtc;
    self->remoteIceFeedEnabled = TRUE;
    cameraStreamContextUnlock(ctx);

    flushBufferedLocalIce(self, session);
    flushBufferedRemoteIce(self, webrtc);

    // Print the signaled stream configuration (codec / bitrate / payload).
    {
        CameraWebrtcVideoConfig videoConfig;
        cameraWebrtcClientGetVideoConfig(webrtc, &videoConfig);
        printNegotiatedConfig(&videoConfig);
    }

    // Block until teardown, then sever the media server's callbacks. Clear the access pointer under
    // the lock so a late callback becomes a no-op before the g_autoptr scope frees the client.
    cameraStreamContextAwaitTeardown(ctx);

    cameraStreamContextLock(ctx);
    self->webrtc = NULL;
    cameraStreamContextUnlock(ctx);

    return true;
}

static void webrtcDestroy(CameraStreamBackend *base)
{
    CameraWebrtcBackend *self = (CameraWebrtcBackend *) base;

    g_free(self->localSdpOffer);
    g_free(self->remoteSdp);

    while (!g_queue_is_empty(&self->localIceCandidates))
    {
        g_free(g_queue_pop_head(&self->localIceCandidates));
    }

    while (!g_queue_is_empty(&self->remoteIceCandidates))
    {
        g_free(g_queue_pop_head(&self->remoteIceCandidates));
    }

    g_free(self);
}

CameraStreamBackend *cameraWebrtcBackendCreate(const CameraStreamOptions *options)
{
    (void) options; // WebRTC uses no command-line options in this reference app

    CameraWebrtcBackend *self = g_new0(CameraWebrtcBackend, 1);
    self->base.run = webrtcRun;
    self->base.destroy = webrtcDestroy;
    g_queue_init(&self->localIceCandidates);
    g_queue_init(&self->remoteIceCandidates);

    return &self->base;
}
