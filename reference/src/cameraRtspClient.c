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

#include "cameraRtspClient.h"
#include "barton-core-reference-io.h"

#include <gst/app/gstappsink.h>
#include <gst/gst.h>

struct _CameraRtspClient
{
    GstElement *pipeline;
    GstElement *src; // rtspsrc

    CameraRtspOnClosed onClosed;
    CameraRtspOnMediaBuffer onBuffer;
    gpointer userData;

    // rtspsrc adds a pad per stream (video, and possibly audio/metadata). Only the first video
    // stream is muxed; guard so a second video pad is ignored. Written on a GStreamer thread.
    GMutex lock;
    gboolean videoLinked;
};

// ============================================================================
// Internal callbacks
// ============================================================================

static GstFlowReturn onNewSample(GstAppSink *appsink, gpointer userData);
static GstBusSyncReply onBusMessage(GstBus *bus, GstMessage *message, gpointer userData);
static void onPadAdded(GstElement *src, GstPad *pad, gpointer userData);

// appsink new-sample callback: hands each muxed fragmented-MP4 buffer to the owner. Runs on a
// GStreamer streaming thread.
static GstFlowReturn onNewSample(GstAppSink *appsink, gpointer userData)
{
    CameraRtspClient *self = (CameraRtspClient *) userData;
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

// Bus sync handler: fires in the thread that posts the message. On an ERROR (e.g. the RTSP source
// failed to connect or authenticate) or EOS, notify the owner via onClosed so it can tear the
// session down gracefully. Runs synchronously (no GMainLoop needed).
static GstBusSyncReply onBusMessage(GstBus *bus, GstMessage *message, gpointer userData)
{
    (void) bus;
    CameraRtspClient *self = (CameraRtspClient *) userData;

    switch (GST_MESSAGE_TYPE(message))
    {
        case GST_MESSAGE_ERROR:
        {
            g_autoptr(GError) err = NULL;
            g_autofree gchar *debug = NULL;
            gst_message_parse_error(message, &err, &debug);
            emitError("[camera-stream] RTSP pipeline error, tearing down: %s\n",
                      err != NULL ? err->message : "(unknown)");

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

    // Nothing drains this bus (there is no GMainLoop or async watch), so drop each message after
    // handling it here rather than letting messages accumulate on the bus queue until teardown.
    return GST_BUS_DROP;
}

// Returns TRUE when the pad carries an H.264 video RTP stream. rtspsrc pads advertise
// application/x-rtp with media/encoding-name fields describing the stream.
static gboolean padIsH264Video(GstPad *pad)
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
        const gchar *encoding = gst_structure_get_string(structure, "encoding-name");
        isVideo = (media != NULL && g_strcmp0(media, "video") == 0) &&
                  (encoding == NULL || g_ascii_strcasecmp(encoding, "H264") == 0);
    }

    if (caps != NULL)
    {
        gst_caps_unref(caps);
    }

    return isVideo;
}

// rtspsrc adds a src pad per stream once the RTSP SETUP completes. Link the first H.264 video
// stream into the fragmented-MP4 mux chain; drain any other stream (audio/metadata) into a
// fakesink so its RTP source does not error out "not-linked" and tear down the pipeline.
static void onPadAdded(GstElement *src, GstPad *pad, gpointer userData)
{
    (void) src;
    CameraRtspClient *self = (CameraRtspClient *) userData;

    if (GST_PAD_DIRECTION(pad) != GST_PAD_SRC)
    {
        return;
    }

    gboolean isVideo = padIsH264Video(pad);

    if (!isVideo)
    {
        GstElement *drain = gst_element_factory_make("fakesink", NULL);

        if (drain != NULL)
        {
            g_object_set(drain, "sync", FALSE, "async", FALSE, NULL);
            gst_bin_add(GST_BIN(self->pipeline), drain);
            gst_element_sync_state_with_parent(drain);

            GstPad *drainSink = gst_element_get_static_pad(drain, "sink");

            if (drainSink != NULL)
            {
                gst_pad_link(pad, drainSink);
                gst_object_unref(drainSink);
            }
        }

        return;
    }

    // Only the first video stream is muxed; ignore any subsequent video pad.
    g_mutex_lock(&self->lock);
    gboolean alreadyLinked = self->videoLinked;
    self->videoLinked = TRUE;
    g_mutex_unlock(&self->lock);

    if (alreadyLinked)
    {
        return;
    }

    // Passthrough chain (no decode): RTP H.264 -> depay -> parse -> fragmented MP4 -> appsink.
    // Identical to the WebRTC path's downstream chain so both feed the same media-server / file
    // sink; only the source element differs.
    GstElement *depay = gst_element_factory_make("rtph264depay", NULL);
    GstElement *parse = gst_element_factory_make("h264parse", NULL);
    // Some cameras emit RTP buffers without a DTS, which makes mp4mux abort with "Buffer has no
    // PTS." h264timestamper reconstructs monotonic PTS/DTS for the parsed access units before
    // muxing; it is a no-op when the timestamps are already present.
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

        if (self->onClosed != NULL)
        {
            self->onClosed(self->userData);
        }

        return;
    }

    {
        GstCaps *avcCaps = gst_caps_new_simple(
            "video/x-h264", "stream-format", G_TYPE_STRING, "avc", "alignment", G_TYPE_STRING, "au", NULL);
        g_object_set(capsfilter, "caps", avcCaps, NULL);
        gst_caps_unref(avcCaps);
    }

    // Fragmented, streamable MP4 so a browser can begin playing mid-stream and a recorded file is
    // playable even if recording is stopped without an EOS. A short fragment duration lowers the
    // latency to the first playable fragment and keeps the browser close to the live edge.
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

        if (self->onClosed != NULL)
        {
            self->onClosed(self->userData);
        }

        return;
    }

    // Link the rtspsrc src pad into the depayloader.
    GstPad *depaySink = gst_element_get_static_pad(depay, "sink");
    GstPadLinkReturn linkResult = gst_pad_link(pad, depaySink);
    gst_object_unref(depaySink);

    if (linkResult != GST_PAD_LINK_OK)
    {
        emitError("[camera-stream] failed to link rtspsrc video pad to the depayloader (link result %d)\n", linkResult);

        if (self->onClosed != NULL)
        {
            self->onClosed(self->userData);
        }

        return;
    }

    emitOutput("[camera-stream] linked RTSP video pad to fragmented MP4 (link result %d)\n", linkResult);
}

// ============================================================================
// Public API
// ============================================================================

CameraRtspClient *
cameraRtspClientCreate(CameraRtspOnClosed onClosed, CameraRtspOnMediaBuffer onBuffer, gpointer userData)
{
    static gsize gstInitOnce = 0;

    if (g_once_init_enter(&gstInitOnce))
    {
        gst_init(NULL, NULL);
        g_once_init_leave(&gstInitOnce, 1);
    }

    CameraRtspClient *self = g_new0(CameraRtspClient, 1);
    self->onClosed = onClosed;
    self->onBuffer = onBuffer;
    self->userData = userData;
    g_mutex_init(&self->lock);

    self->pipeline = gst_pipeline_new("camera-rtsp");
    self->src = gst_element_factory_make("rtspsrc", "rtsp-source");

    if (self->pipeline == NULL || self->src == NULL)
    {
        emitError("[camera-stream] failed to create the GStreamer pipeline or rtspsrc element\n");
        cameraRtspClientDestroy(self);

        return NULL;
    }

    gst_bin_add(GST_BIN(self->pipeline), self->src);

    // rtspsrc exposes its RTP streams as dynamic pads once RTSP setup completes.
    g_signal_connect(self->src, "pad-added", G_CALLBACK(onPadAdded), self);

    // Watch the pipeline bus so an unexpected stop (RTSP error or EOS) triggers a graceful
    // session teardown instead of leaking it. A sync handler is used because the cs command
    // blocks on a GCond rather than running a GMainLoop, so an async bus watch would never
    // be dispatched.
    {
        GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(self->pipeline));
        gst_bus_set_sync_handler(bus, onBusMessage, self, NULL);
        gst_object_unref(bus);
    }

    return self;
}

bool cameraRtspClientStart(CameraRtspClient *client, const gchar *rtspUrl, const gchar *user, const gchar *pass)
{
    if (client == NULL || client->pipeline == NULL || client->src == NULL || rtspUrl == NULL)
    {
        return false;
    }

    // latency: a small jitter buffer keeps the stream close to the live edge. drop-on-latency
    // discards late packets rather than stalling. Credentials drive RTSP basic/digest auth; the
    // stored device credentials are the same ones the driver uses for the ONVIF SOAP calls.
    g_object_set(client->src, "location", rtspUrl, "latency", 200, "drop-on-latency", TRUE, NULL);

    if (user != NULL && user[0] != '\0')
    {
        g_object_set(client->src, "user-id", user, NULL);
    }

    if (pass != NULL && pass[0] != '\0')
    {
        g_object_set(client->src, "user-pw", pass, NULL);
    }

    GstStateChangeReturn ret = gst_element_set_state(client->pipeline, GST_STATE_PLAYING);

    return ret != GST_STATE_CHANGE_FAILURE;
}

void cameraRtspClientDestroy(CameraRtspClient *client)
{
    if (client == NULL)
    {
        return;
    }

    if (client->pipeline != NULL)
    {
        // Detach the bus sync handler first so it cannot fire onClosed while we are already
        // tearing the pipeline down.
        GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(client->pipeline));

        if (bus != NULL)
        {
            gst_bus_set_sync_handler(bus, NULL, NULL, NULL);
            gst_object_unref(bus);
        }

        gst_element_set_state(client->pipeline, GST_STATE_NULL);

        // Wait (bounded) for the NULL state change to settle so the streaming task and any
        // in-flight appsink/bus callbacks (which take `client` as userData) have stopped before we
        // free it. A finite timeout avoids hanging teardown if an element wedges; warn if it hits.
        GstStateChangeReturn stateRet = gst_element_get_state(client->pipeline, NULL, NULL, 5 * GST_SECOND);

        if (stateRet == GST_STATE_CHANGE_ASYNC)
        {
            emitError("[camera-stream] RTSP pipeline did not reach NULL within 5s during teardown\n");
        }

        gst_object_unref(client->pipeline);
    }

    g_mutex_clear(&client->lock);
    g_free(client);
}
