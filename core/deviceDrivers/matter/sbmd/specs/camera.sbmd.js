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
        return JSON.parse(sessionsJson);
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
    var nextId = nextIdStr ? parseInt(nextIdStr, 10) : 1;

    if (isNaN(nextId) || nextId < 1)
    {
        nextId = 1;
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
