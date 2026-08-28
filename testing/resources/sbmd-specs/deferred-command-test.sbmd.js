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

//
// Deferred Command Test Driver  [TEST ONLY — NOT A PRODUCTION DRIVER]
//
// This driver exists solely to exercise the SBMD requestCommand (deferred)
// code path in integration tests.  It targets the Dimmable Plug-in Unit device
// type (0x010B), which is not claimed by any production SBMD driver, so it
// does not interfere with production commissioning.
//
// The "toggle" execute resource issues a requestCommand for OnOff Toggle
// (cluster 0x0006, command 0x02).  This causes SpecBasedMatterDeviceDriver to
// enter the deferred path and record sbmd.deferred.* metrics.
//
// The driver also subscribes to the OnOff attribute so that the "isOn" resource
// is updated after each toggle, giving tests a reliable wait_for_resource_value
// anchor to confirm end-to-end completion.
//
// TODO: A future production SBMD driver that naturally uses requestCommand can
// replace this test driver for the deferred-metrics integration tests.
//

SbmdDriver({
    schemaVersion: '4.0',
    driverVersion: 1,
    name: 'Deferred Command Test',

    constants: {
        CL_ONOFF: 0x0006,

        // Commands
        CMD_TOGGLE: 0x0002,

        // Attributes
        ATTR_ON_OFF: 0x0000,

        // Resource IDs — original
        RES_IS_ON: 'isOn',
        RES_TOGGLE: 'toggle',

        // Resource IDs — handler behaviour test resources
        RES_THROW_ERROR: 'throwError',
        RES_BUSY_LOOP: 'busyLoop',
        RES_ERROR_RESULT: 'errorResult',
        RES_GC_PRESSURE: 'gcPressure',

        // Resource IDs — deferred edge-case test resources
        RES_REARM_TOGGLE: 'rearmToggle',
        RES_TIMEOUT_TOGGLE: 'timeoutToggle',
        RES_RUNAWAY_TOGGLE: 'runawayToggle'
    },

    barton: {
        deviceClass: 'deferredCmdTest',
        deviceClassVersion: 1
    },

    matter: {
        deviceTypes: [0x010b],
        revision: 1,
        featureClusters: [0x0006]
    },

    reporting: {
        minSecs: 1,
        maxSecs: 3600
    },

    aliases: {
        onOff: {
            clusterId: CL_ONOFF,
            attributeId: ATTR_ON_OFF,
            type: 'bool'
        }
    },

    endpoints: {
        '1': {
            profile: 'deferredCmdTest',
            profileVersion: 1,
            resources: {
                isOn: {
                    type: 'boolean',
                    modes: ['read'],
                    prerequisites: [CL_ONOFF],
                    seed: {
                        supplements: {
                            attributes: ['onOff']
                        },
                        handler: seedIsOn
                    }
                },
                toggle: {
                    type: 'function',
                    execute: executeToggle
                },

                // ── Handler behaviour test resources ──────────────────────
                //
                // These resources exercise specific handler outcome paths so
                // that sbmd.handler.outcome, sbmd.js.exception, and related
                // metrics can be verified in integration tests.
                //
                // IMPORTANT: none of these resources interact with the
                // virtual device; they return (or fail to return) entirely
                // within JS, so no wait_for_resource_value anchor is needed.

                throwError: {
                    type: 'function',
                    execute: executeThrowError
                },

                busyLoop: {
                    type: 'function',
                    execute: executeBusyLoop
                },

                errorResult: {
                    type: 'function',
                    execute: executeErrorResult
                },

                gcPressure: {
                    type: 'function',
                    execute: executeGcPressure
                },

                // ── Deferred edge-case test resources ─────────────────────
                //
                // These resources exercise deeper parts of the deferred path
                // (re-arming, timeouts, and runaway chains) for the
                // sbmd.deferred.depth / .timeout / .max_depth metrics.

                rearmToggle: {
                    type: 'function',
                    execute: executeRearmToggle
                },

                timeoutToggle: {
                    type: 'function',
                    execute: executeTimeoutToggle
                },

                runawayToggle: {
                    type: 'function',
                    execute: executeRunawayToggle
                }
            }
        }
    },

    attributeHandlers: {
        handleOnOff: {aliases: ['onOff'], handler: handleOnOffState}
    }
});

// =============================================================================
// Handler Implementations
// =============================================================================

/**
 * Seeds the Barton isOn resource from the cached Matter OnOff attribute.
 * Defaults to false when the attribute is not yet cached.
 */
function seedIsOn(args) {
    var tlvBase64 = args.supplements.attributes.onOff;
    var decoded = tlvBase64 != null ? Sbmd.Tlv.decode(tlvBase64) : null;
    var value = decoded != null ? decoded : false;

    return Sbmd.result()
        .dataModel.updateResource(args.endpointId, RES_IS_ON, value ? 'true' : 'false')
        .success();
}

/**
 * Issues an OnOff Toggle command via requestCommand to exercise the SBMD
 * deferred code path.  The isOn resource is updated authoritatively by the
 * OnOff attribute subscription after the device reports the new state.
 */
function executeToggle(args) {
    return Sbmd.result().device.requestCommand(CL_ONOFF, CMD_TOGGLE, null, {
        onResponse: handleToggleResponse,
        onError: handleToggleError
    });
}

/**
 * Handles the Matter IM-status response for OnOff Toggle.
 * Toggle carries no application-level response payload; the isOn resource is
 * updated authoritatively by the OnOff attribute subscription.  Return success
 * so the deferred chain completes.
 */
function handleToggleResponse(args) {
    return Sbmd.result().success();
}

/**
 * Handles a failed OnOff Toggle command.
 */
function handleToggleError(args) {
    return Sbmd.result().error(args.error.message);
}

/**
 * Maps Matter OnOff attribute reports to the Barton isOn resource.
 */
function handleOnOffState(args) {
    var value = Sbmd.Tlv.decode(args.attribute.tlvBase64);

    if (value === null) {
        return Sbmd.result().error('TLV decode failed for OnOff');
    }

    return Sbmd.result()
        .dataModel.updateResource(args.endpointId, RES_IS_ON, value ? 'true' : 'false')
        .success();
}

// =============================================================================
// Handler Behaviour Test Resources
// =============================================================================

/**
 * Throws a JavaScript exception to produce:
 *   sbmd.handler.outcome{outcome="exception"}  += 1
 *
 * NOTE: sbmd.js.exception is only for init/loading phases per the spec.
 * Handler-phase exceptions are captured solely by handler.outcome.
 */
function executeThrowError(args) {
    throw new Error('deliberate test exception');
}

/**
 * Busy-loops until the SBMD script timeout fires (~5 s) to produce:
 *   sbmd.handler.outcome{outcome="timeout"}    += 1
 *   sbmd.handler.duration_ms                   in high-latency buckets
 *
 * NOTE: sbmd.js.exception is only for init/loading phases per the spec.
 * Timeout outcomes are captured solely by handler.outcome.
 * This handler takes ~5 s per call.  Tests using it must be marked
 * @pytest.mark.slow and excluded from the default CI run.
 */
function executeBusyLoop(args) {
    /* jshint ignore:start */
    var i = 0;
    while (true) { i++; }
    /* jshint ignore:end */
}

/**
 * Returns a controlled error terminal (not a thrown exception) to produce:
 *   sbmd.handler.outcome{outcome="error"}      += 1
 *
 * sbmd.js.exception must NOT increment for this call.
 */
function executeErrorResult(args) {
    return Sbmd.result().error('deliberate test error result');
}

/**
 * Allocates and discards many temporary objects per call to create GC
 * pressure.  After enough calls, this should cause at least one GC cycle,
 * incrementing sbmd.js.gc.count and recording sbmd.js.gc.duration_ms.
 */
function executeGcPressure(args) {
    for (var i = 0; i < 500; i++) {
        var arr = [];
        for (var j = 0; j < 1000; j++) {
            arr.push(j);
        }
    }

    return Sbmd.result().success();
}

// =============================================================================
// Deferred Edge-Case Test Resources
// =============================================================================

/**
 * Issues requestCommand for Toggle, then re-arms once from onResponse.
 * The complete operation makes two requestCommand round-trips (depth = 1).
 *
 * Expected metrics after one call:
 *   sbmd.deferred.depth{…} count=1, sum=1
 */
function executeRearmToggle(args) {
    return Sbmd.result().device.requestCommand(CL_ONOFF, CMD_TOGGLE, null, {
        onResponse: handleRearmToggleFirstResponse,
        onError: handleToggleError
    });
}

function handleRearmToggleFirstResponse(args) {
    // Re-arm: issue a second Toggle command from within the first response.
    return Sbmd.result().device.requestCommand(CL_ONOFF, CMD_TOGGLE, null, {
        onResponse: handleRearmToggleSecondResponse,
        onError: handleToggleError
    });
}

function handleRearmToggleSecondResponse(args) {
    return Sbmd.result().success();
}

/**
 * Issues requestCommand with a 1 ms overall timeout.  The virtual device
 * always takes longer than 1 ms to respond, so the deadline expires and
 * sbmd.deferred.timeout increments.
 *
 * Expected metrics after one call:
 *   sbmd.deferred.timeout{…}    += 1
 *   sbmd.deferred.in_flight      == 0  (timeout path calls CompletePendingOperation)
 */
function executeTimeoutToggle(args) {
    return Sbmd.result().device.requestCommand(CL_ONOFF, CMD_TOGGLE, null, {
        onResponse: handleToggleResponse,
        onError: handleToggleError,
        timeoutMs: 1
    });
}

/**
 * Always re-arms from onResponse, driving the deferred chain past
 * MAX_DEFERRAL_DEPTH (10) to trigger sbmd.deferred.max_depth.
 *
 * Expected metrics after one call:
 *   sbmd.deferred.max_depth{…}  += 1
 *   sbmd.deferred.in_flight      == 0  (max-depth path calls CompletePendingOperation)
 */
function executeRunawayToggle(args) {
    return Sbmd.result().device.requestCommand(CL_ONOFF, CMD_TOGGLE, null, {
        onResponse: handleRunawayToggleResponse,
        onError: handleToggleError
    });
}

function handleRunawayToggleResponse(args) {
    // Unconditionally re-arm — this chain never terminates voluntarily.
    return Sbmd.result().device.requestCommand(CL_ONOFF, CMD_TOGGLE, null, {
        onResponse: handleRunawayToggleResponse,
        onError: handleToggleError
    });
}

