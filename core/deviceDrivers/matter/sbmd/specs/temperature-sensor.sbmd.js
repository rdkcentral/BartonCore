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
// Temperature Sensor SBMD Driver
//
// Maps Matter Temperature Sensor device type to Barton environmentalSensor.
// MeasuredValue is in hundredths of degrees Celsius.
//

SbmdDriver({
    schemaVersion: '4.0',
    driverVersion: 1,
    name: 'Temperature Sensor',

    constants: {
        CL_TEMP_MEASUREMENT: 0x0402,
        ATTR_MEASURED_VALUE: 0x0000,
        RES_TEMPERATURE: 'temperature'
    },

    barton: {
        deviceClass: 'environmentalSensor',
        deviceClassVersion: 1
    },

    matter: {
        deviceTypes: [0x0302],
        revision: 3
    },

    reporting: {
        minSecs: 1,
        maxSecs: 3600
    },

    aliases: {
        tempMeasuredValue: {
            clusterId: CL_TEMP_MEASUREMENT,
            attributeId: ATTR_MEASURED_VALUE,
            type: 'int16'
        }
    },

    endpoints: {
        '1': {
            profile: 'sensor',
            profileVersion: 2,
            resources: {
                temperature: {
                    type: 'com.icontrol.temperature',
                    modes: ['read'],
                    prerequisites: [CL_TEMP_MEASUREMENT]
                }
            }
        }
    },

    attributeHandlers: {
        handleTemperature: {aliases: ['tempMeasuredValue'], handler: handleTemperature}
    }
});

// =============================================================================
// Handler Implementations
// =============================================================================

/**
 * Maps Matter Temperature MeasuredValue (int16, cluster 0x0402) to the Barton
 * temperature resource.
 */
function handleTemperature(args) {
    var value = Sbmd.Tlv.decode(args.attribute.tlvBase64);

    // -32768 (0x8000): Matter null for int16 MeasuredValue
    if (value === null || value === -32768) {
        return Sbmd.result().error('MeasuredValue unavailable');
    }

    return Sbmd.result()
        .dataModel.updateResource(args.endpointId, RES_TEMPERATURE, value.toString())
        .success();
}
