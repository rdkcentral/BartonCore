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

//
// Coordinator for the camera stream (cs) command. Handles the reference-app
// user interface and drives the streaming state machine, delegating the actual
// work to these modules:
//   - cameraDeviceSession: camera flows over the Barton client interface
//   - cameraWebrtcClient:  the WebRTC client backed by GStreamer (muxes to file / fMP4)
//   - cameraMediaServer:   HTTP server that streams the fragmented MP4 to a browser
//
// This module owns only the cross-thread synchronization (the modules invoke
// callbacks from their own threads) and the ordering of the signaling steps.
//

#include "cameraCategory.h"
#include "barton-core-client.h"
#include "barton-core-reference-io.h"
#include "cameraDeviceSession.h"
#include "cameraMediaServer.h"
#include "cameraWebrtcClient.h"
#include <jsonHelper/jsonHelper.h>
#include <signal.h>
#include <stdio.h>

// Default HTTP media server bind + port when `--out` is omitted. Forwarded to the host by
// the dev container (see .devcontainer/devcontainer.json forwardPorts).
#define CAMERA_DEFAULT_SERVE_HOST "127.0.0.1"
#define CAMERA_DEFAULT_SERVE_PORT 8088

// ============================================================================
// Shared state between threads
// ============================================================================

typedef struct
{
    GMutex mutex;
    GCond cond;

    // Set by the WebRTC client callbacks
    gchar *localSdpOffer;
    gboolean offerReady;
    GQueue localIceCandidates; // candidate strings buffered until trickle sending is enabled

    // Set by the device-session callbacks
    gchar *remoteSdp;
    GQueue remoteIceCandidates; // JSON array strings buffered until inbound trickle feeding is enabled
    gboolean remoteSdpReady;
    gboolean sessionEnded;
    gchar *errorMessage;

    // Control
    gboolean teardown;

    // Trickle ICE (outbound): once the camera session is established, local candidates are sent
    // to the camera immediately as they are gathered; until then they are buffered above.
    CameraDeviceSession *session;
    gboolean iceSendEnabled;

    // Trickle ICE (inbound): once the remote SDP is set, the camera's candidates are fed to the
    // WebRTC client as they arrive; until then they are buffered above.
    CameraWebrtcClient *webrtc;
    gboolean remoteIceFeedEnabled;

    // Serve mode: the in-container pipeline muxes fragmented MP4 and this HTTP server streams
    // it to the browser. NULL when recording to a file.
    CameraMediaServer *mediaServer;

    // Record mode: the muxed fragmented MP4 is written here. NULL when serving over HTTP.
    FILE *outFile;
} CameraStreamState;

static volatile CameraStreamState *sigintState = NULL;

static void sendLocalIceCandidate(CameraDeviceSession *session, const gchar *candidate);
static void feedRemoteIceCandidates(CameraWebrtcClient *client, const gchar *jsonCandidates);
static void onMediaBuffer(const guint8 *data, gsize size, gboolean isHeader, gpointer userData);
static void onViewerConnected(gpointer userData);
static gboolean parseOutputUri(const gchar *uri, gchar **filePathOut, gchar **serveHostOut, guint16 *servePortOut);

// ============================================================================
// SIGINT handler
// ============================================================================

static void sigintHandler(int sig)
{
    (void) sig;
    volatile CameraStreamState *s = sigintState;

    if (s != NULL)
    {
        CameraStreamState *state = (CameraStreamState *) s;
        g_mutex_lock(&state->mutex);
        state->teardown = TRUE;
        g_cond_signal(&state->cond);
        g_mutex_unlock(&state->mutex);
    }
}

// ============================================================================
// WebRTC client callbacks
// ============================================================================

static void onLocalOfferReady(const gchar *sdp, gpointer userData)
{
    CameraStreamState *state = (CameraStreamState *) userData;
    g_mutex_lock(&state->mutex);
    g_free(state->localSdpOffer);
    state->localSdpOffer = g_strdup(sdp);
    state->offerReady = TRUE;
    g_cond_signal(&state->cond);
    g_mutex_unlock(&state->mutex);
}

static void onLocalIceCandidate(guint mlineIndex, const gchar *candidate, gpointer userData)
{
    (void) mlineIndex;
    CameraStreamState *state = (CameraStreamState *) userData;

    g_mutex_lock(&state->mutex);
    gboolean sendNow = state->iceSendEnabled;

    if (!sendNow)
    {
        // The camera session isn't established yet; buffer until trickle sending is enabled.
        g_queue_push_tail(&state->localIceCandidates, g_strdup(candidate));
        g_cond_signal(&state->cond);
    }

    CameraDeviceSession *session = state->session;
    g_mutex_unlock(&state->mutex);

    // Trickle: send this candidate to the camera immediately as its own message.
    if (sendNow)
    {
        sendLocalIceCandidate(session, candidate);
    }
}

// Invoked when the WebRTC client stops on its own (render window closed or EOS).
// Requests the same graceful teardown as Ctrl+C so the session is ended on the
// camera (EndSession) and the stream is deallocated rather than leaked.
static void onWebrtcClosed(gpointer userData)
{
    CameraStreamState *state = (CameraStreamState *) userData;
    g_mutex_lock(&state->mutex);
    state->teardown = TRUE;
    g_cond_signal(&state->cond);
    g_mutex_unlock(&state->mutex);
}

// ============================================================================
// Device-session callbacks
// ============================================================================

static void onRemoteSdp(const gchar *sdp, gpointer userData)
{
    CameraStreamState *state = (CameraStreamState *) userData;
    g_mutex_lock(&state->mutex);
    g_free(state->remoteSdp);
    state->remoteSdp = g_strdup(sdp);
    state->remoteSdpReady = TRUE;
    g_cond_signal(&state->cond);
    g_mutex_unlock(&state->mutex);
}

static void onRemoteIce(const gchar *jsonCandidates, gpointer userData)
{
    CameraStreamState *state = (CameraStreamState *) userData;

    g_mutex_lock(&state->mutex);
    gboolean feedNow = state->remoteIceFeedEnabled;
    CameraWebrtcClient *webrtc = state->webrtc;

    if (!feedNow)
    {
        // The remote SDP isn't set yet; buffer until inbound trickle feeding is enabled.
        g_queue_push_tail(&state->remoteIceCandidates, g_strdup(jsonCandidates));
        g_cond_signal(&state->cond);
    }

    g_mutex_unlock(&state->mutex);

    // Trickle: feed these candidates to the WebRTC client the moment they arrive.
    if (feedNow)
    {
        feedRemoteIceCandidates(webrtc, jsonCandidates);
    }
}

static void onSessionError(const gchar *message, gpointer userData)
{
    CameraStreamState *state = (CameraStreamState *) userData;
    g_mutex_lock(&state->mutex);
    g_free(state->errorMessage);
    state->errorMessage = g_strdup(message);
    state->sessionEnded = TRUE;
    g_cond_signal(&state->cond);
    g_mutex_unlock(&state->mutex);
}

// ============================================================================
// Helpers
// ============================================================================

static gboolean waitForCondition(CameraStreamState *state, gboolean *flag, gint timeoutSeconds)
{
    gint64 deadline = g_get_monotonic_time() + (gint64) timeoutSeconds * G_USEC_PER_SEC;
    g_mutex_lock(&state->mutex);

    while (!*flag && !state->teardown && !state->sessionEnded)
    {
        if (!g_cond_wait_until(&state->cond, &state->mutex, deadline))
        {
            // Timeout
            g_mutex_unlock(&state->mutex);

            return FALSE;
        }
    }

    gboolean result = *flag;
    g_mutex_unlock(&state->mutex);

    return result;
}

// Block until Ctrl+C (teardown) or the camera ends the session.

// Wrap a single ICE candidate in the one-element JSON array the camera expects, so each
// candidate can be trickled to the camera as its own message.
static gchar *buildSingleIceCandidateJson(const gchar *candidate)
{
    scoped_cJSON *array = cJSON_CreateArray();
    cJSON_AddItemToArray(array, cJSON_CreateString(candidate));

    // cJSON prints into a malloc'd buffer; copy it into a glib buffer (freed with
    // cJSON_free) so the caller can free the result with g_free.
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

// Send any candidates that were gathered before trickle sending was enabled, one message each.
static void flushBufferedLocalIce(CameraDeviceSession *session, CameraStreamState *state)
{
    for (;;)
    {
        g_mutex_lock(&state->mutex);
        gchar *candidate = g_queue_pop_head(&state->localIceCandidates);
        g_mutex_unlock(&state->mutex);

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

        // GStreamer's webrtcbin expects the bare "candidate:..." value; the camera
        // sends candidates with the SDP "a=" attribute prefix.
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

// Feed any remote candidates that arrived before inbound trickle feeding was enabled, then the
// callback delivers the rest live for the remainder of the session.
static void flushBufferedRemoteIce(CameraWebrtcClient *client, CameraStreamState *state)
{
    for (;;)
    {
        g_mutex_lock(&state->mutex);
        gchar *json = g_queue_pop_head(&state->remoteIceCandidates);
        g_mutex_unlock(&state->mutex);

        if (json == NULL)
        {
            break;
        }

        g_autofree gchar *owned = json;
        feedRemoteIceCandidates(client, owned);
    }
}

static void cleanupState(CameraStreamState *state)
{
    g_free(state->localSdpOffer);
    g_free(state->remoteSdp);
    g_free(state->errorMessage);

    while (!g_queue_is_empty(&state->localIceCandidates))
    {
        g_free(g_queue_pop_head(&state->localIceCandidates));
    }

    while (!g_queue_is_empty(&state->remoteIceCandidates))
    {
        g_free(g_queue_pop_head(&state->remoteIceCandidates));
    }

    if (state->outFile != NULL)
    {
        fclose(state->outFile);
        state->outFile = NULL;
    }

    g_mutex_clear(&state->mutex);
    g_cond_clear(&state->cond);
}

// Pretty-print the negotiated stream configuration. Fields the pipeline hasn't reported
// yet are omitted (resolution shows a placeholder until the first frame decodes).
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
// Command implementation
// ============================================================================

// Runs the full signaling sequence for one stream. Each module handle is scope
// bound (g_autoptr), so every early return tears everything down in the right
// order: the WebRTC client first (stopping the sink), then the window it drew
// into, then the device session (which ends the camera session if it was opened).
static bool runCameraStream(BCoreClient *client,
                            const gchar *deviceId,
                            const gchar *filePath,
                            const gchar *serveHost,
                            guint16 servePort,
                            CameraStreamState *state)
{
    // Subscribe to camera events before anything else so no update is missed.
    g_autoptr(CameraDeviceSession) session =
        cameraDeviceSessionCreate(client, deviceId, onRemoteSdp, onRemoteIce, onSessionError, state);

    if (session == NULL)
    {
        emitError("[camera-stream] Failed to set up camera session\n");

        return false;
    }

    // Make the session available to the ICE-candidate callback so it can trickle candidates.
    g_mutex_lock(&state->mutex);
    state->session = session;
    g_mutex_unlock(&state->mutex);

    // Step 1: Create session
    emitOutput("[camera-stream] Creating session...\n");

    if (!cameraDeviceSessionOpen(session))
    {
        emitError("[camera-stream] Failed to create session\n");

        return false;
    }

    emitOutput("[camera-stream] Session created\n");

    // Step 2: Start stream
    emitOutput("[camera-stream] Starting stream...\n");

    if (!cameraDeviceSessionStartStream(session))
    {
        emitError("[camera-stream] Failed to start stream\n");

        return false;
    }

    const gchar *protocol = cameraDeviceSessionGetProtocol(session);
    const gchar *entryPoint = cameraDeviceSessionGetEntryPoint(session);
    emitOutput("[camera-stream] Stream started (protocol %s, entry %s), setting up WebRTC...\n",
               protocol != NULL ? protocol : "unknown",
               entryPoint != NULL ? entryPoint : "unknown");

    // Step 3: In serve mode, start the HTTP media server before the client so it receives the
    // pipeline's first (init-segment) buffers. In record mode, open the output file. Then create
    // the WebRTC client, which always muxes the camera's H.264 into fragmented MP4 and delivers
    // the buffers to onMediaBuffer. The client is declared after the server so it is destroyed
    // first, stopping the pipeline before the server it feeds.
    g_autoptr(CameraMediaServer) mediaServer = NULL;

    if (filePath != NULL)
    {
        state->outFile = fopen(filePath, "wb");

        if (state->outFile == NULL)
        {
            emitError("[camera-stream] Failed to open %s for writing\n", filePath);

            return false;
        }

        emitOutput("[camera-stream] Recording camera video to %s\n", filePath);
    }
    else
    {
        mediaServer = cameraMediaServerCreate(serveHost, servePort);

        if (mediaServer == NULL)
        {
            emitError("[camera-stream] Failed to start the media server\n");

            return false;
        }

        g_mutex_lock(&state->mutex);
        state->mediaServer = mediaServer;
        g_mutex_unlock(&state->mutex);

        cameraMediaServerSetOnViewer(mediaServer, onViewerConnected, state);

        emitOutput("[camera-stream] Serving camera video at %s\n", cameraMediaServerGetUrl(mediaServer));
    }

    g_autoptr(CameraWebrtcClient) webrtc =
        cameraWebrtcClientCreate(onLocalOfferReady, onLocalIceCandidate, onWebrtcClosed, onMediaBuffer, state);

    if (webrtc == NULL)
    {
        emitError("[camera-stream] Failed to create WebRTC client\n");

        return false;
    }

    // Determine our negotiation role from the driver's stream result. In answerer mode
    // (SolicitOffer flow) the camera provides the offer and we answer it; otherwise (offerer /
    // ProvideOffer flow) we create the offer. Must be set before the peer starts negotiating.
    const gchar *role = cameraDeviceSessionGetRole(session);
    gboolean answerer = (g_strcmp0(role, "answerer") == 0);
    cameraWebrtcClientSetAnswerer(webrtc, answerer);

    // Step 4: Start client (in offerer mode this triggers negotiation -> creates the SDP offer)
    if (!cameraWebrtcClientStart(webrtc))
    {
        emitError("[camera-stream] Failed to start WebRTC client\n");

        return false;
    }

    if (answerer)
    {
        // Answerer (SolicitOffer flow): open signaling by posting an empty local SDP, which tells
        // the driver to allocate a stream and ask the camera to generate the offer. Then wait for
        // that offer, set it as the remote description (which makes the client create our answer),
        // and send the answer back.
        emitOutput("[camera-stream] Requesting camera SDP offer...\n");

        if (!cameraDeviceSessionSendOffer(session, ""))
        {
            emitError("[camera-stream] Failed to request camera SDP offer\n");

            return false;
        }

        emitOutput("[camera-stream] Waiting for camera SDP offer...\n");

        if (!waitForCondition(state, &state->remoteSdpReady, 30))
        {
            emitError("[camera-stream] Timeout waiting for camera SDP offer\n");

            return false;
        }

        if (state->teardown || state->sessionEnded)
        {
            return false;
        }

        g_autofree gchar *remoteOffer = NULL;
        g_mutex_lock(&state->mutex);
        remoteOffer = g_strdup(state->remoteSdp);
        g_mutex_unlock(&state->mutex);

        emitOutput("[camera-stream] Offer received, creating answer...\n");

        if (!cameraWebrtcClientSetRemoteSdp(webrtc, remoteOffer))
        {
            emitError("[camera-stream] Failed to set remote SDP offer\n");

            return false;
        }

        // The client produces the answer asynchronously (create-answer).
        if (!waitForCondition(state, &state->offerReady, 10))
        {
            emitError("[camera-stream] Timeout waiting for local SDP answer\n");

            return false;
        }

        if (state->teardown || state->sessionEnded)
        {
            return false;
        }

        g_autofree gchar *localAnswer = NULL;
        g_mutex_lock(&state->mutex);
        localAnswer = g_strdup(state->localSdpOffer);
        g_mutex_unlock(&state->mutex);

        emitOutput("[camera-stream] Sending SDP answer to camera...\n");

        if (!cameraDeviceSessionSendOffer(session, localAnswer))
        {
            emitError("[camera-stream] Failed to send SDP answer\n");

            return false;
        }
    }
    else
    {
        // Offerer (ProvideOffer flow): we create the offer, the camera answers.
        emitOutput("[camera-stream] Waiting for local SDP offer...\n");

        if (!waitForCondition(state, &state->offerReady, 10))
        {
            emitError("[camera-stream] Timeout waiting for SDP offer\n");

            return false;
        }

        if (state->teardown || state->sessionEnded)
        {
            return false;
        }

        g_autofree gchar *localOffer = NULL;
        g_mutex_lock(&state->mutex);
        localOffer = g_strdup(state->localSdpOffer);
        g_mutex_unlock(&state->mutex);

        emitOutput("[camera-stream] Sending SDP offer to camera...\n");

        if (!cameraDeviceSessionSendOffer(session, localOffer))
        {
            emitError("[camera-stream] Failed to send SDP offer\n");

            return false;
        }

        emitOutput("[camera-stream] SDP offer sent, waiting for answer...\n");

        if (!waitForCondition(state, &state->remoteSdpReady, 30))
        {
            emitError("[camera-stream] Timeout waiting for remote SDP answer\n");

            return false;
        }

        if (state->teardown || state->sessionEnded)
        {
            return false;
        }

        g_autofree gchar *remoteAnswer = NULL;
        g_mutex_lock(&state->mutex);
        remoteAnswer = g_strdup(state->remoteSdp);
        g_mutex_unlock(&state->mutex);

        emitOutput("[camera-stream] Answer received, setting remote SDP...\n");

        if (!cameraWebrtcClientSetRemoteSdp(webrtc, remoteAnswer))
        {
            emitError("[camera-stream] Failed to set remote SDP\n");

            return false;
        }
    }

    // Step 9: Enable trickle ICE in both directions now that the camera session is established
    // and the remote description is set. Candidates gathered/received before now are flushed
    // here; later ones are trickled by the callbacks the moment they arrive, for the life of
    // the session.
    emitOutput("[camera-stream] Trickling ICE candidates...\n");
    g_mutex_lock(&state->mutex);
    state->iceSendEnabled = TRUE;
    state->webrtc = webrtc;
    state->remoteIceFeedEnabled = TRUE;
    g_mutex_unlock(&state->mutex);

    // Outbound: send local candidates to the camera. Inbound: feed the camera's candidates to
    // the WebRTC client. Both drain whatever was buffered during signaling; the callbacks then
    // keep trickling in each direction until the session ends.
    flushBufferedLocalIce(session, state);
    flushBufferedRemoteIce(webrtc, state);

    // Print the signaled stream configuration (codec / bitrate / payload). Resolution and
    // frame rate are only known once frames decode, which the client announces separately.
    {
        CameraWebrtcVideoConfig videoConfig;
        cameraWebrtcClientGetVideoConfig(webrtc, &videoConfig);
        printNegotiatedConfig(&videoConfig);
    }

    // Step 12: Media flowing - wait for teardown
    if (filePath != NULL)
    {
        emitOutput("[camera-stream] Media flowing, recording to %s. Press Ctrl+C to stop.\n", filePath);
    }
    else
    {
        emitOutput("[camera-stream] Media flowing at %s. Press Ctrl+C to stop.\n",
                   cameraMediaServerGetUrl(mediaServer));
    }

    waitForCondition(state, &state->teardown, 3600);

    if (state->sessionEnded)
    {
        emitOutput("[camera-stream] Session ended: %s\n",
                   state->errorMessage != NULL ? state->errorMessage : "unknown reason");
    }
    else
    {
        emitOutput("\n[camera-stream] Stopping stream...\n");
    }

    return true;
}

// SERVE mode: hand each muxed fragmented-MP4 buffer from the pipeline to the HTTP media server.
static void onMediaBuffer(const guint8 *data, gsize size, gboolean isHeader, gpointer userData)
{
    CameraStreamState *state = (CameraStreamState *) userData;

    g_mutex_lock(&state->mutex);
    CameraMediaServer *server = state->mediaServer;
    FILE *outFile = state->outFile;
    g_mutex_unlock(&state->mutex);

    // Serve mode: fan the fragmented-MP4 buffer out to viewers. Record mode: append it to the
    // file. (outFile is only closed in cleanupState, after the pipeline -- and therefore this
    // callback -- has stopped, so the pointer stays valid for the write here.)
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

// Invoked (on the media server thread) when a new viewer connects. Requests a fresh keyframe so
// the viewer can begin decoding immediately. state->webrtc may still be NULL if a viewer connects
// during setup, before the client is published; in that case the stream-start keyframe covers it.
static void onViewerConnected(gpointer userData)
{
    CameraStreamState *state = (CameraStreamState *) userData;

    g_mutex_lock(&state->mutex);
    CameraWebrtcClient *webrtc = state->webrtc;
    g_mutex_unlock(&state->mutex);

    if (webrtc != NULL)
    {
        cameraWebrtcClientRequestKeyframe(webrtc);
    }
}

// Parse the --out URI. A "file://<path>" URI records to a file; anything else is treated as an
// HTTP serve target (the "http://" scheme is optional). A NULL uri (no --out) selects the default
// serve target. filePathOut stays NULL for serve mode; the allocated path/host strings are
// returned via the *Out parameters.
static gboolean parseOutputUri(const gchar *uri, gchar **filePathOut, gchar **serveHostOut, guint16 *servePortOut)
{
    if (uri == NULL)
    {
        return TRUE; // default: serve
    }

    if (g_str_has_prefix(uri, "file://"))
    {
        *filePathOut = g_strdup(uri + strlen("file://"));

        return (*filePathOut)[0] != '\0';
    }

    // Anything that is not a file:// URI is treated as an HTTP serve target. The "http://"
    // scheme is optional; the remainder is parsed as <host>[:<port>].
    const gchar *authorityStart = g_str_has_prefix(uri, "http://") ? uri + strlen("http://") : uri;
    g_autofree gchar *authority = g_strdup(authorityStart);
    gchar *slash = strchr(authority, '/');

    if (slash != NULL)
    {
        *slash = '\0';
    }

    gchar *colon = strrchr(authority, ':');

    if (colon != NULL)
    {
        *colon = '\0';
        *servePortOut = (guint16) g_ascii_strtoull(colon + 1, NULL, 10);
    }

    *serveHostOut = (authority[0] != '\0') ? g_strdup(authority) : g_strdup(CAMERA_DEFAULT_SERVE_HOST);

    return (*servePortOut != 0);
}

static bool cameraStreamFunc(BCoreClient *client, gint argc, gchar **argv)
{
    const gchar *deviceId = argv[0];
    const gchar *outUri = NULL;

    // Parse optional --out <uri>: file://<path> records, http://<host>:<port> serves.
    for (gint i = 1; i < argc; i++)
    {
        if (g_strcmp0(argv[i], "--out") == 0 && i + 1 < argc)
        {
            outUri = argv[i + 1];
            i++;
        }
    }

    g_autofree gchar *filePath = NULL;
    g_autofree gchar *serveHost = NULL;
    guint16 servePort = CAMERA_DEFAULT_SERVE_PORT;

    if (!parseOutputUri(outUri, &filePath, &serveHost, &servePort))
    {
        emitError("Invalid --out URI '%s' (expected file://<path> or http://<host>:<port>)\n",
                  outUri != NULL ? outUri : "");

        return false;
    }

    // Verify device exists
    g_autoptr(BCoreDevice) device = b_core_client_get_device_by_id(client, deviceId);

    if (device == NULL)
    {
        emitError("Device '%s' not found\n", deviceId);

        return false;
    }

    // Initialize shared state
    CameraStreamState state = {0};
    g_mutex_init(&state.mutex);
    g_cond_init(&state.cond);
    g_queue_init(&state.localIceCandidates);
    g_queue_init(&state.remoteIceCandidates);

    // Install signal handler so Ctrl+C requests a graceful teardown
    struct sigaction oldAction;
    struct sigaction newAction = {0};
    newAction.sa_handler = sigintHandler;
    sigemptyset(&newAction.sa_mask);
    sigintState = &state;
    sigaction(SIGINT, &newAction, &oldAction);

    bool success = runCameraStream(
        client, deviceId, filePath, serveHost != NULL ? serveHost : CAMERA_DEFAULT_SERVE_HOST, servePort, &state);

    // Restore signal handler
    sigaction(SIGINT, &oldAction, NULL);
    sigintState = NULL;

    cleanupState(&state);

    emitOutput("[camera-stream] Done.\n");

    return success;
}

Category *buildCameraCategory(void)
{
    Category *cat = categoryCreate("Camera", "Camera streaming commands");

    Command *command = commandCreate("cameraStream",
                                     "cs",
                                     "<deviceId> [--out <uri>]",
                                     "Stream a camera via WebRTC; serve over HTTP (default http://127.0.0.1:8088) "
                                     "or record with --out file://<path>",
                                     1,
                                     3,
                                     cameraStreamFunc);
    categoryAddCommand(cat, command);

    return cat;
}
