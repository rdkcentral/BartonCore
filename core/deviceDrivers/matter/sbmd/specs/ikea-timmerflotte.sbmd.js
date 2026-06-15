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
// IKEA TIMMERFLOTTE SBMD Driver
//
// Vendor-specific driver for the IKEA TIMMERFLOTTE temperature and humidity
// sensor (VID 0x117C / PID 0x8005), claimed by vendor/product ID match.
//

SbmdDriver({
    schemaVersion: '4.0',
    driverVersion: '1.0.0',
    name: 'IKEA TIMMERFLOTTE',

    constants: {
        CL_TEMP_MEASUREMENT: 0x0402,
        CL_HUMIDITY_MEASUREMENT: 0x0405,
        ATTR_MEASURED_VALUE: 0x0000,
        RES_TEMPERATURE: 'temperature',
        RES_HUMIDITY: 'humidity'
    },

    barton: {
        deviceClass: 'environmentalSensor',
        deviceClassVersion: 1
    },

    matter: {
        vendorId: 0x117C,
        productId: 0x8005,
        deviceTypes: [0x0302, 0x0307]
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
        },
        humidityMeasuredValue: {
            clusterId: CL_HUMIDITY_MEASUREMENT,
            attributeId: ATTR_MEASURED_VALUE,
            type: 'uint16'
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
                },
                humidity: {
                    type: 'com.icontrol.humidity',
                    modes: ['read'],
                    prerequisites: [CL_HUMIDITY_MEASUREMENT]
                }
            }
        }
    },

    attributeHandlers: {
        handleTemperature: {
            aliases: ['tempMeasuredValue'],
            handler: function(args) {
                var value = Sbmd.Tlv.decode(args.attribute.tlvBase64);

                // -32768 (0x8000): Matter null for int16 MeasuredValue
                if (value === null || value === -32768) {
                    return Sbmd.result()
                        .error('TLV decode failed for MeasuredValue');
                }

                return Sbmd.result()
                    .dataModel.updateResource(RES_TEMPERATURE, value.toString())
                    .success();
            }
        },
        handleHumidity: {
            aliases: ['humidityMeasuredValue'],
            handler: function(args) {
                var value = Sbmd.Tlv.decode(args.attribute.tlvBase64);

                // 0xFFFF: Matter null for uint16 MeasuredValue
                if (value === null || value === 0xFFFF) {
                    return Sbmd.result()
                        .error('TLV decode failed for MeasuredValue');
                }

                // Matter humidity is in hundredths of percent, convert to whole percent
                var percent = Math.round(value / 100);

                // Explicit endpoint '1' because the humidity cluster is on device
                // endpoint 2 but the resource is registered on Barton endpoint 1
                return Sbmd.result()
                    .dataModel.updateResource('1', RES_HUMIDITY, percent.toString())
                    .success();
            }
        }
    }
});
