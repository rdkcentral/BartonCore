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
// Protocol-specific details are handled by dedicated endpoints. The abstract
// endpoint tells the client which protocol is active and where to go next via
// the return value of the stream execute (not a pushed status event).
//
// Session Lifecycle
// -----------------
// The session interface exposes four execute resources:
//
//   createSession  [execute]  — Allocates a new session and returns a sessionId.
//   stream         [execute]  — Starts streaming for a given sessionId. Returns a
//                               JSON object { protocol, entryPoint } identifying
//                               the active protocol and the resource URI on the
//                               protocol-specific endpoint the client uses next.
//   takePicture    [execute]  — Captures a snapshot (not yet implemented).
//   destroySession [execute]  — Tears down a session and releases resources.
//
// The abstract endpoint carries no protocol-specific signaling and no status
// resource. In-session state, errors, and teardown are signaled on the
// protocol-specific endpoint (see ep/webrtc r/webrtcError below).
//
// Client Flow
// -----------
//   1. Execute createSession         → receive sessionId
//   2. Execute stream with sessionId → receive { protocol, entryPoint }
//   3. Follow entryPoint to the protocol-specific endpoint
//      (e.g., /<id>/ep/webrtc/r/localSdp). Any negotiation details live on that
//      endpoint, not in the abstract stream result; for WebRTC the client reads
//      r/negotiationRole to learn whether it is the 'offerer' (create the SDP
//      offer) or 'answerer' (answer the camera's offer). The backing Matter flow
//      (ProvideOffer vs SolicitOffer) is hidden from the client.
//   4. Complete protocol-specific exchange (SDP, ICE, media URL, etc.) and
//      subscribe to the protocol endpoint's event resources
//   5. Execute destroySession with sessionId when finished
//
// Session State
// -------------
// Sessions are stored in transient data as a JSON object keyed by sessionId.
// Each session tracks its current state and protocol. Transient data entries
// expire after one hour (ONE_HOUR_SECS) as a leak-prevention backstop, but
// clients are expected to call destroySession for proper cleanup.
//
// Protocol Abstraction
// --------------------
// The camera endpoint does not know or care which streaming protocol is in use.
// The protocol identifier (e.g., "webrtc", "direct") is stored per session and
// returned by the stream execute so clients can identify the technology.
// Currently, this driver hardcodes PROTO_WEBRTC for Matter cameras. Other camera
// technologies would have their own SBMD drivers that create the same abstract
// camera endpoint with a different protocol identifier.
//
// See also: openspec/changes/archive/2026-05-04-camera-architecture-redesign/
// =============================================================================

SbmdDriver({
    schemaVersion: '5.0',
    driverVersion: 1,
    name: 'Camera',

    constants: {
        // Endpoint
        EP_CAMERA: 'camera',
        EP_WEBRTC: 'webrtc',

        // Clusters
        CL_WEBRTC_TRANSPORT_PROVIDER: 0x0553,
        CL_WEBRTC_TRANSPORT_REQUESTOR: 0x0554,
        CL_CAMERA_AV_STREAM_MGMT: 0x0551,

        // CameraAVStreamManagement feature bits
        FEAT_WATERMARK: 0x40,
        FEAT_OSD: 0x80,

        // CameraAVStreamManagement commands
        CMD_VIDEO_STREAM_ALLOCATE: 0x03,
        CMD_VIDEO_STREAM_ALLOCATE_RESP: 0x04,

        // WebRTCTransportProvider commands (outgoing — sent to camera)
        CMD_SOLICIT_OFFER: 0x00,
        CMD_SOLICIT_OFFER_RESP: 0x01,
        CMD_PROVIDE_OFFER: 0x02,
        CMD_PROVIDE_OFFER_RESP: 0x03,
        CMD_PROVIDE_ANSWER: 0x04,
        CMD_PROVIDE_ICE: 0x05,
        CMD_END_SESSION: 0x06,

        // Global attribute: AcceptedCommandList (0xFFF9) — used to pick the signaling flow.
        ATTR_ACCEPTED_COMMAND_LIST: 0xfff9,

        // WebRTCTransportRequestor commands (incoming — from camera)
        CMD_OFFER: 0x00,
        CMD_ANSWER: 0x01,
        CMD_ICE_CANDIDATES: 0x02,
        CMD_END: 0x03,

        // The Matter endpoint on THIS node (Barton) that hosts the WebRTCTransportRequestor server
        // cluster. It is sent to the camera as originatingEndpointID in SolicitOffer/ProvideOffer so
        // the camera knows where to deliver its Offer/Answer/ICE/End commands. Real cameras reject
        // the root endpoint (0), so the requestor is hosted on endpoint 1 (see
        // third_party/matter/barton-library/barton-common/barton-library.zap and .matter).
        // TODO: detect this at runtime from the local node's hosted-cluster map (e.g. walk the
        // root Descriptor PartsList and each endpoint's ServerList for CL_WEBRTC_TRANSPORT_REQUESTOR)
        // instead of hardcoding, so the driver stays correct if the requestor endpoint changes.
        WEBRTC_REQUESTOR_ENDPOINT_ID: 1,

        // Negotiation role conveyed to the client in the stream() result. The client uses it to
        // drive its WebRTC peer; the Matter command mapping (ProvideOffer vs SolicitOffer +
        // ProvideAnswer) stays entirely inside this driver.
        //   'offerer'  — client creates the SDP offer      (ProvideOffer flow)
        //   'answerer' — client answers the camera's offer (SolicitOffer flow)
        ROLE_OFFERER: 'offerer',
        ROLE_ANSWERER: 'answerer',

        // webrtcError event values (ep/webrtc r/webrtcError)
        WEBRTC_ERROR_ENDED: 'ended',
        WEBRTC_ERROR_FAILED: 'failed',

        // Transient data keys
        TD_SESSIONS: 'sessions',
        TD_NEXT_SESSION_ID: 'nextSessionId',

        // Protocol identifiers
        PROTO_WEBRTC: 'webrtc',

        // Timing
        ONE_HOUR_SECS: 3600
    },

    barton: {deviceClass: 'camera', deviceClassVersion: 1},

    matter: {
        deviceTypes: [0x0142],
        revision: 1,
        featureClusters: [CL_WEBRTC_TRANSPORT_PROVIDER, CL_CAMERA_AV_STREAM_MGMT]
    },

    reporting: {minSecs: 1, maxSecs: ONE_HOUR_SECS},

    aliases: {
        incomingOffer: {clusterId: CL_WEBRTC_TRANSPORT_REQUESTOR, commandId: CMD_OFFER},
        incomingAnswer: {clusterId: CL_WEBRTC_TRANSPORT_REQUESTOR, commandId: CMD_ANSWER},
        incomingIceCandidates: {
            clusterId: CL_WEBRTC_TRANSPORT_REQUESTOR,
            commandId: CMD_ICE_CANDIDATES
        },
        incomingEnd: {clusterId: CL_WEBRTC_TRANSPORT_REQUESTOR, commandId: CMD_END},
        providerAcceptedCommands: {
            clusterId: CL_WEBRTC_TRANSPORT_PROVIDER,
            attributeId: ATTR_ACCEPTED_COMMAND_LIST
        }
    },

    endpoints: {
        camera: {
            profile: 'camera',
            profileVersion: 1,
            resources: {
                createSession: {
                    type: 'function',

                    execute: {
                        supplements: {transientData: [TD_SESSIONS, TD_NEXT_SESSION_ID]},
                        handler: executeCreateSession
                    }
                },

                stream: {
                    type: 'function',

                    execute: {
                        supplements: {transientData: [TD_SESSIONS]},
                        handler: executeStream
                    }
                },

                takePicture: {type: 'function', execute: executeTakePicture},

                destroySession: {
                    type: 'function',

                    execute: {
                        supplements: {transientData: [TD_SESSIONS]},
                        handler: executeDestroySession
                    }
                }
            }
        },
        webrtc: {
            profile: 'webrtc',
            profileVersion: 1,
            resources: {
                localSdp: {
                    type: 'function',

                    execute: {
                        supplements: {
                            transientData: [TD_SESSIONS],
                            attributes: ['providerAcceptedCommands']
                        },
                        handler: executeLocalSdp
                    }
                },

                negotiationRole: {
                    type: 'string',
                    modes: ['read'],

                    read: {
                        supplements: {attributes: ['providerAcceptedCommands']},
                        handler: readNegotiationRole
                    }
                },

                remoteSdp: {type: 'string', modes: []},

                localIceCandidates: {
                    type: 'function',

                    execute: {
                        supplements: {transientData: [TD_SESSIONS]},
                        handler: executeLocalIceCandidates
                    }
                },

                remoteIceCandidates: {type: 'string', modes: []},

                webrtcError: {
                    type: 'string',
                    // 'volatile' registers this as non-cached so every updateResource emits an
                    // event even when the value is unchanged (e.g. two 'failed' across sessions).
                    modes: ['volatile']
                }
            }
        }
    },

    attributeHandlers: {},

    commandHandlers: {
        handleIncomingOffer: {
            aliases: ['incomingOffer'],
            supplements: {transientData: [TD_SESSIONS]},
            handler: handleIncomingOffer
        },
        handleIncomingAnswer: {
            aliases: ['incomingAnswer'],
            supplements: {transientData: [TD_SESSIONS]},
            handler: handleIncomingAnswer
        },
        handleIncomingIceCandidates: {
            aliases: ['incomingIceCandidates'],
            supplements: {transientData: [TD_SESSIONS]},
            handler: handleIncomingIceCandidates
        },
        handleIncomingEnd: {
            aliases: ['incomingEnd'],
            supplements: {transientData: [TD_SESSIONS]},
            handler: handleIncomingEnd
        }
    }
});

// =============================================================================
// Handler Functions
// =============================================================================

function parseSessions(sessionsJson) {
    if (!sessionsJson) {
        return {};
    }

    try {
        var parsed = JSON.parse(sessionsJson);

        if (parsed === null || typeof parsed !== 'object' || Array.isArray(parsed)) {
            return null;
        }

        return parsed;
    } catch (e) {
        return null;
    }
}

function executeCreateSession(args) {
    var sessionsJson = args.supplements.transientData[TD_SESSIONS];
    var sessions = parseSessions(sessionsJson);

    if (sessions === null) {
        return Sbmd.result()
            .storage.setTransientData(TD_SESSIONS, '', 0)
            .error('Corrupt session data. Sessions data reset.');
    }

    var nextIdStr = args.supplements.transientData[TD_NEXT_SESSION_ID];
    var nextId = nextIdStr ? parseInt(nextIdStr, 10) : NaN;

    if (isNaN(nextId) || nextId < 1) {
        nextId = 1;

        for (var id in sessions) {
            var n = parseInt(id, 10);

            if (!isNaN(n) && n >= nextId) {
                nextId = n + 1;
            }
        }
    }
    var sessionId = nextId.toString();

    sessions[sessionId] = {state: 'created', protocol: PROTO_WEBRTC};

    return Sbmd.result()
        .storage.setTransientData(TD_SESSIONS, JSON.stringify(sessions), ONE_HOUR_SECS)
        .storage.setTransientData(TD_NEXT_SESSION_ID, (nextId + 1).toString(), ONE_HOUR_SECS)
        .success(sessionId);
}

function pickNegotiationRole(args) {
    // Choose the WebRTC signaling flow from the camera's advertised capabilities. Prefer
    // SolicitOffer (camera generates the offer, client answers) since real cameras favor it; use
    // ProvideOffer (client offers) only when SolicitOffer is not accepted. Default to SolicitOffer
    // when the AcceptedCommandList is unavailable.
    var raw =
        args.supplements && args.supplements.attributes
            ? args.supplements.attributes.providerAcceptedCommands
            : null;

    // Attribute supplements are delivered as base64-encoded TLV (see MakeAttrFetcher). The
    // AcceptedCommandList is a TLV array, which decodes to a plain array of command IDs. Decode
    // defensively: any failure leaves 'accepted' null and falls through to the SolicitOffer default.
    var accepted = null;

    if (raw) {
        try {
            accepted = Sbmd.Tlv.decode(raw);
        } catch (e) {
            accepted = null;
        }
    }

    if (Array.isArray(accepted)) {
        if (accepted.indexOf(CMD_SOLICIT_OFFER) !== -1) {
            return ROLE_ANSWERER;
        }

        if (accepted.indexOf(CMD_PROVIDE_OFFER) !== -1) {
            return ROLE_OFFERER;
        }
    }

    return ROLE_ANSWERER;
}

function readNegotiationRole(args) {
    // Expose the WebRTC negotiation role ('offerer' | 'answerer') to the client. It is derived from
    // the camera's advertised WebRTCTransportProvider commands, so it lives here on the webrtc
    // endpoint rather than in the abstract stream result.
    return Sbmd.result().success(pickNegotiationRole(args));
}

function executeStream(args) {
    var input = args.resource.input;

    if (!input) {
        return Sbmd.result().error('sessionId required');
    }

    var sessionId = input.toString();

    var sessionsJson = args.supplements.transientData[TD_SESSIONS];
    var sessions = parseSessions(sessionsJson);

    if (sessions === null) {
        return Sbmd.result()
            .storage.setTransientData(TD_SESSIONS, '', 0)
            .error('Corrupt session data. Sessions data reset.');
    }

    if (!sessions[sessionId]) {
        return Sbmd.result().error('unknown sessionId: ' + sessionId);
    }

    sessions[sessionId].state = 'streaming';

    var deviceId = args.deviceUuid;
    var entryPoint = '/' + deviceId + '/ep/webrtc/r/localSdp';

    // The abstract stream result only names the active protocol and where to begin signaling.
    // Everything protocol-specific — including the WebRTC negotiation role (read from
    // ep/webrtc r/negotiationRole) and the ProvideOffer/SolicitOffer/ProvideAnswer commands — stays
    // on the webrtc endpoint, so this command carries no WebRTC concepts.
    var streamInfo = {
        protocol: sessions[sessionId].protocol,
        entryPoint: entryPoint
    };

    var result = Sbmd.result().storage.setTransientData(
        TD_SESSIONS,
        JSON.stringify(sessions),
        ONE_HOUR_SECS
    );

    return result.success(JSON.stringify(streamInfo));
}

function executeTakePicture(args) {
    // TODO: implement snapshot capture via CameraAvStreamManagement
    return Sbmd.result().error('takePicture not yet implemented');
}

function executeDestroySession(args) {
    var input = args.resource.input;

    if (!input) {
        return Sbmd.result().error('sessionId required');
    }

    var sessionId = input.toString();

    var sessionsJson = args.supplements.transientData[TD_SESSIONS];
    var sessions = parseSessions(sessionsJson);

    if (sessions === null) {
        return Sbmd.result()
            .storage.setTransientData(TD_SESSIONS, '', 0)
            .error('Corrupt session data. Sessions data reset.');
    }

    if (!sessions[sessionId]) {
        return Sbmd.result().error('unknown sessionId: ' + sessionId);
    }

    var wasStreaming =
        sessions[sessionId].state === 'streaming' &&
        sessions[sessionId].webRTCSessionID !== undefined;
    var webRTCSessionID = sessions[sessionId].webRTCSessionID;

    delete sessions[sessionId];

    var result = Sbmd.result().storage.setTransientData(
        TD_SESSIONS,
        JSON.stringify(sessions),
        ONE_HOUR_SECS
    );

    if (wasStreaming) {
        var endSchema = {
            webRTCSessionID: {tag: 0, type: 'uint16'},
            reason: {tag: 1, type: 'enum8'}
        };
        var endPayload = Sbmd.Tlv.encodeStruct(
            {webRTCSessionID: webRTCSessionID, reason: 2},
            endSchema
        );

        return result.device.sendCommand(CL_WEBRTC_TRANSPORT_PROVIDER, CMD_END_SESSION, endPayload);
    }

    return result.success();
}

// =============================================================================
// WebRTC Endpoint Execute Handlers
// =============================================================================

function executeLocalSdp(args) {
    var sessionsJson = args.supplements.transientData[TD_SESSIONS];
    var sessions = parseSessions(sessionsJson);

    if (sessions === null) {
        return Sbmd.result().error('No active sessions');
    }

    // Find the active streaming session
    var sessionId = null;

    for (var id in sessions) {
        if (sessions[id].state === 'streaming') {
            sessionId = id;
            break;
        }
    }

    if (!sessionId) {
        return Sbmd.result().error('No active streaming session');
    }

    var input = args.resource.input;
    var sdp = input ? input.toString() : '';
    var role = pickNegotiationRole(args);

    if (role === ROLE_ANSWERER) {
        // SolicitOffer flow. Route by how far negotiation has progressed rather than by the SDP
        // payload: until the camera has offered there is no webRTCSessionID, so this call opens the
        // flow (allocate a stream, then SolicitOffer in handleAllocateForSolicit). Once the camera's
        // offer has arrived (webRTCSessionID recorded, remoteSdp emitted) the next call carries our
        // answer, which we relay via ProvideAnswer.
        var haveCameraSession =
            sessions[sessionId].webRTCSessionID !== undefined &&
            sessions[sessionId].webRTCSessionID !== null;

        if (!haveCameraSession) {
            return allocateThenSolicitOffer(args, sessions, sessionId);
        }

        return sendProvideAnswer(sessions, sessionId, sdp);
    }

    // ProvideOffer flow: this local SDP is our offer — allocate a stream, then send it directly.
    if (sdp === '') {
        return Sbmd.result().error('SDP string required');
    }

    return allocateThenProvideOffer(args, sessions, sessionId, sdp);
}

function sendProvideAnswer(sessions, sessionId, sdp) {
    var webRTCSessionID = sessions[sessionId].webRTCSessionID;

    if (webRTCSessionID === undefined || webRTCSessionID === null) {
        return Sbmd.result().error('No webRTCSessionID (camera offer not yet received)');
    }

    var schema = {
        webRTCSessionID: {tag: 0, type: 'uint16'},
        sdp: {tag: 1, type: 'string'}
    };

    var payload = Sbmd.Tlv.encodeStruct({webRTCSessionID: webRTCSessionID, sdp: sdp}, schema);

    return Sbmd.result().device.sendCommand(
        CL_WEBRTC_TRANSPORT_PROVIDER,
        CMD_PROVIDE_ANSWER,
        payload
    );
}

function allocateThenSolicitOffer(args, sessions, sessionId) {
    // SolicitOffer flow, step 1: allocate a video stream (the camera requires one before it will
    // honor SolicitOffer). handleAllocateForSolicit then sends SolicitOffer for the allocated
    // stream. Both legs use requestCommand so their promises stay alive across the deferred chain.
    var featureMap = args.clusterFeatureMaps[CL_CAMERA_AV_STREAM_MGMT] || 0;
    var allocPayload = buildVideoStreamAllocatePayload(featureMap);

    return Sbmd.result()
        .storage.setTransientData(TD_SESSIONS, JSON.stringify(sessions), ONE_HOUR_SECS)
        .device.requestCommand(CL_CAMERA_AV_STREAM_MGMT, CMD_VIDEO_STREAM_ALLOCATE, allocPayload, {
            responseCommandId: CMD_VIDEO_STREAM_ALLOCATE_RESP,
            onResponse: handleAllocateForSolicit,
            onError: handleVideoStreamAllocateError,
            context: {sessionId: sessionId, sessions: sessions},
            timeoutMs: 5000
        });
}

function buildVideoStreamAllocatePayload(featureMap) {
    // Build a VideoStreamAllocate payload. The camera requires an allocated stream before either
    // ProvideOffer or SolicitOffer will succeed, so both negotiation flows share this builder.
    var allocSchema = {
        streamUsage: {tag: 0, type: 'enum8'},
        videoCodec: {tag: 1, type: 'enum8'},
        minFrameRate: {tag: 2, type: 'uint16'},
        maxFrameRate: {tag: 3, type: 'uint16'},
        minResolution: {tag: 4, type: 'struct'},
        maxResolution: {tag: 5, type: 'struct'},
        minBitRate: {tag: 6, type: 'uint32'},
        maxBitRate: {tag: 7, type: 'uint32'},
        keyFrameInterval: {tag: 8, type: 'uint16'}
    };

    // The requested parameters must fall within a stream configuration the
    // camera supports, otherwise VideoStreamAllocate is rejected with
    // DYNAMIC_CONSTRAINT_ERROR. These values target a widely-supported H.264
    // configuration: VGA-to-720p resolution, a single steady frame rate, a
    // conservative bit-rate ceiling, and the spec-recommended 4s key-frame
    // interval (expressed in milliseconds).
    var allocData = {
        streamUsage: 3,
        videoCodec: 0,
        minFrameRate: 30,
        maxFrameRate: 30,
        minResolution: {0: 640, 1: 480},
        maxResolution: {0: 1280, 1: 720},
        minBitRate: 10000,
        maxBitRate: 2000000,
        keyFrameInterval: 4000
    };

    // If the camera supports Watermark or OSD, those fields are mandatory.
    if (featureMap & FEAT_WATERMARK) {
        allocSchema.watermarkEnabled = {tag: 9, type: 'bool'};
        allocData.watermarkEnabled = false;
    }

    if (featureMap & FEAT_OSD) {
        allocSchema.OSDEnabled = {tag: 10, type: 'bool'};
        allocData.OSDEnabled = false;
    }

    return Sbmd.Tlv.encodeStruct(allocData, allocSchema);
}

function allocateThenProvideOffer(args, sessions, sessionId, sdp) {
    // Step 1: Allocate a video stream on the camera.
    // The camera requires an allocated stream before ProvideOffer will succeed.
    var featureMap = args.clusterFeatureMaps[CL_CAMERA_AV_STREAM_MGMT] || 0;
    var allocPayload = buildVideoStreamAllocatePayload(featureMap);

    return Sbmd.result()
        .storage.setTransientData(TD_SESSIONS, JSON.stringify(sessions), ONE_HOUR_SECS)
        .device.requestCommand(CL_CAMERA_AV_STREAM_MGMT, CMD_VIDEO_STREAM_ALLOCATE, allocPayload, {
            responseCommandId: CMD_VIDEO_STREAM_ALLOCATE_RESP,
            onResponse: handleVideoStreamAllocateResponse,
            onError: handleVideoStreamAllocateError,
            context: {sdp: sdp, sessionId: sessionId, sessions: sessions},
            timeoutMs: 5000
        });
}

function handleVideoStreamAllocateResponse(args) {
    var ctx = args.handlerContext;
    var responseData = args.response.data;

    // VideoStreamAllocateResponse: tag 0 = VideoStreamID (uint16)
    var decoded = Sbmd.Tlv.decode(responseData);
    var videoStreamID = decoded[0];

    // Now send ProvideOffer with the allocated video stream ID
    var schema = {
        webRTCSessionID: {tag: 0, type: 'uint16'},
        sdp: {tag: 1, type: 'string'},
        streamUsage: {tag: 2, type: 'enum8'},
        originatingEndpointID: {tag: 3, type: 'uint16'},
        videoStreamID: {tag: 4, type: 'uint16'}
    };

    var tlvBase64 = Sbmd.Tlv.encodeStruct(
        {
            webRTCSessionID: null,
            sdp: ctx.sdp,
            streamUsage: 3,
            originatingEndpointID: WEBRTC_REQUESTOR_ENDPOINT_ID,
            videoStreamID: videoStreamID
        },
        schema
    );

    return Sbmd.result().device.requestCommand(
        CL_WEBRTC_TRANSPORT_PROVIDER,
        CMD_PROVIDE_OFFER,
        tlvBase64,
        {
            responseCommandId: CMD_PROVIDE_OFFER_RESP,
            onResponse: handleProvideOfferResponse,
            onError: handleProvideOfferError,
            context: {sessionId: ctx.sessionId, sessions: ctx.sessions},
            timeoutMs: 10000
        }
    );
}

function handleVideoStreamAllocateError(args) {
    // Async failure after the local SDP was posted: deliver it to the client via the
    // webrtcError event (a bare .error() here has no client channel). Covers timeouts too,
    // since a requestCommand deadline is reported through onError with type 'timeout'.
    var metadata = {
        sessionId: args.handlerContext ? args.handlerContext.sessionId : 'unknown',
        reason: args.error ? args.error.type : 'error',
        detail: 'VideoStreamAllocate failed: ' + (args.error ? args.error.message : 'unknown')
    };

    return Sbmd.result()
        .dataModel.updateResource(EP_WEBRTC, 'webrtcError', WEBRTC_ERROR_FAILED, metadata)
        .success();
}

function handleAllocateForSolicit(args) {
    var ctx = args.handlerContext;
    var responseData = args.response.data;

    // VideoStreamAllocateResponse: tag 0 = VideoStreamID (uint16)
    var decoded = Sbmd.Tlv.decode(responseData);
    var videoStreamID = decoded[0];

    // Record the allocated stream on the session for later reference.
    if (ctx.sessions[ctx.sessionId]) {
        ctx.sessions[ctx.sessionId].videoStreamID = videoStreamID;
    }

    // SolicitOffer: ask the camera to generate the offer for the allocated stream. The camera
    // replies with SolicitOfferResponse (carrying the webRTCSessionID) and then sends its Offer
    // command, which arrives asynchronously as a remoteSdp event. Chaining another requestCommand
    // here (never sendCommand) keeps the deferred operation's promise alive until the response.
    var solicitSchema = {
        streamUsage: {tag: 0, type: 'enum8'},
        originatingEndpointID: {tag: 1, type: 'uint16'},
        videoStreamID: {tag: 2, type: 'uint16'}
    };
    var solicitPayload = Sbmd.Tlv.encodeStruct(
        {
            streamUsage: 3,
            originatingEndpointID: WEBRTC_REQUESTOR_ENDPOINT_ID,
            videoStreamID: videoStreamID
        },
        solicitSchema
    );

    return Sbmd.result()
        .storage.setTransientData(TD_SESSIONS, JSON.stringify(ctx.sessions), ONE_HOUR_SECS)
        .device.requestCommand(CL_WEBRTC_TRANSPORT_PROVIDER, CMD_SOLICIT_OFFER, solicitPayload, {
            responseCommandId: CMD_SOLICIT_OFFER_RESP,
            onResponse: handleSolicitOfferResponse,
            onError: handleSolicitOfferError,
            context: {sessionId: ctx.sessionId, sessions: ctx.sessions},
            timeoutMs: 10000
        });
}

function handleSolicitOfferResponse(args) {
    var ctx = args.handlerContext;
    var responseData = args.response.data;

    // SolicitOfferResponse: tag 0 = WebRTCSessionID (uint16), tag 1 = DeferredOffer (bool).
    // Record the session ID now; the camera's subsequent Offer command also carries it, but
    // capturing it here means ProvideAnswer works even if the two arrive out of order.
    var decoded = Sbmd.Tlv.decode(responseData);
    var webRTCSessionID = decoded[0];

    if (ctx.sessions[ctx.sessionId]) {
        ctx.sessions[ctx.sessionId].webRTCSessionID = webRTCSessionID;
    }

    return Sbmd.result()
        .storage.setTransientData(TD_SESSIONS, JSON.stringify(ctx.sessions), ONE_HOUR_SECS)
        .success();
}

function handleSolicitOfferError(args) {
    // Async failure in the allocate -> SolicitOffer chain: deliver it to the client via the
    // webrtcError event (covers timeouts too, reported as onError type 'timeout').
    var metadata = {
        sessionId: args.handlerContext ? args.handlerContext.sessionId : 'unknown',
        reason: args.error ? args.error.type : 'error',
        detail: 'SolicitOffer failed: ' + (args.error ? args.error.message : 'unknown')
    };

    return Sbmd.result()
        .dataModel.updateResource(EP_WEBRTC, 'webrtcError', WEBRTC_ERROR_FAILED, metadata)
        .success();
}

function handleProvideOfferResponse(args) {
    var ctx = args.handlerContext;
    var responseData = args.response.data;

    // ProvideOfferResponse: tag 0 = WebRTCSessionID (uint16)
    var decoded = Sbmd.Tlv.decode(responseData);
    var webRTCSessionID = decoded[0];

    // Store the allocated WebRTC session ID for later commands (ICE, EndSession).
    ctx.sessions[ctx.sessionId].webRTCSessionID = webRTCSessionID;

    return Sbmd.result()
        .storage.setTransientData(TD_SESSIONS, JSON.stringify(ctx.sessions), ONE_HOUR_SECS)
        .success();
}

function handleProvideOfferError(args) {
    // Async failure after the local SDP was posted: deliver it to the client via the
    // webrtcError event. Covers timeouts too (onError type 'timeout').
    var metadata = {
        sessionId: args.handlerContext ? args.handlerContext.sessionId : 'unknown',
        reason: args.error ? args.error.type : 'error',
        detail: 'ProvideOffer failed: ' + (args.error ? args.error.message : 'unknown')
    };

    return Sbmd.result()
        .dataModel.updateResource(EP_WEBRTC, 'webrtcError', WEBRTC_ERROR_FAILED, metadata)
        .success();
}

function executeLocalIceCandidates(args) {
    var step = 'start';

    try {
        step = 'read-input';
        var input = args.resource.input;

        if (!input) {
            return Sbmd.result().error('ICE candidates JSON array required');
        }

        step = 'json-parse';
        var candidates;

        try {
            candidates = JSON.parse(input.toString());
        } catch (e) {
            return Sbmd.result().error('Invalid JSON: ' + e.message);
        }

        step = 'is-array';

        if (!Array.isArray(candidates)) {
            return Sbmd.result().error('Input must be a JSON array of ICE candidate strings');
        }

        step = 'parse-sessions';
        var sessionsJson = args.supplements.transientData[TD_SESSIONS];
        var sessions = parseSessions(sessionsJson);

        if (sessions === null) {
            return Sbmd.result().error('No active sessions');
        }

        step = 'find-session';
        // Find the active streaming session with a webRTCSessionID
        var webRTCSessionID = null;

        for (var id in sessions) {
            if (sessions[id].state === 'streaming' && sessions[id].webRTCSessionID !== undefined) {
                webRTCSessionID = sessions[id].webRTCSessionID;
                break;
            }
        }

        if (webRTCSessionID === null) {
            return Sbmd.result().error('No active WebRTC session');
        }

        step = 'build-structs';
        // Build ICECandidateStruct array: each element has {candidate, SDPMid, SDPMLineIndex}
        var iceCandidateStructs = [];

        for (var i = 0; i < candidates.length; i++) {
            iceCandidateStructs.push({0: candidates[i], 1: null, 2: null});
        }

        step = 'encode';
        var schema = {
            webRTCSessionID: {tag: 0, type: 'uint16'},
            ICECandidates: {tag: 1, type: 'array'}
        };

        var tlvBase64 = Sbmd.Tlv.encodeStruct(
            {webRTCSessionID: webRTCSessionID, ICECandidates: iceCandidateStructs},
            schema
        );

        step = 'send';

        return Sbmd.result().device.sendCommand(
            CL_WEBRTC_TRANSPORT_PROVIDER,
            CMD_PROVIDE_ICE,
            tlvBase64
        );
    } catch (topErr) {
        return Sbmd.result().error(
            'DIAG-STEP[' +
                step +
                ']: ' +
                (topErr && topErr.message) +
                ' | name=' +
                (topErr && topErr.name) +
                ' | line=' +
                (topErr && topErr.lineNumber)
        );
    }
}

// =============================================================================
// WebRTC Command Handlers (Incoming from Camera)
// =============================================================================

function handleIncomingOffer(args) {
    var tlvBase64 = args.command.tlvBase64;

    if (!tlvBase64) {
        return Sbmd.result().error('No TLV payload in Offer command');
    }

    var decoded = Sbmd.Tlv.decode(tlvBase64);

    // Offer fields: webRTCSessionID (tag 0), sdp (tag 1)
    var webRTCSessionID = decoded[0];
    var sdp = decoded[1];

    if (sdp === undefined || sdp === null) {
        return Sbmd.result().error('Offer command missing SDP');
    }

    // Store the Matter webRTCSessionID for correlation
    var sessionsJson = args.supplements.transientData[TD_SESSIONS];
    var sessions = parseSessions(sessionsJson);

    // Find the active streaming session and associate the Matter session ID
    for (var id in sessions) {
        if (sessions[id].state === 'streaming') {
            sessions[id].webRTCSessionID = webRTCSessionID;
            break;
        }
    }

    return Sbmd.result()
        .storage.setTransientData(TD_SESSIONS, JSON.stringify(sessions), ONE_HOUR_SECS)
        .dataModel.updateResource(EP_WEBRTC, 'remoteSdp', sdp.toString())
        .success();
}

function handleIncomingAnswer(args) {
    var tlvBase64 = args.command.tlvBase64;

    if (!tlvBase64) {
        return Sbmd.result().error('No TLV payload in Answer command');
    }

    var decoded = Sbmd.Tlv.decode(tlvBase64);

    // Answer fields: webRTCSessionID (tag 0), sdp (tag 1)
    var webRTCSessionID = decoded[0];
    var sdp = decoded[1];

    if (sdp === undefined || sdp === null) {
        return Sbmd.result().error('Answer command missing SDP');
    }

    // Store the camera-allocated webRTCSessionID for use by subsequent commands
    // (ProvideICECandidates, EndSession)
    var sessionsJson = args.supplements.transientData[TD_SESSIONS];
    var sessions = parseSessions(sessionsJson);

    if (sessions && webRTCSessionID !== undefined && webRTCSessionID !== null) {
        for (var id in sessions) {
            if (sessions[id].state === 'streaming') {
                sessions[id].webRTCSessionID = webRTCSessionID;
                break;
            }
        }
    }

    return Sbmd.result()
        .storage.setTransientData(TD_SESSIONS, JSON.stringify(sessions || {}), ONE_HOUR_SECS)
        .dataModel.updateResource(EP_WEBRTC, 'remoteSdp', sdp.toString())
        .success();
}

function handleIncomingIceCandidates(args) {
    var tlvBase64 = args.command.tlvBase64;

    if (!tlvBase64) {
        return Sbmd.result().error('No TLV payload in ICECandidates command');
    }

    var decoded = Sbmd.Tlv.decode(tlvBase64);

    // ICECandidates fields: webRTCSessionID (tag 0), ICECandidates (tag 1, array of structs)
    var candidateStructs = decoded[1];

    if (!candidateStructs || !Array.isArray(candidateStructs)) {
        return Sbmd.result().error('ICECandidates command missing candidates array');
    }

    // Extract candidate strings from ICECandidateStruct array
    // Each struct has: candidate (tag 0), SDPMid (tag 1), SDPMLineIndex (tag 2)
    var candidates = [];

    for (var i = 0; i < candidateStructs.length; i++) {
        var cs = candidateStructs[i];
        candidates.push(cs[0] || '');
    }

    return Sbmd.result()
        .dataModel.updateResource(EP_WEBRTC, 'remoteIceCandidates', JSON.stringify(candidates))
        .success();
}

function handleIncomingEnd(args) {
    var tlvBase64 = args.command.tlvBase64;
    var reason = 12; // UnknownReason default

    if (tlvBase64) {
        var decoded = Sbmd.Tlv.decode(tlvBase64);

        // End fields: webRTCSessionID (tag 0), reason (tag 1)
        if (decoded[1] !== undefined) {
            reason = decoded[1];
        }
    }

    // Find and clean up the associated session
    var sessionsJson = args.supplements.transientData[TD_SESSIONS];
    var sessions = parseSessions(sessionsJson);

    var sessionId = null;

    for (var id in sessions) {
        if (sessions[id].state === 'streaming') {
            sessionId = id;
            break;
        }
    }

    if (sessionId) {
        delete sessions[sessionId];
    }

    var metadata = {
        sessionId: sessionId || 'unknown',
        reason: reason,
        detail: 'WebRTC session ended by camera (reason: ' + reason + ')'
    };

    return Sbmd.result()
        .storage.setTransientData(TD_SESSIONS, JSON.stringify(sessions), ONE_HOUR_SECS)
        .dataModel.updateResource(EP_WEBRTC, 'webrtcError', WEBRTC_ERROR_ENDED, metadata)
        .success();
}
