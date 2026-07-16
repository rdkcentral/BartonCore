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

#include "cameraMediaServer.h"
#include "barton-core-reference-io.h"
#include <gio/gio.h>
#include <string.h>

// Self-contained player page. Uses Media Source Extensions to play the live fragmented MP4: it
// streams /stream.mp4 with fetch(), appends fragments to a SourceBuffer in 'sequence' mode, trims
// the buffer to stay near the live edge, and seeks/recovers if it falls behind or stalls. This is
// far more robust for a continuously-growing live stream than a plain progressive <video src>,
// which starves at the live edge. The codec matches the camera's negotiated H.264 (42e01f).
static const char CAMERA_PLAYER_PAGE[] =
    "<!doctype html><html><head><meta charset=\"utf-8\"><title>Barton Camera</title>\n"
    "<style>html,body{margin:0;height:100%;background:#111}"
    "video{width:100%;height:100vh;object-fit:contain;background:#000}</style></head>\n"
    "<body><video id=\"v\" autoplay muted playsinline controls></video>\n"
    "<script>\n"
    "const v=document.getElementById('v');\n"
    "const CODEC='video/mp4; codecs=\"avc1.42e01f\"';\n"
    "let ms,sb,reader,url,q=[],started=false;\n"
    "function pump(){if(!sb||sb.updating||!q.length)return;try{sb.appendBuffer(q.shift());}catch(e){reset();}}\n"
    "function trim(){if(!sb||sb.updating||!sb.buffered.length)return;"
    "const s=sb.buffered.start(0),e=sb.buffered.end(sb.buffered.length-1);"
    "if(e-s>12){try{sb.remove(s,e-8);}catch(e2){}}}\n"
    "function edge(){if(!sb||!sb.buffered.length)return;const e=sb.buffered.end(sb.buffered.length-1);"
    "if(e-v.currentTime>4||v.currentTime<sb.buffered.start(0))v.currentTime=Math.max(0,e-0.3);"
    "if(v.paused)v.play().catch(()=>{});}\n"
    "function reset(){try{reader&&reader.cancel();}catch(e){}"
    "if(url){URL.revokeObjectURL(url);url=null;}sb=null;q=[];started=false;setTimeout(start,1000);}\n"
    "async function start(){\n"
    "  if(!('MediaSource'in window)||!MediaSource.isTypeSupported(CODEC)){v.outerHTML='<p "
    "style=\\\"color:#fff\\\">MSE/H.264 not supported by this browser.</p>';return;}\n"
    "  ms=new MediaSource();url=URL.createObjectURL(ms);v.src=url;\n"
    "  await new Promise(r=>ms.addEventListener('sourceopen',r,{once:true}));\n"
    "  sb=ms.addSourceBuffer(CODEC);sb.mode='sequence';\n"
    "  sb.addEventListener('updateend',()=>{pump();trim();edge();});\n"
    "  const resp=await fetch('/stream.mp4',{cache:'no-store'});\n"
    "  reader=resp.body.getReader();\n"
    "  while(true){const{done,value}=await reader.read();if(done)break;q.push(value);pump();"
    "if(!started){started=true;v.play().catch(()=>{});}}\n"
    "  reset();\n"
    "}\n"
    "v.addEventListener('waiting',edge);setInterval(edge,2000);\n"
    "start().catch(()=>setTimeout(start,1000));\n"
    "</script></body></html>\n";

struct _CameraMediaServer
{
    gchar *bindHost;
    guint16 port;
    gchar *url;

    GThread *thread;
    GMainContext *context;
    GMainLoop *loop;
    GSocketService *service; // created on the server thread

    // Guards initSegment and clients (touched by both the server thread and the GStreamer
    // appsink thread that calls cameraMediaServerPushBuffer).
    GMutex mutex;
    GByteArray *initSegment; // cached ftyp + moov, replayed to each new viewer
    gboolean initComplete;   // set once the first fragment (moof/styp) has been seen
    GList *clients;          // GSocketConnection* (owns a ref each), currently streaming

    // Invoked (on the server thread) when a viewer connects; used to request a keyframe.
    CameraMediaServerOnViewer onViewer;
    gpointer onViewerData;

    // Startup handshake between the caller and the server thread.
    GMutex startMutex;
    GCond startCond;
    gboolean started;
    gboolean startFailed;
};

// Write the whole page as a single HTTP response, then let the connection drop.
static void servePlayerPage(GOutputStream *out)
{
    gsize pageLen = strlen(CAMERA_PLAYER_PAGE);
    gchar *resp = g_strdup_printf("HTTP/1.1 200 OK\r\n"
                                  "Content-Type: text/html; charset=utf-8\r\n"
                                  "Content-Length: %zu\r\n"
                                  "Connection: close\r\n\r\n%s",
                                  pageLen,
                                  CAMERA_PLAYER_PAGE);
    g_output_stream_write_all(out, resp, strlen(resp), NULL, NULL, NULL);
    g_output_stream_flush(out, NULL, NULL);
    g_free(resp);
}

static gboolean onIncoming(GSocketService *service, GSocketConnection *conn, GObject *sourceObject, gpointer userData)
{
    (void) service;
    (void) sourceObject;
    CameraMediaServer *self = (CameraMediaServer *) userData;

    GInputStream *in = g_io_stream_get_input_stream(G_IO_STREAM(conn));
    gchar buf[2048];
    gssize n = g_input_stream_read(in, buf, sizeof(buf) - 1, NULL, NULL);

    if (n <= 0)
    {
        return FALSE;
    }

    buf[n] = '\0';

    // Only GET is supported; pull the request-target out of "GET <path> HTTP/1.1".
    gchar path[256] = "/";

    if (strncmp(buf, "GET ", 4) == 0)
    {
        const gchar *p = buf + 4;
        const gchar *sp = strchr(p, ' ');
        gsize len = (sp != NULL) ? (gsize) (sp - p) : 0;

        if (len > 0 && len < sizeof(path))
        {
            memcpy(path, p, len);
            path[len] = '\0';
        }
    }
    else
    {
        return FALSE;
    }

    gchar *query = strchr(path, '?');

    if (query != NULL)
    {
        *query = '\0';
    }

    GOutputStream *out = g_io_stream_get_output_stream(G_IO_STREAM(conn));

    if (strcmp(path, "/stream.mp4") == 0)
    {
        // Drop a viewer whose socket stalls (e.g. a dead tunnel) so it cannot block the
        // pipeline; writes below and in pushBuffer then fail after this timeout instead of
        // hanging.
        GSocket *sock = g_socket_connection_get_socket(conn);

        if (sock != NULL)
        {
            g_socket_set_timeout(sock, 10);
        }

        static const gchar *hdr = "HTTP/1.1 200 OK\r\n"
                                  "Content-Type: video/mp4\r\n"
                                  "Cache-Control: no-cache, no-store\r\n"
                                  "Connection: close\r\n\r\n";

        // Copy the init segment under the lock, then write header + init outside it so a slow
        // socket never blocks the lock. Register as a viewer only after the init is sent, so a
        // fragment from pushBuffer can never be interleaved ahead of the init bytes.
        g_mutex_lock(&self->mutex);
        GBytes *init = g_bytes_new(self->initSegment->data, self->initSegment->len);
        g_mutex_unlock(&self->mutex);

        gboolean ok = g_output_stream_write_all(out, hdr, strlen(hdr), NULL, NULL, NULL);
        gsize initLen = 0;
        const guint8 *initData = g_bytes_get_data(init, &initLen);

        if (ok && initLen > 0)
        {
            ok = g_output_stream_write_all(out, initData, initLen, NULL, NULL, NULL);
        }

        g_bytes_unref(init);

        if (!ok)
        {
            return FALSE;
        }

        g_mutex_lock(&self->mutex);
        self->clients = g_list_prepend(self->clients, g_object_ref(conn));
        guint viewerCount = g_list_length(self->clients);
        g_mutex_unlock(&self->mutex);

        emitOutput("[camera-stream] Viewer connected (%u active)\n", viewerCount);

        // Ask for a fresh keyframe so this viewer can begin decoding without waiting for the
        // camera's periodic keyframe. Invoked outside the mutex; the handler must not re-enter.
        if (self->onViewer != NULL)
        {
            self->onViewer(self->onViewerData);
        }

        // Claim the connection: our clients-list ref keeps it alive after this handler returns.
        return TRUE;
    }

    servePlayerPage(out);

    return FALSE;
}

static gpointer serverThread(gpointer data)
{
    CameraMediaServer *self = (CameraMediaServer *) data;

    g_main_context_push_thread_default(self->context);

    GError *error = NULL;
    GInetAddress *inet = g_inet_address_new_from_string(self->bindHost);

    if (inet == NULL)
    {
        inet = g_inet_address_new_loopback(G_SOCKET_FAMILY_IPV4);
    }

    GSocketAddress *addr = g_inet_socket_address_new(inet, self->port);
    g_object_unref(inet);

    self->service = g_socket_service_new();
    gboolean bound = g_socket_listener_add_address(
        G_SOCKET_LISTENER(self->service), addr, G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_TCP, NULL, NULL, &error);
    g_object_unref(addr);

    g_mutex_lock(&self->startMutex);
    self->started = TRUE;
    self->startFailed = !bound;
    g_cond_signal(&self->startCond);
    g_mutex_unlock(&self->startMutex);

    if (!bound)
    {
        emitError("[camera-stream] media server failed to bind %s:%u: %s\n",
                  self->bindHost,
                  self->port,
                  error != NULL ? error->message : "unknown error");
        g_clear_error(&error);
        g_main_context_pop_thread_default(self->context);

        return NULL;
    }

    g_signal_connect(self->service, "incoming", G_CALLBACK(onIncoming), self);
    g_socket_service_start(self->service);

    g_main_loop_run(self->loop);

    g_socket_service_stop(self->service);
    g_main_context_pop_thread_default(self->context);

    return NULL;
}

CameraMediaServer *cameraMediaServerCreate(const gchar *bindHost, guint16 port)
{
    CameraMediaServer *self = g_new0(CameraMediaServer, 1);
    self->bindHost = g_strdup(bindHost != NULL ? bindHost : "127.0.0.1");
    self->port = port;
    self->url = g_strdup_printf("http://%s:%u", self->bindHost, port);
    self->initSegment = g_byte_array_new();
    g_mutex_init(&self->mutex);
    g_mutex_init(&self->startMutex);
    g_cond_init(&self->startCond);
    self->context = g_main_context_new();
    self->loop = g_main_loop_new(self->context, FALSE);

    self->thread = g_thread_new("cam-media-server", serverThread, self);

    g_mutex_lock(&self->startMutex);

    while (!self->started)
    {
        g_cond_wait(&self->startCond, &self->startMutex);
    }

    gboolean failed = self->startFailed;
    g_mutex_unlock(&self->startMutex);

    if (failed)
    {
        cameraMediaServerDestroy(self);

        return NULL;
    }

    return self;
}

void cameraMediaServerPushBuffer(CameraMediaServer *self, const guint8 *data, gsize size, gboolean isHeader)
{
    (void) isHeader; // the mp4mux HEADER flag is unreliable; detect boxes instead

    if (self == NULL || data == NULL || size == 0)
    {
        return;
    }

    g_mutex_lock(&self->mutex);

    // The streamable fragmented MP4 begins with the init segment (ftyp + moov) and is then a
    // series of fragments, each starting with a styp or moof box. Everything before the first
    // fragment is the init segment; cache it (for viewers that connect later) and do not stream
    // it as a live fragment. mp4mux emits box-aligned buffers, so the box type at the buffer
    // start identifies the transition.
    GList *snapshot = NULL;

    if (!self->initComplete)
    {
        gboolean isFragmentStart =
            (size >= 8 && (memcmp(data + 4, "moof", 4) == 0 || memcmp(data + 4, "styp", 4) == 0));

        if (isFragmentStart)
        {
            self->initComplete = TRUE;
        }
        else
        {
            g_byte_array_append(self->initSegment, data, size);
            g_mutex_unlock(&self->mutex);

            return;
        }
    }

    // Snapshot the viewer list, then write outside the lock so a slow/stalled viewer cannot
    // block new connections or other viewers.
    snapshot = g_list_copy_deep(self->clients, (GCopyFunc) g_object_ref, NULL);
    g_mutex_unlock(&self->mutex);

    GList *failed = NULL;

    for (GList *it = snapshot; it != NULL; it = it->next)
    {
        GSocketConnection *conn = (GSocketConnection *) it->data;
        GOutputStream *out = g_io_stream_get_output_stream(G_IO_STREAM(conn));

        if (!g_output_stream_write_all(out, data, size, NULL, NULL, NULL))
        {
            failed = g_list_prepend(failed, conn);
        }
    }

    // Remove any viewers whose write failed (disconnected or timed out).
    if (failed != NULL)
    {
        g_mutex_lock(&self->mutex);

        for (GList *it = failed; it != NULL; it = it->next)
        {
            GList *link = g_list_find(self->clients, it->data);

            if (link != NULL)
            {
                self->clients = g_list_delete_link(self->clients, link);
                g_object_unref(it->data); // release the clients-list reference
            }
        }

        g_mutex_unlock(&self->mutex);
        g_list_free(failed);
    }

    g_list_free_full(snapshot, g_object_unref);
}

const gchar *cameraMediaServerGetUrl(CameraMediaServer *self)
{
    return (self != NULL) ? self->url : NULL;
}

void cameraMediaServerSetOnViewer(CameraMediaServer *self, CameraMediaServerOnViewer onViewer, gpointer userData)
{
    if (self == NULL)
    {
        return;
    }

    self->onViewer = onViewer;
    self->onViewerData = userData;
}

void cameraMediaServerDestroy(CameraMediaServer *self)
{
    if (self == NULL)
    {
        return;
    }

    if (self->loop != NULL)
    {
        g_main_loop_quit(self->loop);
    }

    if (self->thread != NULL)
    {
        g_thread_join(self->thread);
    }

    g_mutex_lock(&self->mutex);

    for (GList *it = self->clients; it != NULL; it = it->next)
    {
        GSocketConnection *conn = (GSocketConnection *) it->data;
        g_io_stream_close(G_IO_STREAM(conn), NULL, NULL);
        g_object_unref(conn);
    }

    g_list_free(self->clients);
    self->clients = NULL;
    g_mutex_unlock(&self->mutex);

    g_clear_object(&self->service);

    if (self->loop != NULL)
    {
        g_main_loop_unref(self->loop);
    }

    if (self->context != NULL)
    {
        g_main_context_unref(self->context);
    }

    if (self->initSegment != NULL)
    {
        g_byte_array_free(self->initSegment, TRUE);
    }

    g_free(self->bindHost);
    g_free(self->url);
    g_mutex_clear(&self->mutex);
    g_mutex_clear(&self->startMutex);
    g_cond_clear(&self->startCond);
    g_free(self);
}
