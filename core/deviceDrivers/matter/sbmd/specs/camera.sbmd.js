// ------------------------------ tabstop = 4 ----------------------------------
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
// ------------------------------ tabstop = 4 ----------------------------------

// =============================================================================
// Camera SBMD Driver — Abstract Session Interface
// =============================================================================
//
// This driver implements the abstract camera endpoint (profile: "camera") that
// provides a protocol-agnostic session lifecycle for camera streaming. It is
// the client-facing layer in a two-layer architecture:
//
//   ep/camera   (this driver)   — abstract session lifecycle
//   ep/webrtc, ep/direct, ...   — protocol-specific signaling (separate drivers)
//
// Clients interact exclusively with the camera endpoint to manage sessions.
// Protocol-specific details are handled by dedicated endpoints that the client
// is directed to via sessionStatus event metadata.
//
// Session Lifecycle
// -----------------
// The session interface exposes four resources:
//
//   createSession  [execute]  — Allocates a new session and returns a sessionId.
//   stream         [execute]  — Starts streaming for a given sessionId. Emits a
//                               sessionStatus event with status "setup" and
//                               metadata containing the protocol in use and the
//                               nextAction URI on the protocol-specific endpoint.
//   takePicture    [execute]  — Captures a snapshot (not yet implemented).
//   destroySession [execute]  — Tears down a session and releases resources.
//   sessionStatus  [events]   — Emits events to coordinate the multi-step flow.
//                               Not readable — events are the source of truth.
//
// Client Flow
// -----------
//   1. Execute createSession         → receive sessionId
//   2. Execute stream with sessionId → receive sessionStatus "setup" event
//   3. Follow nextAction URI from metadata to the protocol-specific endpoint
//      (e.g., /devices/<id>/ep/webrtc/r/offerSdp)
//   4. Complete protocol-specific exchange (SDP, ICE, media URL, etc.)
//   5. Receive sessionStatus "done" event when streaming is established
//   6. Execute destroySession with sessionId when finished
//
// Session State
// -------------
// Sessions are stored in transient data as a JSON object keyed by sessionId.
// Each session tracks its current state and protocol. Transient data entries
// expire after one hour (ONE_HOUR_SECS) as a leak-prevention backstop, but
// clients are expected to call destroySession for proper cleanup.
//
// The sessionStatus resource is registered with no read modes — it is
// event-only. Multiple sessions may be active simultaneously, each correlated
// by sessionId in the event metadata.
//
// Protocol Abstraction
// --------------------
// The camera endpoint does not know or care which streaming protocol is in use.
// The protocol identifier (e.g., "webrtc", "direct") is stored per session and
// included in sessionStatus metadata so clients can identify the technology.
// Currently, this driver hardcodes PROTO_WEBRTC for Matter cameras. Other camera
// technologies would have their own SBMD drivers that create the same abstract
// camera endpoint with a different protocol identifier.
//
// See also: openspec/changes/archive/2026-05-04-camera-architecture-redesign/
// =============================================================================

SbmdDriver({
    schemaVersion: '4.0',
    driverVersion: 1,
    name: 'Camera',

    constants: {
        // Endpoint
        EP_CAMERA: 'camera',

        // Clusters
        CL_WEBRTC_TRANSPORT_PROVIDER: 0x0553,
        CL_CAMERA_AV_STREAM_MGMT: 0x0551,

        // Session status values
        STATUS_SETUP: 'setup',
        STATUS_DONE: 'done',
        STATUS_ERROR: 'error',

        // Transient data keys
        TD_SESSIONS: 'sessions',
        TD_NEXT_SESSION_ID: 'nextSessionId',

        // Protocol identifiers
        PROTO_WEBRTC: 'webrtc',

        // Timing
        ONE_HOUR_SECS: 3600,
    },

    barton: {
        deviceClass: 'camera',
        deviceClassVersion: 1,
    },

    matter: {
        deviceTypes: [0x0142],
        revision: 1,
        featureClusters: [CL_WEBRTC_TRANSPORT_PROVIDER],
    },

    reporting: {
        minSecs: 1,
        maxSecs: ONE_HOUR_SECS,
    },

    aliases: {},

    endpoints: {
        camera: {
            profile: 'camera',
            profileVersion: 1,
            resources: {
                createSession: {
                    type: 'function',

                    execute: {
                        supplements: {
                            transientData: [TD_SESSIONS, TD_NEXT_SESSION_ID],
                        },
                        handler: executeCreateSession,
                    },
                },

                stream: {
                    type: 'function',

                    execute: {
                        supplements: {
                            transientData: [TD_SESSIONS],
                        },
                        handler: executeStream,
                    },
                },

                takePicture: {
                    type: 'function',
                    execute: executeTakePicture,
                },

                destroySession: {
                    type: 'function',

                    execute: {
                        supplements: {
                            transientData: [TD_SESSIONS],
                        },
                        handler: executeDestroySession,
                    },
                },

                sessionStatus: {
                    type: 'string',
                    modes: [],
                },
            },
        },
    },

    attributeHandlers: {},
});

// =============================================================================
// Handler Functions
// =============================================================================

function parseSessions(sessionsJson)
{
    if (!sessionsJson)
    {
        return {};
    }

    try
    {
        var parsed = JSON.parse(sessionsJson);

        if (parsed === null || typeof parsed !== 'object' || Array.isArray(parsed))
        {
            return null;
        }

        return parsed;
    }
    catch (e)
    {
        return null;
    }
}

function executeCreateSession(args)
{
    var sessionsJson = args.supplements.transientData[TD_SESSIONS];
    var sessions = parseSessions(sessionsJson);

    if (sessions === null)
    {
        return Sbmd.result()
            .storage.setTransientData(TD_SESSIONS, '', 0)
            .error('Corrupt session data. Sessions data reset.');
    }

    var nextIdStr = args.supplements.transientData[TD_NEXT_SESSION_ID];
    var nextId = nextIdStr ? parseInt(nextIdStr, 10) : NaN;

    if (isNaN(nextId) || nextId < 1)
    {
        nextId = 1;

        for (var id in sessions)
        {
            var n = parseInt(id, 10);

            if (!isNaN(n) && n >= nextId)
            {
                nextId = n + 1;
            }
        }
    }
    var sessionId = nextId.toString();

    sessions[sessionId] = {
        state: 'created',
        protocol: PROTO_WEBRTC,
    };

    return Sbmd.result()
        .storage.setTransientData(TD_SESSIONS, JSON.stringify(sessions), ONE_HOUR_SECS)
        .storage.setTransientData(TD_NEXT_SESSION_ID, (nextId + 1).toString(), ONE_HOUR_SECS)
        .success(sessionId);
}

function executeStream(args)
{
    var input = args.resource.input;

    if (!input)
    {
        return Sbmd.result().error('sessionId required');
    }

    var sessionId = input.toString();

    var sessionsJson = args.supplements.transientData[TD_SESSIONS];
    var sessions = parseSessions(sessionsJson);

    if (sessions === null)
    {
        return Sbmd.result()
            .storage.setTransientData(TD_SESSIONS, '', 0)
            .error('Corrupt session data. Sessions data reset.');
    }

    if (!sessions[sessionId])
    {
        return Sbmd.result().error('unknown sessionId: ' + sessionId);
    }

    sessions[sessionId].state = 'streaming';
    sessions[sessionId].action = 'stream';

    var protocol = sessions[sessionId].protocol;
    var deviceId = args.deviceUuid;
    var nextAction = '/devices/' + deviceId + '/ep/webrtc/r/offerSdp';

    var metadata = {
        sessionId: sessionId,
        protocol: protocol,
        nextAction: nextAction,
    };

    return Sbmd.result()
        .storage.setTransientData(TD_SESSIONS, JSON.stringify(sessions), ONE_HOUR_SECS)
        .dataModel.updateResource(EP_CAMERA, 'sessionStatus', STATUS_SETUP, metadata)
        .success();
}

function executeTakePicture(args)
{
    // TODO: implement snapshot capture via CameraAvStreamManagement
    return Sbmd.result().error('takePicture not yet implemented');
}

function executeDestroySession(args)
{
    var input = args.resource.input;

    if (!input)
    {
        return Sbmd.result().error('sessionId required');
    }

    var sessionId = input.toString();

    var sessionsJson = args.supplements.transientData[TD_SESSIONS];
    var sessions = parseSessions(sessionsJson);

    if (sessions === null)
    {
        return Sbmd.result()
            .storage.setTransientData(TD_SESSIONS, '', 0)
            .error('Corrupt session data. Sessions data reset.');
    }

    if (!sessions[sessionId])
    {
        return Sbmd.result().error('unknown sessionId: ' + sessionId);
    }

    delete sessions[sessionId];

    return Sbmd.result().storage.setTransientData(TD_SESSIONS, JSON.stringify(sessions), ONE_HOUR_SECS).success();
}
