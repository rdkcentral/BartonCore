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

#include "cameraWebrtcClient.h"
#include "barton-core-reference-io.h"
#include <stdio.h>
#include <string.h>

#define GST_USE_UNSTABLE_API
#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#include <gst/sdp/sdp.h>
#include <gst/webrtc/webrtc.h>

// Bounded window for the peer connection to reach CONNECTED after the remote description
// is set. Matter signaling cannot observe media-plane connectivity, so this client-side
// deadline turns a stuck/failed ICE exchange into a reported teardown instead of a hang.
// Comfortably exceeds the observed successful connect time (a few seconds; well under the
// ~10s to first decoded frame).
#define CONNECTIVITY_TIMEOUT_SECONDS 20

struct _CameraWebrtcClient
{
    GstElement *pipeline;
    GstElement *webrtcbin;

    CameraWebrtcOnOfferReady onOffer;
    CameraWebrtcOnIceCandidate onIce;
    CameraWebrtcOnClosed onClosed;
    gpointer userData;

    // Negotiation role. In answerer mode (SolicitOffer flow) the camera provides the offer and we
    // generate the answer; otherwise (offerer / ProvideOffer flow) we create the offer. Set once
    // before cameraWebrtcClientStart(); read on the webrtcbin thread.
    gboolean answerer;

    // Received H.264 is muxed to fragmented MP4 and delivered buffer-by-buffer to onBuffer.
    CameraWebrtcOnMediaBuffer onBuffer;

    // Negotiated video configuration, filled from the SDP answer. Protected by configLock
    // since the SDP callback runs on a GStreamer thread.
    GMutex configLock;
    CameraWebrtcVideoConfig config;

    // Connectivity watchdog. webrtcbin runs no GMainLoop here, so a dedicated thread
    // enforces a bounded deadline for the peer connection to reach CONNECTED; the
    // connection-state handler also tears down immediately on FAILED.
    GThread *connThread;
    GMutex connMutex;
    GCond connCond;
    gboolean connEstablished;
    gboolean connCancelled;
    gint64 connDeadline; // monotonic usec deadline once armed

    // Video src pad from webrtcbin, retained so a keyframe (RTCP PLI) can be requested on
    // demand. Set on a GStreamer thread in onPadAdded; guarded by configLock.
    GstPad *videoPad;

    // Persistent keyframe worker: on demand it sends a short burst of RTCP PLIs. A burst (rather
    // than a single PLI) tolerates packet loss and a mostly-static camera that emits keyframes
    // only on request. Triggered at stream start and whenever a new viewer connects.
    GThread *keyframeThread;
    GMutex keyframeMutex;
    GCond keyframeCond;
    gboolean keyframePending;
    gboolean keyframeStop;
};

// ============================================================================
// Internal callbacks
// ============================================================================

static void onNegotiationNeeded(GstElement *webrtcbin, gpointer userData);
static void onOfferCreated(GstPromise *promise, gpointer userData);
static void onAnswerCreated(GstPromise *promise, gpointer userData);
static void onIceCandidateCallback(GstElement *webrtcbin, guint mlineIndex, gchar *candidate, gpointer userData);
static void onPadAdded(GstElement *webrtcbin, GstPad *pad, gpointer userData);
static GstFlowReturn onNewSample(GstAppSink *appsink, gpointer userData);
static GstBusSyncReply onBusMessage(GstBus *bus, GstMessage *message, gpointer userData);
static void extractSdpVideoConfig(const GstSDPMessage *msg, CameraWebrtcVideoConfig *config);
static void onConnectionStateNotify(GstElement *webrtcbin, GParamSpec *pspec, gpointer userData);
static gpointer connectivityWatchdog(gpointer userData);
static void requestKeyframe(CameraWebrtcClient *self);
static gpointer keyframeWorker(gpointer userData);
static void triggerKeyframeBurst(CameraWebrtcClient *self);

static void onNegotiationNeeded(GstElement *webrtcbin, gpointer userData)
{
    CameraWebrtcClient *self = (CameraWebrtcClient *) userData;

    // In answerer mode the camera provides the offer; we create our answer only after the remote
    // offer is set (see cameraWebrtcClientSetRemoteSdp). Do not generate an offer here.
    if (self->answerer)
    {
        return;
    }

    GstPromise *promise = gst_promise_new_with_change_func(onOfferCreated, self, NULL);
    g_signal_emit_by_name(webrtcbin, "create-offer", NULL, promise);
}

static void onOfferCreated(GstPromise *promise, gpointer userData)
{
    CameraWebrtcClient *self = (CameraWebrtcClient *) userData;

    const GstStructure *reply = gst_promise_get_reply(promise);

    if (reply == NULL)
    {
        emitError("[camera-stream] create-offer returned no reply\n");
        gst_promise_unref(promise);

        return;
    }

    GstWebRTCSessionDescription *offer = NULL;
    gst_structure_get(reply, "offer", GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &offer, NULL);

    if (offer == NULL)
    {
        emitError("[camera-stream] create-offer reply has no offer\n");
        gst_promise_unref(promise);

        return;
    }

    // Set local description
    g_signal_emit_by_name(self->webrtcbin, "set-local-description", offer, NULL);

    // Extract SDP string and notify
    g_autofree gchar *sdpStr = gst_sdp_message_as_text(offer->sdp);

    if (self->onOffer != NULL)
    {
        self->onOffer(sdpStr, self->userData);
    }

    gst_webrtc_session_description_free(offer);
    gst_promise_unref(promise);
}

// Answerer-mode counterpart of onOfferCreated: once webrtcbin has produced our answer to the
// camera's offer, set it as the local description and hand the SDP to the owner (via onOffer,
// which delivers the local description regardless of role).
static void onAnswerCreated(GstPromise *promise, gpointer userData)
{
    CameraWebrtcClient *self = (CameraWebrtcClient *) userData;

    const GstStructure *reply = gst_promise_get_reply(promise);

    if (reply == NULL)
    {
        emitError("[camera-stream] create-answer returned no reply\n");
        gst_promise_unref(promise);

        return;
    }

    GstWebRTCSessionDescription *answer = NULL;
    gst_structure_get(reply, "answer", GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &answer, NULL);

    if (answer == NULL)
    {
        emitError("[camera-stream] create-answer reply has no answer\n");
        gst_promise_unref(promise);

        return;
    }

    g_signal_emit_by_name(self->webrtcbin, "set-local-description", answer, NULL);

    g_autofree gchar *sdpStr = gst_sdp_message_as_text(answer->sdp);

    if (self->onOffer != NULL)
    {
        self->onOffer(sdpStr, self->userData);
    }

    gst_webrtc_session_description_free(answer);
    gst_promise_unref(promise);
}

static void onIceCandidateCallback(GstElement *webrtcbin, guint mlineIndex, gchar *candidate, gpointer userData)
{
    (void) webrtcbin;
    CameraWebrtcClient *self = (CameraWebrtcClient *) userData;

    if (self->onIce != NULL)
    {
        self->onIce(mlineIndex, candidate, self->userData);
    }
}

// Fires (on webrtcbin's thread) as the peer connection state changes. CONNECTED cancels
// the connectivity watchdog; FAILED tears the session down immediately via onClosed.
static void onConnectionStateNotify(GstElement *webrtcbin, GParamSpec *pspec, gpointer userData)
{
    (void) pspec;
    CameraWebrtcClient *self = (CameraWebrtcClient *) userData;

    GstWebRTCPeerConnectionState state = GST_WEBRTC_PEER_CONNECTION_STATE_NEW;
    g_object_get(webrtcbin, "connection-state", &state, NULL);

    if (state == GST_WEBRTC_PEER_CONNECTION_STATE_CONNECTED)
    {
        g_mutex_lock(&self->connMutex);
        self->connEstablished = TRUE;
        g_cond_signal(&self->connCond);
        g_mutex_unlock(&self->connMutex);
    }
    else if (state == GST_WEBRTC_PEER_CONNECTION_STATE_FAILED)
    {
        emitError("[camera-stream] WebRTC peer connection failed\n");

        g_mutex_lock(&self->connMutex);
        self->connCancelled = TRUE;
        g_cond_signal(&self->connCond);
        g_mutex_unlock(&self->connMutex);

        if (self->onClosed != NULL)
        {
            self->onClosed(self->userData);
        }
    }
}

// Waits until the peer connection reaches CONNECTED, the watchdog is cancelled (FAILED or
// teardown), or the deadline elapses. On deadline it reports the failure and tears down.
static gpointer connectivityWatchdog(gpointer userData)
{
    CameraWebrtcClient *self = (CameraWebrtcClient *) userData;

    g_mutex_lock(&self->connMutex);

    while (!self->connEstablished && !self->connCancelled)
    {
        if (!g_cond_wait_until(&self->connCond, &self->connMutex, self->connDeadline))
        {
            break; // deadline reached
        }
    }

    gboolean timedOut = !self->connEstablished && !self->connCancelled;
    g_mutex_unlock(&self->connMutex);

    if (timedOut)
    {
        emitError("[camera-stream] Timed out after %d s waiting for WebRTC connectivity\n",
                  CONNECTIVITY_TIMEOUT_SECONDS);

        if (self->onClosed != NULL)
        {
            self->onClosed(self->userData);
        }
    }

    return NULL;
}

// Bus sync handler: fires in the thread that posts the message. On an ERROR
// (e.g. the render window was closed) or EOS, notify the owner via onClosed so it
// can tear the session down gracefully. Runs synchronously (no GMainLoop needed).
static GstBusSyncReply onBusMessage(GstBus *bus, GstMessage *message, gpointer userData)
{
    (void) bus;
    CameraWebrtcClient *self = (CameraWebrtcClient *) userData;

    switch (GST_MESSAGE_TYPE(message))
    {
        case GST_MESSAGE_ERROR:
        {
            g_autoptr(GError) err = NULL;
            g_autofree gchar *debug = NULL;
            gst_message_parse_error(message, &err, &debug);
            emitError("[camera-stream] pipeline error, tearing down: %s\n", err != NULL ? err->message : "(unknown)");

            if (self->onClosed != NULL)
            {
                self->onClosed(self->userData);
            }

            break;
        }

        case GST_MESSAGE_EOS:
            if (self->onClosed != NULL)
            {
                self->onClosed(self->userData);
            }

            break;

        default:
            break;
    }

    return GST_BUS_PASS;
}

// appsink new-sample callback: hands each muxed fragmented-MP4 buffer to the owner. Runs on a
// GStreamer streaming thread.
static GstFlowReturn onNewSample(GstAppSink *appsink, gpointer userData)
{
    CameraWebrtcClient *self = (CameraWebrtcClient *) userData;
    GstSample *sample = gst_app_sink_pull_sample(appsink);

    if (sample == NULL)
    {
        return GST_FLOW_OK;
    }

    GstBuffer *buffer = gst_sample_get_buffer(sample);

    if (buffer != NULL && self->onBuffer != NULL)
    {
        gboolean isHeader = GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_HEADER);
        GstMapInfo map;

        if (gst_buffer_map(buffer, &map, GST_MAP_READ))
        {
            self->onBuffer(map.data, map.size, isHeader, self->userData);
            gst_buffer_unmap(buffer, &map);
        }
    }

    gst_sample_unref(sample);

    return GST_FLOW_OK;
}

static gboolean padIsVideo(GstPad *pad)
{
    GstCaps *caps = gst_pad_get_current_caps(pad);

    if (caps == NULL)
    {
        caps = gst_pad_query_caps(pad, NULL);
    }

    gboolean isVideo = FALSE;

    if (caps != NULL && gst_caps_get_size(caps) > 0)
    {
        const GstStructure *structure = gst_caps_get_structure(caps, 0);
        const gchar *media = gst_structure_get_string(structure, "media");
        isVideo = (media != NULL && g_strcmp0(media, "video") == 0);
    }

    if (caps != NULL)
    {
        gst_caps_unref(caps);
    }

    return isVideo;
}

// Ask the camera for an immediate keyframe by sending an upstream force-key-unit event on the
// webrtcbin video src pad; webrtcbin's rtpsession turns this into an RTCP PLI. Built as a raw
// custom event (matching gst_video_event_new_upstream_force_key_unit) so no gstreamer-video
// dependency is needed. Safe to call at any time and from any thread; a no-op until the video pad
// exists.
static void requestKeyframe(CameraWebrtcClient *self)
{
    GstPad *pad = NULL;

    g_mutex_lock(&self->configLock);

    if (self->videoPad != NULL)
    {
        pad = gst_object_ref(self->videoPad);
    }

    g_mutex_unlock(&self->configLock);

    if (pad == NULL)
    {
        return;
    }

    GstStructure *structure = gst_structure_new("GstForceKeyUnit",
                                                "running-time",
                                                GST_TYPE_CLOCK_TIME,
                                                GST_CLOCK_TIME_NONE,
                                                "all-headers",
                                                G_TYPE_BOOLEAN,
                                                TRUE,
                                                "count",
                                                G_TYPE_UINT,
                                                0,
                                                NULL);
    GstEvent *event = gst_event_new_custom(GST_EVENT_CUSTOM_UPSTREAM, structure);
    gst_pad_send_event(pad, event);
    gst_object_unref(pad);
}

// Persistent worker: waits for a request, then sends a short burst of keyframe (PLI) requests.
// A burst overcomes a lost initial PLI and a mostly-static camera that would otherwise not emit a
// keyframe for many seconds. requestKeyframe() is a no-op until the video pad exists, so an early
// burst is harmless.
static gpointer keyframeWorker(gpointer userData)
{
    CameraWebrtcClient *self = (CameraWebrtcClient *) userData;

    g_mutex_lock(&self->keyframeMutex);

    while (!self->keyframeStop)
    {
        if (!self->keyframePending)
        {
            g_cond_wait(&self->keyframeCond, &self->keyframeMutex);

            continue;
        }

        self->keyframePending = FALSE;
        g_mutex_unlock(&self->keyframeMutex);

        for (int i = 0; i < 5 && !self->keyframeStop; i++)
        {
            requestKeyframe(self);
            g_usleep(250 * 1000);
        }

        g_mutex_lock(&self->keyframeMutex);
    }

    g_mutex_unlock(&self->keyframeMutex);

    return NULL;
}

// Ask the keyframe worker to send a burst of PLIs. Cheap and non-blocking; safe from any thread.
static void triggerKeyframeBurst(CameraWebrtcClient *self)
{
    g_mutex_lock(&self->keyframeMutex);
    self->keyframePending = TRUE;
    g_cond_signal(&self->keyframeCond);
    g_mutex_unlock(&self->keyframeMutex);
}

static void onPadAdded(GstElement *webrtcbin, GstPad *pad, gpointer userData)
{
    (void) webrtcbin;
    CameraWebrtcClient *self = (CameraWebrtcClient *) userData;

    if (GST_PAD_DIRECTION(pad) != GST_PAD_SRC)
    {
        return;
    }

    emitOutput("[camera-stream] webrtcbin src pad added '%s' (video=%d)\n", GST_PAD_NAME(pad), padIsVideo(pad));

    // The camera also negotiates an audio m-line (so the offer/answer m-lines match), but this
    // reference app consumes video only. The audio pad must still be drained into a fakesink;
    // leaving it unlinked makes its RTP source error out with "not-linked", which tears down the
    // whole pipeline.
    if (!padIsVideo(pad))
    {
        GstElement *audioSink = gst_element_factory_make("fakesink", NULL);

        if (audioSink != NULL)
        {
            g_object_set(audioSink, "sync", FALSE, "async", FALSE, NULL);
            gst_bin_add(GST_BIN(self->pipeline), audioSink);
            gst_element_sync_state_with_parent(audioSink);

            GstPad *audioSinkPad = gst_element_get_static_pad(audioSink, "sink");

            if (audioSinkPad != NULL)
            {
                gst_pad_link(pad, audioSinkPad);
                gst_object_unref(audioSinkPad);
            }
        }

        return;
    }

    // Passthrough chain (no decode): RTP H.264 -> depay -> parse -> fragmented MP4 -> appsink.
    // The muxed fMP4 buffers are handed to the owner via onBuffer; the browser (or a media
    // player) decodes the H.264, so no display/GPU is needed here.
    GstElement *depay = gst_element_factory_make("rtph264depay", NULL);
    GstElement *parse = gst_element_factory_make("h264parse", NULL);
    // The camera's RTP buffers arrive without a DTS (and mp4mux derives a null PTS from that),
    // so mp4mux aborts with "Buffer has no PTS." h264timestamper reconstructs monotonic PTS/DTS
    // for the parsed access units before muxing.
    GstElement *timestamper = gst_element_factory_make("h264timestamper", NULL);
    // mp4mux only accepts H.264 as stream-format=avc/avc3, alignment=au. rtph264depay emits
    // byte-stream/nal, so force h264parse to output AVC access units via a capsfilter; without
    // this mp4mux fails to negotiate and aborts with "Could not multiplex stream".
    GstElement *capsfilter = gst_element_factory_make("capsfilter", NULL);
    GstElement *mux = gst_element_factory_make("mp4mux", NULL);
    GstElement *sink = gst_element_factory_make("appsink", NULL);

    if (depay == NULL || parse == NULL || timestamper == NULL || capsfilter == NULL || mux == NULL || sink == NULL)
    {
        emitError("[camera-stream] failed to create depay/parse/timestamper/capsfilter/mux/appsink elements\n");

        return;
    }

    {
        GstCaps *avcCaps = gst_caps_new_simple(
            "video/x-h264", "stream-format", G_TYPE_STRING, "avc", "alignment", G_TYPE_STRING, "au", NULL);
        g_object_set(capsfilter, "caps", avcCaps, NULL);
        gst_caps_unref(avcCaps);
    }

    // Fragmented, streamable MP4 so a browser can begin playing mid-stream and a recorded file
    // is playable even if recording is stopped without an EOS. A short fragment duration lowers
    // the latency to the first playable fragment and keeps the browser close to the live edge.
    g_object_set(mux, "fragment-duration", 250, "streamable", TRUE, NULL);

    GstAppSinkCallbacks callbacks = {0};
    callbacks.new_sample = onNewSample;
    g_object_set(sink, "emit-signals", FALSE, "sync", FALSE, NULL);
    gst_app_sink_set_callbacks(GST_APP_SINK(sink), &callbacks, self, NULL);

    gst_bin_add_many(GST_BIN(self->pipeline), depay, parse, timestamper, capsfilter, mux, sink, NULL);
    gst_element_sync_state_with_parent(depay);
    gst_element_sync_state_with_parent(parse);
    gst_element_sync_state_with_parent(timestamper);
    gst_element_sync_state_with_parent(capsfilter);
    gst_element_sync_state_with_parent(mux);
    gst_element_sync_state_with_parent(sink);

    if (!gst_element_link_many(depay, parse, timestamper, capsfilter, mux, sink, NULL))
    {
        emitError("[camera-stream] failed to link depay -> parse -> timestamper -> capsfilter -> mux -> appsink\n");

        return;
    }

    // Link the webrtcbin src pad into the depayloader.
    GstPad *depaySink = gst_element_get_static_pad(depay, "sink");
    GstPadLinkReturn linkResult = gst_pad_link(pad, depaySink);
    gst_object_unref(depaySink);

    emitOutput("[camera-stream] linked video pad to fragmented MP4 (link result %d)\n", linkResult);

    // Retain the pad so keyframes can be requested on demand, then trigger a keyframe burst so
    // mp4mux can cut its first fragment without waiting for the camera's periodic keyframe (which,
    // for a mostly-static scene, can be many seconds away).
    g_mutex_lock(&self->configLock);
    self->videoPad = gst_object_ref(pad);
    g_mutex_unlock(&self->configLock);

    triggerKeyframeBurst(self);
}

// ============================================================================
// Public API
// ============================================================================

CameraWebrtcClient *cameraWebrtcClientCreate(CameraWebrtcOnOfferReady onOffer,
                                             CameraWebrtcOnIceCandidate onIce,
                                             CameraWebrtcOnClosed onClosed,
                                             CameraWebrtcOnMediaBuffer onBuffer,
                                             gpointer userData)
{
    static gboolean gstInitialized = FALSE;

    if (!gstInitialized)
    {
        gst_init(NULL, NULL);
        gstInitialized = TRUE;
    }

    CameraWebrtcClient *self = g_new0(CameraWebrtcClient, 1);
    self->onOffer = onOffer;
    self->onIce = onIce;
    self->onClosed = onClosed;
    self->onBuffer = onBuffer;
    self->userData = userData;

    g_mutex_init(&self->configLock);
    self->config.payloadType = -1;
    self->config.clockRate = -1;
    self->config.bitrateKbps = -1;
    self->config.width = -1;
    self->config.height = -1;
    self->config.framerateNum = -1;
    self->config.framerateDen = 1;

    g_mutex_init(&self->connMutex);
    g_cond_init(&self->connCond);

    g_mutex_init(&self->keyframeMutex);
    g_cond_init(&self->keyframeCond);
    self->keyframeThread = g_thread_new("kf-worker", keyframeWorker, self);

    // Create pipeline with webrtcbin
    self->pipeline = gst_pipeline_new("camera-stream");
    self->webrtcbin = gst_element_factory_make("webrtcbin", "webrtc");

    if (self->webrtcbin == NULL)
    {
        emitError("[camera-stream] webrtcbin element not available\n");
        cameraWebrtcClientDestroy(self);

        return NULL;
    }

    // We are receive-only
    g_object_set(self->webrtcbin, "bundle-policy", 3 /* max-bundle */, NULL);

    // Local-only connection: clear any STUN/TURN servers so ICE gathers only
    // host candidates (same-machine loopback/LAN). No server-reflexive or relay
    // candidates are produced.
    g_object_set(self->webrtcbin, "stun-server", NULL, "turn-server", NULL, NULL);

    gst_bin_add(GST_BIN(self->pipeline), self->webrtcbin);

    // Watch the pipeline bus so an unexpected stop (render window closed -> ERROR,
    // or EOS) triggers a graceful session teardown instead of leaking it. A sync
    // handler is used because the cs command blocks on a GCond rather than running
    // a GMainLoop, so an async bus watch would never be dispatched.
    {
        GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(self->pipeline));
        gst_bus_set_sync_handler(bus, onBusMessage, self, NULL);
        gst_object_unref(bus);
    }

    // Connect signals
    g_signal_connect(self->webrtcbin, "on-negotiation-needed", G_CALLBACK(onNegotiationNeeded), self);
    g_signal_connect(self->webrtcbin, "on-ice-candidate", G_CALLBACK(onIceCandidateCallback), self);
    g_signal_connect(self->webrtcbin, "pad-added", G_CALLBACK(onPadAdded), self);
    g_signal_connect(self->webrtcbin, "notify::connection-state", G_CALLBACK(onConnectionStateNotify), self);

    // Add receive-only transceivers so the generated offer contains real media
    // sections. Without explicit transceivers, webrtcbin creates an offer with
    // no media lines, which the camera rejects. The camera answers with both a
    // video and an audio m-line, and WebRTC requires the answer to have the
    // same number of m-lines as the offer, so we must offer both even though
    // this app only renders video. The order (video then audio) must match the
    // camera's answer.
    {
        GstWebRTCRTPTransceiver *transceiver = NULL;
        GstCaps *videoCaps = gst_caps_from_string(
            "application/x-rtp,media=video,encoding-name=H264,clock-rate=(int)90000,payload=(int)96");
        g_signal_emit_by_name(
            self->webrtcbin, "add-transceiver", GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY, videoCaps, &transceiver);
        gst_caps_unref(videoCaps);

        if (transceiver != NULL)
        {
            gst_object_unref(transceiver);
        }
    }

    {
        GstWebRTCRTPTransceiver *transceiver = NULL;
        GstCaps *audioCaps = gst_caps_from_string(
            "application/"
            "x-rtp,media=audio,encoding-name=OPUS,clock-rate=(int)48000,payload=(int)111,encoding-params=(string)2");
        g_signal_emit_by_name(
            self->webrtcbin, "add-transceiver", GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY, audioCaps, &transceiver);
        gst_caps_unref(audioCaps);

        if (transceiver != NULL)
        {
            gst_object_unref(transceiver);
        }
    }

    return self;
}

bool cameraWebrtcClientStart(CameraWebrtcClient *client)
{
    if (client == NULL || client->pipeline == NULL)
    {
        return false;
    }

    GstStateChangeReturn ret = gst_element_set_state(client->pipeline, GST_STATE_PLAYING);

    return ret != GST_STATE_CHANGE_FAILURE;
}

void cameraWebrtcClientSetAnswerer(CameraWebrtcClient *client, bool answerer)
{
    if (client != NULL)
    {
        client->answerer = answerer ? TRUE : FALSE;
    }
}

bool cameraWebrtcClientSetRemoteSdp(CameraWebrtcClient *client, const gchar *sdp)
{
    if (client == NULL || client->webrtcbin == NULL || sdp == NULL)
    {
        return false;
    }

    GstSDPMessage *sdpMsg = NULL;

    if (gst_sdp_message_new(&sdpMsg) != GST_SDP_OK)
    {
        return false;
    }

    if (gst_sdp_message_parse_buffer((const guint8 *) sdp, (guint) strlen(sdp), sdpMsg) != GST_SDP_OK)
    {
        gst_sdp_message_free(sdpMsg);

        return false;
    }

    // Capture the negotiated codec / bitrate / profile for the summary.
    g_mutex_lock(&client->configLock);
    extractSdpVideoConfig(sdpMsg, &client->config);
    g_mutex_unlock(&client->configLock);

    // In answerer mode the remote description is the camera's OFFER: set it, then create our answer
    // (handed to the owner via onOffer). In offerer mode it is the camera's ANSWER.
    GstWebRTCSDPType sdpType = client->answerer ? GST_WEBRTC_SDP_TYPE_OFFER : GST_WEBRTC_SDP_TYPE_ANSWER;
    GstWebRTCSessionDescription *remote = gst_webrtc_session_description_new(sdpType, sdpMsg);

    g_signal_emit_by_name(client->webrtcbin, "set-remote-description", remote, NULL);
    gst_webrtc_session_description_free(remote);

    if (client->answerer)
    {
        GstPromise *promise = gst_promise_new_with_change_func(onAnswerCreated, client, NULL);
        g_signal_emit_by_name(client->webrtcbin, "create-answer", NULL, promise);
    }

    // Signaling is complete enough to attempt connectivity; arm the bounded watchdog so a
    // stuck ICE exchange becomes a reported teardown rather than an indefinite hang. Started
    // once; cancelled when the connection reaches CONNECTED, FAILED, or on destroy.
    g_mutex_lock(&client->connMutex);

    if (client->connThread == NULL && !client->connEstablished && !client->connCancelled)
    {
        client->connDeadline = g_get_monotonic_time() + (gint64) CONNECTIVITY_TIMEOUT_SECONDS * G_USEC_PER_SEC;
        client->connThread = g_thread_new("cam-conn-wd", connectivityWatchdog, client);
    }

    g_mutex_unlock(&client->connMutex);

    return true;
}

void cameraWebrtcClientAddIceCandidate(CameraWebrtcClient *client, guint mlineIndex, const gchar *candidate)
{
    if (client == NULL || client->webrtcbin == NULL)
    {
        return;
    }

    g_signal_emit_by_name(client->webrtcbin, "add-ice-candidate", mlineIndex, candidate);
}

void cameraWebrtcClientDestroy(CameraWebrtcClient *client)
{
    if (client == NULL)
    {
        return;
    }

    // Stop the connectivity watchdog before tearing anything else down. Signalling it and
    // joining ensures the thread cannot fire onClosed after this point.
    g_mutex_lock(&client->connMutex);
    client->connCancelled = TRUE;
    g_cond_signal(&client->connCond);
    g_mutex_unlock(&client->connMutex);

    if (client->connThread != NULL)
    {
        g_thread_join(client->connThread);
        client->connThread = NULL;
    }

    // Stop the keyframe worker (it may be waiting on its condition or mid-burst) and join it.
    g_mutex_lock(&client->keyframeMutex);
    client->keyframeStop = TRUE;
    g_cond_signal(&client->keyframeCond);
    g_mutex_unlock(&client->keyframeMutex);

    if (client->keyframeThread != NULL)
    {
        g_thread_join(client->keyframeThread);
        client->keyframeThread = NULL;
    }

    if (client->pipeline != NULL)
    {
        // Detach the bus sync handler first so it cannot fire onClosed while we
        // are already tearing the pipeline down.
        GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(client->pipeline));

        if (bus != NULL)
        {
            gst_bus_set_sync_handler(bus, NULL, NULL, NULL);
            gst_object_unref(bus);
        }

        gst_element_set_state(client->pipeline, GST_STATE_NULL);
        gst_object_unref(client->pipeline);
    }

    if (client->videoPad != NULL)
    {
        gst_object_unref(client->videoPad);
        client->videoPad = NULL;
    }

    g_mutex_clear(&client->configLock);
    g_mutex_clear(&client->connMutex);
    g_cond_clear(&client->connCond);
    g_mutex_clear(&client->keyframeMutex);
    g_cond_clear(&client->keyframeCond);
    g_free(client);
}

void cameraWebrtcClientGetVideoConfig(CameraWebrtcClient *client, CameraWebrtcVideoConfig *config)
{
    if (client == NULL || config == NULL)
    {
        return;
    }

    g_mutex_lock(&client->configLock);
    *config = client->config;
    g_mutex_unlock(&client->configLock);
}

void cameraWebrtcClientRequestKeyframe(CameraWebrtcClient *client)
{
    if (client == NULL)
    {
        return;
    }

    triggerKeyframeBurst(client);
}

// Pull the negotiated codec, RTP payload/clock, H.264 profile-level-id, and signaled
// bandwidth (b=AS) out of the video m-section of the SDP answer.
static void extractSdpVideoConfig(const GstSDPMessage *msg, CameraWebrtcVideoConfig *config)
{
    guint mediaCount = gst_sdp_message_medias_len(msg);

    for (guint m = 0; m < mediaCount; m++)
    {
        const GstSDPMedia *media = gst_sdp_message_get_media(msg, m);

        if (g_strcmp0(gst_sdp_media_get_media(media), "video") != 0)
        {
            continue;
        }

        for (guint b = 0; b < gst_sdp_media_bandwidths_len(media); b++)
        {
            const GstSDPBandwidth *bw = gst_sdp_media_get_bandwidth(media, b);

            if (bw != NULL && g_strcmp0(bw->bwtype, "AS") == 0)
            {
                config->bitrateKbps = (gint) bw->bandwidth;
            }
        }

        for (guint a = 0; a < gst_sdp_media_attributes_len(media); a++)
        {
            const GstSDPAttribute *attr = gst_sdp_media_get_attribute(media, a);

            if (attr == NULL || attr->value == NULL)
            {
                continue;
            }

            if (g_strcmp0(attr->key, "rtpmap") == 0)
            {
                gint payloadType = 0;
                gint clockRate = 0;
                gchar encoding[16] = {0};

                // e.g. "96 H264/90000"
                if (sscanf(attr->value, "%d %15[^/]/%d", &payloadType, encoding, &clockRate) >= 2)
                {
                    g_strlcpy(config->codec, encoding, sizeof(config->codec));
                    config->payloadType = payloadType;

                    if (clockRate > 0)
                    {
                        config->clockRate = clockRate;
                    }
                }
            }
            else if (g_strcmp0(attr->key, "fmtp") == 0)
            {
                // e.g. "96 profile-level-id=42e01f;packetization-mode=1"
                const gchar *p = strstr(attr->value, "profile-level-id=");

                if (p != NULL)
                {
                    p += strlen("profile-level-id=");
                    gsize i = 0;

                    while (*p != '\0' && *p != ';' && *p != ' ' && i < sizeof(config->profileLevelId) - 1)
                    {
                        config->profileLevelId[i++] = *p++;
                    }

                    config->profileLevelId[i] = '\0';
                }
            }
        }

        break;
    }
}
