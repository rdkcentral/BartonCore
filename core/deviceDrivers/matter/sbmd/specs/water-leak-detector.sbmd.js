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
// Water Leak Detector SBMD Driver
//
// Maps Matter Water Leak Detector device type to Barton sensor device class.
// StateValue=true means water detected (faulted=true).
//

SbmdDriver({
    schemaVersion: '5.0',
    driverVersion: 2,
    name: 'Water Leak Detector',

    constants: {
        CL_BOOLEAN_STATE: 0x0045,
        ATTR_STATE_VALUE: 0x0000,
        EVT_STATE_CHANGE: 0x0000,
        RES_FAULTED: 'faulted'
    },

    barton: {
        deviceClass: 'sensor',
        deviceClassVersion: 1
    },

    matter: {
        deviceTypes: [0x0043],
        revision: 1
    },

    reporting: {
        minSecs: 1,
        maxSecs: 3600
    },

    aliases: {
        stateValue: {
            clusterId: CL_BOOLEAN_STATE,
            attributeId: ATTR_STATE_VALUE,
            type: 'bool'
        },
        stateChange: {
            clusterId: CL_BOOLEAN_STATE,
            eventId: EVT_STATE_CHANGE
        }
    },

    endpoints: {
        '1': {
            profile: 'sensor',
            profileVersion: 2,
            resources: {
                faulted: {
                    type: 'com.icontrol.boolean',
                    modes: ['read'],
                    prerequisites: [CL_BOOLEAN_STATE],
                    seed: {
                        supplements: {
                            attributes: ['stateValue']
                        },
                        handler: function (args) {
                            var tlvBase64 = args.supplements.attributes.stateValue;
                            var value = tlvBase64 !== null ? Sbmd.Tlv.decode(tlvBase64) : null;

                            // StateValue=true means water detected (faulted)
                            return Sbmd.result()
                                .dataModel.updateResource(
                                    args.endpointId,
                                    RES_FAULTED,
                                    value === true ? 'true' : 'false'
                                )
                                .success();
                        }
                    }
                }
            }
        }
    },

    eventHandlers: {
        handleStateChange: {
            aliases: ['stateChange'],
            handler: function (args) {
                var fields = Sbmd.Tlv.decode(args.event.tlvBase64);

                if (fields === null) {
                    return Sbmd.result().error('TLV decode failed for StateChange');
                }

                // StateValue=true means water detected (faulted)
                return Sbmd.result()
                    .dataModel.updateResource(
                        args.endpointId,
                        RES_FAULTED,
                        fields[0] === true ? 'true' : 'false'
                    )
                    .success();
            }
        }
    }
});
