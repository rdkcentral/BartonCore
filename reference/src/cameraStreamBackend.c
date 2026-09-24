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

#include "cameraStreamBackend.h"
#include "barton-core-reference-io.h"
#include "cameraOnvifBackend.h"
#include "cameraWebrtcBackend.h"

// Registry mapping a stream protocol (the technology the camera's data model reports from the
// stream springboard) to its backend constructor. Adding a technology is a single entry here plus
// its backend module — the command never learns the concrete types.
typedef struct
{
    const gchar *protocol;
    CameraStreamBackend *(*create)(const CameraStreamOptions *options);
} BackendEntry;

static const BackendEntry BACKENDS[] = {
    {"webrtc", cameraWebrtcBackendCreate},
    { "onvif",  cameraOnvifBackendCreate},
};

CameraStreamBackend *cameraStreamBackendCreate(const gchar *protocol, const CameraStreamOptions *options)
{
    // A camera that does not report a protocol is treated as WebRTC, preserving legacy behavior.
    const gchar *technology = (protocol != NULL && protocol[0] != '\0') ? protocol : "webrtc";

    for (gsize i = 0; i < G_N_ELEMENTS(BACKENDS); i++)
    {
        if (g_strcmp0(BACKENDS[i].protocol, technology) == 0)
        {
            return BACKENDS[i].create(options);
        }
    }

    emitError("[camera-stream] unsupported stream protocol '%s'\n", protocol != NULL ? protocol : "(null)");

    return NULL;
}

bool cameraStreamBackendRun(CameraStreamBackend *backend, CameraStreamContext *ctx)
{
    return backend->run(backend, ctx);
}

void cameraStreamBackendDestroy(CameraStreamBackend *backend)
{
    if (backend != NULL)
    {
        backend->destroy(backend);
    }
}
