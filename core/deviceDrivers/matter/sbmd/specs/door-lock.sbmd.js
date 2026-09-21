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
// The locked resource is seeded from the cached LockState attribute at
// commission time; live updates are driven exclusively by LockOperation
// events. DoorLockAlarm events drive the jammed, tampered, and
// invalidCodeEntryLimit resources.
// Lock/Unlock commands sent via execute handlers with optional PIN code.
//

SbmdDriver({
    schemaVersion: '5.0',
    driverVersion: 2,
    name: 'Door Lock',

    constants: {
        CL_DOOR_LOCK: 0x0101,

        // Attributes
        ATTR_LOCK_STATE: 0x0000,

        // Events
        EVT_DOOR_LOCK_ALARM: 0x0000,
        EVT_LOCK_OPERATION: 0x0002,

        // Commands
        CMD_LOCK_DOOR: 0x0000,
        CMD_UNLOCK_DOOR: 0x0001,

        // DoorLockAlarm alarm codes
        ALARM_LOCK_JAMMED: 0x00,
        ALARM_WRONG_CODE_ENTRY_LIMIT: 0x04,
        ALARM_FRONT_ESCUTCHEON_REMOVED: 0x05,
        ALARM_DOOR_FORCED_OPEN: 0x06,

        // LockOperation operation types
        OP_TYPE_LOCK: 0x00,
        OP_TYPE_UNLOCK: 0x01,
        OP_TYPE_UNLATCH: 0x04,

        // LockOperation operation source
        OP_SOURCE_MANUAL: 0x01,

        // Resource IDs
        RES_LOCKED: 'locked',
        RES_LOCK: 'lock',
        RES_UNLOCK: 'unlock',
        RES_JAMMED: 'jammed',
        RES_TAMPERED: 'tampered',
        RES_INVALID_CODE_ENTRY_LIMIT: 'invalidCodeEntryLimit'
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
        },
        doorLockAlarm: {
            clusterId: CL_DOOR_LOCK,
            eventId: EVT_DOOR_LOCK_ALARM
        },
        lockOperation: {
            clusterId: CL_DOOR_LOCK,
            eventId: EVT_LOCK_OPERATION
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
                    prerequisites: [CL_DOOR_LOCK],
                    seed: {
                        supplements: {
                            attributes: ['lockState']
                        },
                        handler: seedLocked
                    }
                },
                lock: {
                    type: 'function',
                    execute: executeLock
                },
                unlock: {
                    type: 'function',
                    execute: executeUnlock
                },
                jammed: {
                    type: 'boolean',
                    modes: ['read'],
                    prerequisites: [CL_DOOR_LOCK]
                },
                tampered: {
                    type: 'boolean',
                    modes: ['read'],
                    prerequisites: [CL_DOOR_LOCK]
                },
                invalidCodeEntryLimit: {
                    type: 'boolean',
                    modes: ['read'],
                    prerequisites: [CL_DOOR_LOCK]
                }
            }
        }
    },

    eventHandlers: {
        handleDoorLockAlarm: {aliases: ['doorLockAlarm'], handler: handleDoorLockAlarm},
        handleLockOperation: {aliases: ['lockOperation'], handler: handleLockOperation}
    }
});

// =============================================================================
// Handler Implementations
// =============================================================================

/**
 * Seeds the Barton locked resource from the cached Matter LockState attribute
 * (cluster 0x0101). Defaults to unlocked when the attribute is not yet cached.
 */
function seedLocked(args) {
    var tlvBase64 = args.supplements.attributes.lockState;
    var value = tlvBase64 !== null ? Sbmd.Tlv.decode(tlvBase64) : null;

    // LockState: 0=NotFullyLocked, 1=Locked, 2=Unlocked, 3=Unlatched
    // If the attribute is not yet cached, defaults to false (unlocked).
    var isLocked = value === 1;

    return Sbmd.result()
        .dataModel.updateResource(args.endpointId, RES_LOCKED, isLocked ? 'true' : 'false')
        .success();
}

/**
 * Invokes the Matter LockDoor command (cluster 0x0101), attaching a PIN
 * credential when the device requires one (PIN + COTA features) and input is
 * provided.
 */
function executeLock(args) {
    var featureMap = args.clusterFeatureMaps[CL_DOOR_LOCK] || 0;
    var tlvBase64 = null;
    var pinString = args.resource.input;

    // 0x01 = PIN credential, 0x80 = COTA
    if ((featureMap & 0x81) === 0x81 && pinString && pinString.length > 0) {
        var schema = {
            PINCode: {tag: 0, type: 'octstr'}
        };
        var pinBytes = new Uint8Array(pinString.length);

        for (var i = 0; i < pinString.length; i++) {
            pinBytes[i] = pinString.charCodeAt(i);
        }

        tlvBase64 = Sbmd.Tlv.encodeStruct({PINCode: pinBytes}, schema);
    }

    return Sbmd.result().device.sendCommand(CL_DOOR_LOCK, CMD_LOCK_DOOR, tlvBase64, {
        timedInvokeTimeoutMs: 10000
    });
}

/**
 * Invokes the Matter UnlockDoor command (cluster 0x0101), attaching a PIN
 * credential when the device requires one (PIN + COTA features) and input is
 * provided.
 */
function executeUnlock(args) {
    var featureMap = args.clusterFeatureMaps[CL_DOOR_LOCK] || 0;
    var tlvBase64 = null;
    var pinString = args.resource.input;

    // 0x01 = PIN credential, 0x80 = COTA
    if ((featureMap & 0x81) === 0x81 && pinString && pinString.length > 0) {
        var schema = {
            PINCode: {tag: 0, type: 'octstr'}
        };
        var pinBytes = new Uint8Array(pinString.length);

        for (var i = 0; i < pinString.length; i++) {
            pinBytes[i] = pinString.charCodeAt(i);
        }

        tlvBase64 = Sbmd.Tlv.encodeStruct({PINCode: pinBytes}, schema);
    }

    return Sbmd.result().device.sendCommand(CL_DOOR_LOCK, CMD_UNLOCK_DOOR, tlvBase64, {
        timedInvokeTimeoutMs: 10000
    });
}

/**
 * Maps Matter DoorLockAlarm events (cluster 0x0101, event 0x0000) to the
 * jammed, tampered, and invalidCodeEntryLimit resources. Alarm codes with no
 * corresponding resource are logged and ignored.
 */
function handleDoorLockAlarm(args) {
    var fields = Sbmd.Tlv.decode(args.event.tlvBase64);

    if (fields === null) {
        return Sbmd.result().error('TLV decode failed for DoorLockAlarm');
    }

    var alarmCode = fields[0];
    var result = Sbmd.result();

    if (alarmCode === ALARM_LOCK_JAMMED) {
        return result.dataModel.updateResource(args.endpointId, RES_JAMMED, 'true').success();
    }

    if (alarmCode === ALARM_WRONG_CODE_ENTRY_LIMIT) {
        return result.dataModel.updateResource(args.endpointId, RES_INVALID_CODE_ENTRY_LIMIT, 'true').success();
    }

    if (alarmCode === ALARM_FRONT_ESCUTCHEON_REMOVED || alarmCode === ALARM_DOOR_FORCED_OPEN) {
        return result.dataModel.updateResource(args.endpointId, RES_TAMPERED, 'true').success();
    }

    return result.log('DoorLockAlarm: unresourced alarm code 0x' + alarmCode.toString(16)).success();
}

/**
 * Maps Matter LockOperation events (cluster 0x0101, event 0x0002) to the locked
 * resource and clears the tampered and invalidCodeEntryLimit fault resources.
 * A manually-sourced operation additionally clears jammed.
 */
function handleLockOperation(args) {
    var fields = Sbmd.Tlv.decode(args.event.tlvBase64);

    if (fields === null) {
        return Sbmd.result().error('TLV decode failed for LockOperation');
    }

    var opType = fields[0];
    var source = fields[1];

    var isLock = opType === OP_TYPE_LOCK;
    var isUnlock = opType === OP_TYPE_UNLOCK || opType === OP_TYPE_UNLATCH;

    if (!isLock && !isUnlock) {
        return Sbmd.result().success();
    }

    var result = Sbmd.result()
        .dataModel.updateResource(args.endpointId, RES_LOCKED, isLock ? 'true' : 'false')
        .dataModel.updateResource(args.endpointId, RES_TAMPERED, 'false')
        .dataModel.updateResource(args.endpointId, RES_INVALID_CODE_ENTRY_LIMIT, 'false');

    if (source === OP_SOURCE_MANUAL) {
        result = result.dataModel.updateResource(args.endpointId, RES_JAMMED, 'false');
    }

    return result.success();
}
