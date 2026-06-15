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
// Door Lock SBMD Driver
//
// Maps Matter Door Lock device type to Barton doorLock device class.
// Uses LockState attribute for real-time lock state updates.
// Lock/Unlock commands sent via execute handlers with optional PIN code.
//
// Note: LockOperation event handler support is deferred until event
// infrastructure is implemented.
//

SbmdDriver({
    schemaVersion: '4.0',
    driverVersion: '1.0.0',
    name: 'Door Lock',

    constants: {
        CL_DOOR_LOCK: 0x0101,

        // Attributes
        ATTR_LOCK_STATE: 0x0000,

        // Commands
        CMD_LOCK_DOOR: 0x0000,
        CMD_UNLOCK_DOOR: 0x0001,

        // Resource IDs
        RES_LOCKED: 'locked',
        RES_LOCK: 'lock',
        RES_UNLOCK: 'unlock'
    },

    barton: {
        deviceClass: 'doorLock',
        deviceClassVersion: 3
    },

    matter: {
        deviceTypes: [0x000a],
        revision: 1,
        featureClusters: [0x0101]
    },

    reporting: {
        minSecs: 1,
        maxSecs: 3600
    },

    aliases: {
        lockState: {
            clusterId: CL_DOOR_LOCK,
            attributeId: ATTR_LOCK_STATE,
            type: 'enum8'
        }
    },

    endpoints: {
        '1': {
            profile: 'doorLock',
            profileVersion: 3,
            resources: {
                locked: {
                    type: 'boolean',
                    modes: ['read'],
                    prerequisites: [CL_DOOR_LOCK]
                },
                lock: {
                    type: 'function',
                    execute: function(args) {
                        var featureMap = args.clusterFeatureMaps[CL_DOOR_LOCK] || 0;
                        var tlvBase64 = null;
                        var pinString = args.resource.input;

                        // 0x01 = PIN credential, 0x80 = COTA
                        if (((featureMap & 0x81) === 0x81) &&
                            pinString && pinString.length > 0) {
                            var schema = {
                                PINCode: { tag: 0, type: 'octstr' }
                            };
                            var pinBytes = new Uint8Array(pinString.length);

                            for (var i = 0; i < pinString.length; i++) {
                                pinBytes[i] = pinString.charCodeAt(i);
                            }

                            tlvBase64 = SbmdUtils.Tlv.encodeStruct({ PINCode: pinBytes }, schema);
                        }

                        return SbmdUtils.result()
                            .device.sendCommand(CL_DOOR_LOCK, CMD_LOCK_DOOR, tlvBase64, { timedInvokeTimeoutMs: 10000 });
                    }
                },
                unlock: {
                    type: 'function',
                    execute: function(args) {
                        var featureMap = args.clusterFeatureMaps[CL_DOOR_LOCK] || 0;
                        var tlvBase64 = null;
                        var pinString = args.resource.input;

                        // 0x01 = PIN credential, 0x80 = COTA
                        if (((featureMap & 0x81) === 0x81) &&
                            pinString && pinString.length > 0) {
                            var schema = {
                                PINCode: { tag: 0, type: 'octstr' }
                            };
                            var pinBytes = new Uint8Array(pinString.length);

                            for (var i = 0; i < pinString.length; i++) {
                                pinBytes[i] = pinString.charCodeAt(i);
                            }

                            tlvBase64 = SbmdUtils.Tlv.encodeStruct({ PINCode: pinBytes }, schema);
                        }

                        return SbmdUtils.result()
                            .device.sendCommand(CL_DOOR_LOCK, CMD_UNLOCK_DOOR, tlvBase64, { timedInvokeTimeoutMs: 10000 });
                    }
                }
            }
        }
    },

    attributeHandlers: {
        handleLockState: {
            aliases: ['lockState'],
            handler: function(args) {
                var value = SbmdUtils.Tlv.decode(args.attribute.tlvBase64);

                // LockState: 0=NotFullyLocked, 1=Locked, 2=Unlocked, 3=Unlatched
                var isLocked = value === 1;

                return SbmdUtils.result()
                    .dataModel.updateResource(RES_LOCKED, isLocked ? 'true' : 'false')
                    .success();
            }
        }
    }
});
