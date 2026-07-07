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
// Thermostat SBMD Driver
//
// Maps Matter Thermostat device type to Barton thermostat device class.
// Supports thermostat cluster mandatory attributes, system mode, setpoints,
// running state, and optional fan control cluster.
//

SbmdDriver({
    schemaVersion: '4.0',
    driverVersion: 1,
    name: 'Thermostat',

    constants: {
        // Clusters
        CL_THERMOSTAT: 0x0201,
        CL_FAN_CONTROL: 0x0202,

        // Thermostat cluster attributes
        ATTR_LOCAL_TEMPERATURE: 0x0000,
        ATTR_ABS_MIN_HEAT: 0x0003,
        ATTR_ABS_MAX_HEAT: 0x0004,
        ATTR_ABS_MIN_COOL: 0x0005,
        ATTR_ABS_MAX_COOL: 0x0006,
        ATTR_OCCUPIED_COOLING_SETPOINT: 0x0011,
        ATTR_OCCUPIED_HEATING_SETPOINT: 0x0012,
        ATTR_CTRL_SEQ_OP: 0x001b,
        ATTR_SYSTEM_MODE: 0x001c,
        ATTR_RUNNING_STATE: 0x0029,

        // Fan Control attributes
        ATTR_FAN_MODE: 0x0000,
        ATTR_FAN_PERCENT_CURRENT: 0x0006,

        // Resource IDs
        RES_LOCAL_TEMP: 'localTemperature',
        RES_HEAT_SETPOINT: 'heatSetpoint',
        RES_COOL_SETPOINT: 'coolSetpoint',
        RES_ABS_MIN_HEAT: 'absoluteMinHeatLimit',
        RES_ABS_MAX_HEAT: 'absoluteMaxHeatLimit',
        RES_ABS_MIN_COOL: 'absoluteMinCoolLimit',
        RES_ABS_MAX_COOL: 'absoluteMaxCoolLimit',
        RES_CTRL_SEQ_OP: 'controlSequenceOfOperation',
        RES_SYSTEM_MODE: 'systemMode',
        RES_SYSTEM_STATE: 'systemState',
        RES_FAN_MODE: 'fanMode',
        RES_FAN_ON: 'fanOn'
    },

    barton: {
        deviceClass: 'thermostat',
        deviceClassVersion: 1
    },

    matter: {
        deviceTypes: [0x0301],
        revision: 1,
        featureClusters: [0x0201]
    },

    reporting: {
        minSecs: 1,
        maxSecs: 3600
    },

    aliases: {
        localTemperature: {
            clusterId: CL_THERMOSTAT,
            attributeId: ATTR_LOCAL_TEMPERATURE,
            type: 'int16'
        },
        absMinHeat: {
            clusterId: CL_THERMOSTAT,
            attributeId: ATTR_ABS_MIN_HEAT,
            type: 'int16'
        },
        absMaxHeat: {
            clusterId: CL_THERMOSTAT,
            attributeId: ATTR_ABS_MAX_HEAT,
            type: 'int16'
        },
        absMinCool: {
            clusterId: CL_THERMOSTAT,
            attributeId: ATTR_ABS_MIN_COOL,
            type: 'int16'
        },
        absMaxCool: {
            clusterId: CL_THERMOSTAT,
            attributeId: ATTR_ABS_MAX_COOL,
            type: 'int16'
        },
        occupiedCoolingSetpoint: {
            clusterId: CL_THERMOSTAT,
            attributeId: ATTR_OCCUPIED_COOLING_SETPOINT,
            type: 'int16'
        },
        occupiedHeatingSetpoint: {
            clusterId: CL_THERMOSTAT,
            attributeId: ATTR_OCCUPIED_HEATING_SETPOINT,
            type: 'int16'
        },
        ctrlSeqOp: {
            clusterId: CL_THERMOSTAT,
            attributeId: ATTR_CTRL_SEQ_OP,
            type: 'enum8'
        },
        systemMode: {
            clusterId: CL_THERMOSTAT,
            attributeId: ATTR_SYSTEM_MODE,
            type: 'enum8'
        },
        runningState: {
            clusterId: CL_THERMOSTAT,
            attributeId: ATTR_RUNNING_STATE,
            type: 'uint16'
        },
        fanMode: {
            clusterId: CL_FAN_CONTROL,
            attributeId: ATTR_FAN_MODE,
            type: 'enum8'
        },
        fanPercentCurrent: {
            clusterId: CL_FAN_CONTROL,
            attributeId: ATTR_FAN_PERCENT_CURRENT,
            type: 'uint8'
        }
    },

    endpoints: {
        '1': {
            profile: 'thermostat',
            profileVersion: 2,
            resources: {
                localTemperature: {
                    type: 'com.icontrol.temperature',
                    modes: ['read'],
                    prerequisites: [CL_THERMOSTAT]
                },
                heatSetpoint: {
                    type: 'com.icontrol.temperature',
                    modes: ['read', 'write'],
                    prerequisites: [CL_THERMOSTAT],
                    write: writeHeatSetpoint
                },
                coolSetpoint: {
                    type: 'com.icontrol.temperature',
                    modes: ['read', 'write'],
                    prerequisites: [CL_THERMOSTAT],
                    write: writeCoolSetpoint
                },
                absoluteMinHeatLimit: {
                    type: 'com.icontrol.temperature',
                    modes: ['read'],
                    prerequisites: [CL_THERMOSTAT]
                },
                absoluteMaxHeatLimit: {
                    type: 'com.icontrol.temperature',
                    modes: ['read'],
                    prerequisites: [CL_THERMOSTAT]
                },
                absoluteMinCoolLimit: {
                    type: 'com.icontrol.temperature',
                    modes: ['read'],
                    prerequisites: [CL_THERMOSTAT]
                },
                absoluteMaxCoolLimit: {
                    type: 'com.icontrol.temperature',
                    modes: ['read'],
                    prerequisites: [CL_THERMOSTAT]
                },
                controlSequenceOfOperation: {
                    type: 'com.icontrol.tstatCtrlSeqOp',
                    modes: ['read', 'write'],
                    prerequisites: [CL_THERMOSTAT],
                    write: writeControlSequenceOfOperation
                },
                systemMode: {
                    type: 'com.icontrol.tstatSystemMode',
                    modes: ['read', 'write'],
                    prerequisites: [CL_THERMOSTAT],
                    write: writeSystemMode
                },
                systemState: {
                    type: 'com.icontrol.tstatSystemState',
                    optional: true,
                    modes: ['read'],
                    prerequisites: [CL_THERMOSTAT]
                },
                fanMode: {
                    type: 'com.icontrol.tstatFanMode',
                    optional: true,
                    modes: ['read', 'write'],
                    prerequisites: [CL_FAN_CONTROL],
                    write: writeFanMode
                },
                fanOn: {
                    type: 'boolean',
                    optional: true,
                    modes: ['read'],
                    prerequisites: [CL_FAN_CONTROL]
                }
            }
        }
    },

    attributeHandlers: {
        handleLocalTemperature: {aliases: ['localTemperature'], handler: handleLocalTemperature},
        handleHeatSetpoint: {aliases: ['occupiedHeatingSetpoint'], handler: handleHeatSetpoint},
        handleCoolSetpoint: {aliases: ['occupiedCoolingSetpoint'], handler: handleCoolSetpoint},
        handleAbsMinHeat: {aliases: ['absMinHeat'], handler: handleAbsMinHeat},
        handleAbsMaxHeat: {aliases: ['absMaxHeat'], handler: handleAbsMaxHeat},
        handleAbsMinCool: {aliases: ['absMinCool'], handler: handleAbsMinCool},
        handleAbsMaxCool: {aliases: ['absMaxCool'], handler: handleAbsMaxCool},
        handleCtrlSeqOp: {aliases: ['ctrlSeqOp'], handler: handleCtrlSeqOp},
        handleSystemMode: {aliases: ['systemMode'], handler: handleSystemMode},
        handleRunningState: {aliases: ['runningState'], handler: handleRunningState},
        handleFanMode: {aliases: ['fanMode'], handler: handleFanMode},
        handleFanPercentCurrent: {aliases: ['fanPercentCurrent'], handler: handleFanPercentCurrent}
    }
});

// =============================================================================
// Handler Implementations
// =============================================================================

/**
 * Writes the Barton heatSetpoint to the Matter OccupiedHeatingSetpoint
 * attribute (int16, cluster 0x0201). Rejects invalid input.
 */
function writeHeatSetpoint(args) {
    var tlvBase64 = Sbmd.Tlv.encode(args.resource.input, 'int16');

    if (tlvBase64 === null) {
        return Sbmd.result().error('Invalid temperature value');
    }

    return Sbmd.result().device.writeAttribute(CL_THERMOSTAT, ATTR_OCCUPIED_HEATING_SETPOINT, tlvBase64);
}

/**
 * Writes the Barton coolSetpoint to the Matter OccupiedCoolingSetpoint
 * attribute (int16, cluster 0x0201). Rejects invalid input.
 */
function writeCoolSetpoint(args) {
    var tlvBase64 = Sbmd.Tlv.encode(args.resource.input, 'int16');

    if (tlvBase64 === null) {
        return Sbmd.result().error('Invalid temperature value');
    }

    return Sbmd.result().device.writeAttribute(CL_THERMOSTAT, ATTR_OCCUPIED_COOLING_SETPOINT, tlvBase64);
}

/**
 * Writes the Barton controlSequenceOfOperation string to the Matter
 * ControlSequenceOfOperation attribute (enum8, cluster 0x0201). Rejects unknown
 * values.
 */
function writeControlSequenceOfOperation(args) {
    var seqValues = [
        'coolingOnly',
        'coolingWithReheat',
        'heatingOnly',
        'heatingWithReheat',
        'coolingAndHeatingFourPipes',
        'coolingAndHeatingFourPipesWithReheat'
    ];
    var seqValue = -1;

    for (var i = 0; i < seqValues.length; i++) {
        if (seqValues[i] === args.resource.input) {
            seqValue = i;
            break;
        }
    }

    if (seqValue < 0) {
        return Sbmd.result().error('Unknown control sequence: ' + args.resource.input);
    }

    var tlvBase64 = Sbmd.Tlv.encode(seqValue, 'enum8');

    return Sbmd.result().device.writeAttribute(CL_THERMOSTAT, ATTR_CTRL_SEQ_OP, tlvBase64);
}

/**
 * Writes the Barton systemMode string to the Matter SystemMode attribute
 * (enum8, cluster 0x0201). Rejects unknown modes.
 */
function writeSystemMode(args) {
    var reverseModeMap = {
        'off': 0,
        'auto': 1,
        'cool': 3,
        'heat': 4,
        'precooling': 6,
        'fanOnly': 7
    };
    var modeValue = reverseModeMap[args.resource.input];

    if (modeValue === undefined) {
        return Sbmd.result().error('Unknown system mode: ' + args.resource.input);
    }

    var tlvBase64 = Sbmd.Tlv.encode(modeValue, 'enum8');

    return Sbmd.result().device.writeAttribute(CL_THERMOSTAT, ATTR_SYSTEM_MODE, tlvBase64);
}

/**
 * Writes the Barton fanMode string to the Matter FanMode attribute (enum8, Fan
 * Control cluster 0x0202). Rejects unknown modes.
 */
function writeFanMode(args) {
    var reverseModeMap = {
        'off': 0,
        'on': 4,
        'auto': 5
    };
    var modeValue = reverseModeMap[args.resource.input];

    if (modeValue === undefined) {
        return Sbmd.result().error('Unknown fan mode: ' + args.resource.input);
    }

    var tlvBase64 = Sbmd.Tlv.encode(modeValue, 'enum8');

    return Sbmd.result().device.writeAttribute(CL_FAN_CONTROL, ATTR_FAN_MODE, tlvBase64);
}

/**
 * Maps Matter LocalTemperature (int16 hundredths of a degree, cluster 0x0201)
 * to the Barton localTemperature resource, zero-padded to a fixed width.
 */
function handleLocalTemperature(args) {
    var value = Sbmd.Tlv.decode(args.attribute.tlvBase64);

    // No update when the reading is unavailable (null)
    if (value === null) {
        return Sbmd.result().success();
    }

    var neg = value < 0;
    var s = Math.abs(value).toString();

    while (s.length < (neg ? 3 : 4)) {
        s = '0' + s;
    }

    return Sbmd.result()
        .dataModel.updateResource(RES_LOCAL_TEMP, (neg ? '-' : '') + s)
        .success();
}

/**
 * Maps Matter OccupiedHeatingSetpoint (int16 hundredths of a degree, cluster
 * 0x0201) to the Barton heatSetpoint resource, zero-padded to a fixed width.
 */
function handleHeatSetpoint(args) {
    var value = Sbmd.Tlv.decode(args.attribute.tlvBase64);

    if (value === null) {
        return Sbmd.result().error('TLV decode failed for OccupiedHeatingSetpoint');
    }

    var neg = value < 0;
    var s = Math.abs(value).toString();

    while (s.length < (neg ? 3 : 4)) {
        s = '0' + s;
    }

    return Sbmd.result()
        .dataModel.updateResource(RES_HEAT_SETPOINT, (neg ? '-' : '') + s)
        .success();
}

/**
 * Maps Matter OccupiedCoolingSetpoint (int16 hundredths of a degree, cluster
 * 0x0201) to the Barton coolSetpoint resource, zero-padded to a fixed width.
 */
function handleCoolSetpoint(args) {
    var value = Sbmd.Tlv.decode(args.attribute.tlvBase64);

    if (value === null) {
        return Sbmd.result().error('TLV decode failed for OccupiedCoolingSetpoint');
    }

    var neg = value < 0;
    var s = Math.abs(value).toString();

    while (s.length < (neg ? 3 : 4)) {
        s = '0' + s;
    }

    return Sbmd.result()
        .dataModel.updateResource(RES_COOL_SETPOINT, (neg ? '-' : '') + s)
        .success();
}

/**
 * Maps Matter AbsMinHeatSetpointLimit (int16, cluster 0x0201) to the Barton
 * absoluteMinHeatLimit resource, zero-padded to a fixed width.
 */
function handleAbsMinHeat(args) {
    var value = Sbmd.Tlv.decode(args.attribute.tlvBase64);

    if (value === null) {
        return Sbmd.result().error('TLV decode failed');
    }

    var neg = value < 0;
    var s = Math.abs(value).toString();

    while (s.length < (neg ? 3 : 4)) {
        s = '0' + s;
    }

    return Sbmd.result()
        .dataModel.updateResource(RES_ABS_MIN_HEAT, (neg ? '-' : '') + s)
        .success();
}

/**
 * Maps Matter AbsMaxHeatSetpointLimit (int16, cluster 0x0201) to the Barton
 * absoluteMaxHeatLimit resource, zero-padded to a fixed width.
 */
function handleAbsMaxHeat(args) {
    var value = Sbmd.Tlv.decode(args.attribute.tlvBase64);

    if (value === null) {
        return Sbmd.result().error('TLV decode failed');
    }

    var neg = value < 0;
    var s = Math.abs(value).toString();

    while (s.length < (neg ? 3 : 4)) {
        s = '0' + s;
    }

    return Sbmd.result()
        .dataModel.updateResource(RES_ABS_MAX_HEAT, (neg ? '-' : '') + s)
        .success();
}

/**
 * Maps Matter AbsMinCoolSetpointLimit (int16, cluster 0x0201) to the Barton
 * absoluteMinCoolLimit resource, zero-padded to a fixed width.
 */
function handleAbsMinCool(args) {
    var value = Sbmd.Tlv.decode(args.attribute.tlvBase64);

    if (value === null) {
        return Sbmd.result().error('TLV decode failed');
    }

    var neg = value < 0;
    var s = Math.abs(value).toString();

    while (s.length < (neg ? 3 : 4)) {
        s = '0' + s;
    }

    return Sbmd.result()
        .dataModel.updateResource(RES_ABS_MIN_COOL, (neg ? '-' : '') + s)
        .success();
}

/**
 * Maps Matter AbsMaxCoolSetpointLimit (int16, cluster 0x0201) to the Barton
 * absoluteMaxCoolLimit resource, zero-padded to a fixed width.
 */
function handleAbsMaxCool(args) {
    var value = Sbmd.Tlv.decode(args.attribute.tlvBase64);

    if (value === null) {
        return Sbmd.result().error('TLV decode failed');
    }

    var neg = value < 0;
    var s = Math.abs(value).toString();

    while (s.length < (neg ? 3 : 4)) {
        s = '0' + s;
    }

    return Sbmd.result()
        .dataModel.updateResource(RES_ABS_MAX_COOL, (neg ? '-' : '') + s)
        .success();
}

/**
 * Maps Matter ControlSequenceOfOperation (enum8, cluster 0x0201) to the Barton
 * controlSequenceOfOperation resource as a string. Rejects unknown values.
 */
function handleCtrlSeqOp(args) {
    var value = Sbmd.Tlv.decode(args.attribute.tlvBase64);

    if (value === null) {
        return Sbmd.result().error('TLV decode failed');
    }

    var seqValues = [
        'coolingOnly',
        'coolingWithReheat',
        'heatingOnly',
        'heatingWithReheat',
        'coolingAndHeatingFourPipes',
        'coolingAndHeatingFourPipesWithReheat'
    ];
    var seq = seqValues[value];

    if (seq === undefined) {
        return Sbmd.result().error('Unknown ControlSequenceOfOperation: ' + value);
    }

    return Sbmd.result().dataModel.updateResource(RES_CTRL_SEQ_OP, seq).success();
}

/**
 * Maps Matter SystemMode (enum8, cluster 0x0201) to the Barton systemMode
 * resource as a string; unknown modes become 'unknown'.
 */
function handleSystemMode(args) {
    var value = Sbmd.Tlv.decode(args.attribute.tlvBase64);

    if (value === null) {
        return Sbmd.result().error('TLV decode failed');
    }

    var modeMap = {
        0: 'off',
        1: 'auto',
        3: 'cool',
        4: 'heat',
        5: 'heat',
        6: 'precooling',
        7: 'fanOnly'
    };
    var mode = modeMap[value];

    if (mode === undefined) {
        mode = 'unknown';
    }

    return Sbmd.result().dataModel.updateResource(RES_SYSTEM_MODE, mode).success();
}

/**
 * Maps Matter ThermostatRunningState (bitmap16, cluster 0x0201) to the Barton
 * systemState resource (heating/cooling/off) from the active heat/cool bits.
 */
function handleRunningState(args) {
    var value = Sbmd.Tlv.decode(args.attribute.tlvBase64);

    if (value === null) {
        return Sbmd.result().error('TLV decode failed');
    }

    var state = 'off';

    if (value & 0x0001 || value & 0x0008) {
        state = 'heating';
    } else if (value & 0x0002 || value & 0x0010) {
        state = 'cooling';
    }

    return Sbmd.result().dataModel.updateResource(RES_SYSTEM_STATE, state).success();
}

/**
 * Maps Matter FanMode (enum8, Fan Control cluster 0x0202) to the Barton fanMode
 * resource (off/on/auto); unknown modes become 'unknown'.
 */
function handleFanMode(args) {
    var value = Sbmd.Tlv.decode(args.attribute.tlvBase64);

    if (value === null) {
        return Sbmd.result().error('TLV decode failed');
    }

    // FanMode: 0=Off, 1=Low, 2=Medium, 3=High, 4=On, 5=Auto
    var modeMap = {
        0: 'off',
        1: 'on',
        2: 'on',
        3: 'on',
        4: 'on',
        5: 'auto'
    };
    var mode = modeMap[value];

    if (mode === undefined) {
        mode = 'unknown';
    }

    return Sbmd.result().dataModel.updateResource(RES_FAN_MODE, mode).success();
}

/**
 * Maps Matter PercentCurrent (uint8, Fan Control cluster 0x0202) to the Barton
 * fanOn resource as a boolean string (on when non-zero).
 */
function handleFanPercentCurrent(args) {
    var value = Sbmd.Tlv.decode(args.attribute.tlvBase64);

    if (value === null) {
        return Sbmd.result().error('TLV decode failed');
    }

    return Sbmd.result()
        .dataModel.updateResource(RES_FAN_ON, String(value !== 0))
        .success();
}
