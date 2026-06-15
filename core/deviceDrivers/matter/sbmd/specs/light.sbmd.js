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
// Light SBMD Driver
//
// Maps Matter light device types to Barton light device class.
// Supports On/Off Light, Dimmable Light, Color Temperature Light,
// Extended Color Light, and their switch/plug-in unit variants.
//

SbmdDriver({
    schemaVersion: '4.0',
    driverVersion: '1.0.0',
    name: 'Light',

    constants: {
        // Endpoint
        EP_LIGHT: '1',

        // Clusters
        CL_ON_OFF: 0x0006,
        CL_LEVEL: 0x0008,

        // Attributes
        ATTR_ON_OFF: 0x0000,
        ATTR_CURRENT_LEVEL: 0x0000,

        // Commands
        CMD_OFF: 0x0000,
        CMD_ON: 0x0001,
        CMD_MOVE_TO_LEVEL_WITH_ON_OFF: 0x0004,

        // Resources
        RES_IS_ON: 'isOn',
        RES_CURRENT_LEVEL: 'currentLevel',
    },

    barton: {
        deviceClass: 'light',
        deviceClassVersion: 0,
    },

    matter: {
        deviceTypes: [
            0x0100,  // On/Off Light
            0x010a,  // On/Off Plug-in Unit
            0x0101,  // Dimmable Light
            0x010b,  // Dimmable Plug-in Unit
            0x0102,  // Color Dimmable Light
            0x0200,  // Color Dimmable Light (alternate)
            0x010d,  // Extended Color Light
            0x0210,  // Extended Color Light (alternate)
            0x010c,  // Color Temperature Light
            0x0220,  // Color Temperature Light (alternate)
            0x0103,  // On/Off Light Switch
            0x0104,  // Dimmable Light Switch
            0x0105,  // Color Dimmable Light Switch
        ],
        revision: 1,
        featureClusters: [],
    },

    reporting: {
        minSecs: 1,
        maxSecs: 3600,
    },

    aliases: {
        onOff: {
            clusterId: CL_ON_OFF,
            attributeId: ATTR_ON_OFF,
            type: 'bool',
        },
        currentLevel: {
            clusterId: CL_LEVEL,
            attributeId: ATTR_CURRENT_LEVEL,
            type: 'uint8',
        },
    },

    endpoints: {
        "1": {
            profile: 'light',
            profileVersion: 0,
            resources: {
                isOn: {
                    type: 'boolean',
                    modes: ['read', 'write'],
                    prerequisites: ['onOff'],

                    write: function(args) {
                        var commandId = (args.resource.input === 'true') ? CMD_ON : CMD_OFF;

                        return SbmdUtils.result()
                            .device.sendCommand(CL_ON_OFF, commandId);
                    },
                },

                currentLevel: {
                    type: 'com.icontrol.lightLevel',
                    optional: true,
                    modes: ['read', 'write'],
                    prerequisites: ['currentLevel'],

                    write: function(args) {
                        var percent = parseInt(args.resource.input, 10);

                        if (isNaN(percent)) {
                            percent = 0;
                        }

                        if (percent < 0) {
                            percent = 0;
                        }

                        if (percent > 100) {
                            percent = 100;
                        }

                        // Convert percentage (0-100) to Matter level (0-254)
                        var level = Math.round(percent / 100 * 254);

                        var cmdArgs = {
                            Level: level,
                            TransitionTime: 0,
                            OptionsMask: 0,
                            OptionsOverride: 0,
                        };
                        var schema = {
                            Level: { tag: 0, type: 'uint8' },
                            TransitionTime: { tag: 1, type: 'uint16' },
                            OptionsMask: { tag: 2, type: 'uint8' },
                            OptionsOverride: { tag: 3, type: 'uint8' },
                        };
                        var tlvBase64 = SbmdUtils.Tlv.encodeStruct(cmdArgs, schema);

                        return SbmdUtils.result()
                            .device.sendCommand(CL_LEVEL, CMD_MOVE_TO_LEVEL_WITH_ON_OFF, tlvBase64);
                    },
                },
            },
        },
    },

    attributeHandlers: {
        handleOnOff: {
            aliases: ['onOff'],
            handler: function(args) {
                var value = SbmdUtils.Tlv.decode(args.attribute.tlvBase64);
                var isOn = (value === true) ? 'true' : 'false';

                return SbmdUtils.result()
                    .dataModel.updateResource(args.endpointId, RES_IS_ON, isOn)
                    .success();
            },
        },

        handleCurrentLevel: {
            aliases: ['currentLevel'],
            handler: function(args) {
                var level = SbmdUtils.Tlv.decode(args.attribute.tlvBase64);
                var percent = Math.round(level / 254 * 100);

                return SbmdUtils.result()
                    .dataModel.updateResource(args.endpointId, RES_CURRENT_LEVEL, percent.toString())
                    .success();
            },
        },
    },
});
