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

        // Resource IDs
        RES_IS_ON: 'isOn',
        RES_TOGGLE: 'toggle'
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
            type: 'boolean'
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
    var decoded = (tlvBase64 != null) ? Sbmd.Tlv.decode(tlvBase64) : null;
    var value = (decoded != null) ? decoded : false;

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
