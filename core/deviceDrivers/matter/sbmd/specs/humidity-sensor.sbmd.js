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
// Humidity Sensor SBMD Driver
//
// Maps Matter Humidity Sensor device type to Barton environmentalSensor.
// MeasuredValue is in hundredths of percent RH; converted to whole percent.
//

SbmdDriver({
    schemaVersion: '4.0',
    driverVersion: 1,
    name: 'Humidity Sensor',

    constants: {
        CL_HUMIDITY_MEASUREMENT: 0x0405,
        ATTR_MEASURED_VALUE: 0x0000,
        RES_HUMIDITY: 'humidity'
    },

    barton: {
        deviceClass: 'environmentalSensor',
        deviceClassVersion: 1
    },

    matter: {
        deviceTypes: [0x0307],
        revision: 3
    },

    reporting: {
        minSecs: 1,
        maxSecs: 3600
    },

    aliases: {
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
                humidity: {
                    type: 'com.icontrol.humidity',
                    modes: ['read'],
                    prerequisites: [CL_HUMIDITY_MEASUREMENT]
                }
            }
        }
    },

    attributeHandlers: {
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

                return Sbmd.result()
                    .dataModel.updateResource(RES_HUMIDITY, percent.toString())
                    .success();
            }
        }
    }
});
