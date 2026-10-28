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
 * Abstract camera stream backend.
 *
 * The camera stream command works only with this abstraction; the concrete implementation is
 * selected by the camera's streaming technology (the protocol its data model reports from the
 * stream springboard — e.g. "webrtc" or "onvif"). Each backend encapsulates its own protocol
 * state and drives the shared context (session + output sink) to deliver media.
 */

#pragma once

#include "cameraStreamContext.h"
#include <glib.h>
#include <stdbool.h>

typedef struct CameraStreamBackend CameraStreamBackend;

/**
 * Command-line stream options, expressed as technology-neutral camera capabilities: credentials
 * and a still-capture target. A backend applies whichever its technology supports.
 */
typedef struct
{
    const gchar *user;         // camera credential (interim mechanism)
    const gchar *pass;         // camera credential (interim mechanism)
    const gchar *snapshotPath; // still-capture target: take a picture and save the image here
} CameraStreamOptions;

struct CameraStreamBackend
{
    /**
     * Run the protocol-specific stream to completion. The device session and output sink are
     * reached through @p ctx. Blocks until teardown or the camera ends the session.
     *
     * @return true on a clean run
     */
    bool (*run)(CameraStreamBackend *self, CameraStreamContext *ctx);

    /** Release the backend and its protocol state. */
    void (*destroy)(CameraStreamBackend *self);
};

/**
 * Create the backend for a stream protocol (e.g. "webrtc", "onvif"). A NULL/empty protocol
 * defaults to WebRTC. Returns NULL (after emitting an error) for an unsupported protocol.
 *
 * @param protocol the technology the camera reported from the stream springboard
 * @param options  command-line options the backend may consult
 * @return the backend, or NULL if unsupported
 */
CameraStreamBackend *cameraStreamBackendCreate(const gchar *protocol, const CameraStreamOptions *options);

/**
 * Run the backend's stream. Convenience wrapper over the run vfunc.
 *
 * @param backend the backend
 * @param ctx     the shared context
 * @return true on a clean run
 */
bool cameraStreamBackendRun(CameraStreamBackend *backend, CameraStreamContext *ctx);

/**
 * Destroy a backend.
 *
 * @param backend the backend (may be NULL)
 */
void cameraStreamBackendDestroy(CameraStreamBackend *backend);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(CameraStreamBackend, cameraStreamBackendDestroy)
