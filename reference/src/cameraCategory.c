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
// The camera stream (cs) command. This layer is technology-agnostic: it opens the abstract camera
// session, starts the stream, and hands off to a CameraStreamBackend selected by the technology the
// camera's data model reports (the stream protocol). All protocol-specific state and logic live in
// the backends (see cameraWebrtcBackend / cameraOnvifBackend) behind the CameraStreamBackend
// abstraction, and everything shared (the device session, output sink, and teardown signaling)
// lives in CameraStreamContext.
//

#include "cameraCategory.h"
#include "barton-core-client.h"
#include "barton-core-reference-io.h"
#include "cameraDeviceSession.h"
#include "cameraMediaServer.h"
#include "cameraStreamBackend.h"
#include "cameraStreamContext.h"
#include <signal.h>
#include <string.h>

// CAMERA_DEFAULT_SERVE_HOST / CAMERA_DEFAULT_SERVE_PORT are defined in cameraMediaServer.h so the
// command layer and the media server share the same defaults.

// Set by the SIGINT handler, which must stay async-signal-safe. The context's wait loop polls this
// flag rather than relying on the handler to take locks or signal condition variables.
static volatile sig_atomic_t sigintRequested = 0;

// ============================================================================
// SIGINT handler
// ============================================================================

static void sigintHandler(int sig)
{
    (void) sig;

    // Only touch an async-signal-safe flag here. Taking a GLib mutex or signaling a condition
    // variable from a signal handler is undefined behavior; the wait loop polls this flag instead.
    sigintRequested = 1;
}

// ============================================================================
// Argument parsing
// ============================================================================

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

        guint64 parsedPort = g_ascii_strtoull(colon + 1, NULL, 10);

        if (parsedPort == 0 || parsedPort > G_MAXUINT16)
        {
            return FALSE; // out-of-range or unparseable port
        }
        *servePortOut = (guint16) parsedPort;
    }

    *serveHostOut = (authority[0] != '\0') ? g_strdup(authority) : g_strdup(CAMERA_DEFAULT_SERVE_HOST);

    return (*servePortOut != 0);
}

// ============================================================================
// Command implementation
// ============================================================================

// Open the abstract camera session, start the stream, and drive the technology-specific backend.
// Every handle is scope bound (g_autoptr) so early returns tear everything down in the right order:
// the backend (and its media client) first, then the context (output sink), then the device session
// (which ends the camera session if it was opened).
static bool runCameraStream(BCoreClient *client,
                            const gchar *deviceId,
                            const gchar *filePath,
                            const gchar *serveHost,
                            guint16 servePort,
                            const CameraStreamOptions *options)
{
    g_autoptr(CameraDeviceSession) session = cameraDeviceSessionCreate(client, deviceId);

    if (session == NULL)
    {
        emitError("[camera-stream] Failed to set up camera session\n");

        return false;
    }

    emitOutput("[camera-stream] Creating session...\n");

    if (!cameraDeviceSessionOpen(session))
    {
        emitError("[camera-stream] Failed to create session\n");

        return false;
    }

    emitOutput("[camera-stream] Session created\n");

    emitOutput("[camera-stream] Starting stream...\n");

    if (!cameraDeviceSessionStartStream(session))
    {
        emitError("[camera-stream] Failed to start stream\n");

        return false;
    }

    // The stream springboard reports the technology (protocol) and the entry point to begin on.
    const gchar *protocol = cameraDeviceSessionGetProtocol(session);
    const gchar *entryPoint = cameraDeviceSessionGetEntryPoint(session);
    emitOutput("[camera-stream] Stream started (protocol %s, entry %s)\n",
               protocol != NULL ? protocol : "unknown",
               entryPoint != NULL ? entryPoint : "unknown");

    // Select the backend for the reported technology; unknown protocols are rejected by the factory.
    g_autoptr(CameraStreamBackend) backend = cameraStreamBackendCreate(protocol, options);

    if (backend == NULL)
    {
        return false;
    }

    g_autoptr(CameraStreamContext) ctx =
        cameraStreamContextCreate(session, filePath, serveHost, servePort, &sigintRequested);

    // Session-ended status is shared across technologies, so it is handled by the context.
    cameraDeviceSessionSetStatusCallback(session, cameraStreamContextOnSessionEnded, ctx);

    bool result = cameraStreamBackendRun(backend, ctx);

    // The session callbacks capture ctx / the backend (self), both scope-bound autoptrs freed when
    // this function returns. Clear every session callback before the session (also scope bound) is
    // torn down so a late resource-updated event cannot invoke one against a dangling pointer (UAF).
    cameraDeviceSessionSetStatusCallback(session, NULL, NULL);
    cameraDeviceSessionSetWebrtcCallbacks(session, NULL, NULL, NULL);
    cameraDeviceSessionSetOnvifCallbacks(session, NULL, NULL, NULL);

    return result;
}

static bool cameraStreamFunc(BCoreClient *client, gint argc, gchar **argv)
{
    const gchar *deviceId = argv[0];
    const gchar *outUri = NULL;
    CameraStreamOptions options = {0};
    const gchar *snapshotArg = NULL;

    // Parse optional flags:
    //   --out <uri>        file://<path> records, http://<host>:<port> serves (default: serve)
    //   --user <name>      ONVIF/RTSP username (interim credential mechanism; ONVIF path only)
    //   --pass <secret>    ONVIF/RTSP password (interim credential mechanism; ONVIF path only)
    //   --snapshot <path>  ONVIF path only: take a picture and save the JPEG to file://<path>
    for (gint i = 1; i < argc; i++)
    {
        if (g_strcmp0(argv[i], "--out") == 0 && i + 1 < argc)
        {
            outUri = argv[++i];
        }
        else if (g_strcmp0(argv[i], "--user") == 0 && i + 1 < argc)
        {
            options.user = argv[++i];
        }
        else if (g_strcmp0(argv[i], "--pass") == 0 && i + 1 < argc)
        {
            options.pass = argv[++i];
        }
        else if (g_strcmp0(argv[i], "--snapshot") == 0 && i + 1 < argc)
        {
            snapshotArg = argv[++i];
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

    // The snapshot target is a plain path; accept an optional file:// scheme for symmetry with --out.
    g_autofree gchar *snapshotPath = NULL;

    if (snapshotArg != NULL)
    {
        snapshotPath = g_str_has_prefix(snapshotArg, "file://") ? g_strdup(snapshotArg + strlen("file://"))
                                                                : g_strdup(snapshotArg);

        if (snapshotPath[0] == '\0')
        {
            emitError("Invalid --snapshot path '%s'\n", snapshotArg);

            return false;
        }

        options.snapshotPath = snapshotPath;
    }

    // Verify device exists
    g_autoptr(BCoreDevice) device = b_core_client_get_device_by_id(client, deviceId);

    if (device == NULL)
    {
        emitError("Device '%s' not found\n", deviceId);

        return false;
    }

    // Install signal handler so Ctrl+C requests a graceful teardown. Clear any leftover request
    // from a previous run before arming the handler for this one.
    struct sigaction oldAction;
    struct sigaction newAction = {0};
    newAction.sa_handler = sigintHandler;
    sigemptyset(&newAction.sa_mask);
    sigintRequested = 0;
    sigaction(SIGINT, &newAction, &oldAction);

    bool success = runCameraStream(
        client, deviceId, filePath, serveHost != NULL ? serveHost : CAMERA_DEFAULT_SERVE_HOST, servePort, &options);

    // Restore signal handler
    sigaction(SIGINT, &oldAction, NULL);

    emitOutput("[camera-stream] Done.\n");

    return success;
}

Category *buildCameraCategory(void)
{
    Category *cat = categoryCreate("Camera", "Camera streaming commands");

    Command *command =
        commandCreate("cameraStream",
                      "cs",
                      "<deviceId> [--out <uri>] [--user <name>] [--pass <secret>] [--snapshot <file://path>]",
                      "Stream a camera (WebRTC or ONVIF/RTSP); serve over HTTP (default "
                      "http://127.0.0.1:8088) or record with --out file://<path>. For ONVIF cameras, "
                      "--user/--pass supply interim credentials and --snapshot saves a still image.",
                      1,
                      9,
                      cameraStreamFunc);
    categoryAddCommand(cat, command);

    return cat;
}
