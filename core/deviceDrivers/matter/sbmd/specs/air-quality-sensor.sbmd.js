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
// Air Quality Sensor SBMD Driver
//
// Maps Matter Air Quality Sensor device type to Barton airQualitySensor.
// Supports air quality level, temperature, humidity, CO2, and PM2.5.
//

SbmdDriver({
    schemaVersion: '4.0',
    driverVersion: '1.0.0',
    name: 'Air Quality Sensor',

    constants: {
        // Clusters
        CL_AIR_QUALITY: 0x005b,
        CL_TEMP_MEASUREMENT: 0x0402,
        CL_HUMIDITY_MEASUREMENT: 0x0405,
        CL_CO2_MEASUREMENT: 0x040d,
        CL_PM25_MEASUREMENT: 0x042a,

        // Attributes (all MeasuredValue / AirQuality = 0x0000)
        ATTR_VALUE: 0x0000,

        // Resource IDs
        RES_AIR_QUALITY: 'airQuality',
        RES_TEMPERATURE: 'temperature',
        RES_HUMIDITY: 'humidity',
        RES_CO2: 'co2Concentration',
        RES_PM25: 'pm25Concentration'
    },

    barton: {
        deviceClass: 'airQualitySensor',
        deviceClassVersion: 1
    },

    matter: {
        deviceTypes: [0x002c],
        revision: 1,
        featureClusters: [0x005b]
    },

    reporting: {
        minSecs: 1,
        maxSecs: 3600
    },

    aliases: {
        airQualityValue: {
            clusterId: CL_AIR_QUALITY,
            attributeId: ATTR_VALUE,
            type: 'enum8'
        },
        tempMeasuredValue: {
            clusterId: CL_TEMP_MEASUREMENT,
            attributeId: ATTR_VALUE,
            type: 'int16'
        },
        humidityMeasuredValue: {
            clusterId: CL_HUMIDITY_MEASUREMENT,
            attributeId: ATTR_VALUE,
            type: 'uint16'
        },
        co2MeasuredValue: {
            clusterId: CL_CO2_MEASUREMENT,
            attributeId: ATTR_VALUE,
            type: 'float'
        },
        pm25MeasuredValue: {
            clusterId: CL_PM25_MEASUREMENT,
            attributeId: ATTR_VALUE,
            type: 'float'
        }
    },

    endpoints: {
        '1': {
            profile: 'airQualitySensor',
            profileVersion: 1,
            resources: {
                airQuality: {
                    type: 'com.icontrol.airQuality',
                    modes: ['read', 'dynamic', 'emitEvents'],
                    prerequisites: [CL_AIR_QUALITY],
                    seed: function(args) {
                        return SbmdUtils.result()
                            .dataModel.updateResource(RES_AIR_QUALITY, 'unknown')
                            .success();
                    }
                },
                temperature: {
                    type: 'com.icontrol.temperature',
                    optional: true,
                    modes: ['read', 'dynamic', 'emitEvents'],
                    prerequisites: [CL_TEMP_MEASUREMENT],
                    seed: function(args) {
                        return SbmdUtils.result()
                            .dataModel.updateResource(RES_TEMPERATURE, '0')
                            .success();
                    }
                },
                humidity: {
                    type: 'com.icontrol.humidity',
                    optional: true,
                    modes: ['read', 'dynamic', 'emitEvents'],
                    prerequisites: [CL_HUMIDITY_MEASUREMENT],
                    seed: function(args) {
                        return SbmdUtils.result()
                            .dataModel.updateResource(RES_HUMIDITY, '0')
                            .success();
                    }
                },
                co2Concentration: {
                    type: 'com.icontrol.co2',
                    optional: true,
                    modes: ['read', 'dynamic', 'emitEvents'],
                    prerequisites: [CL_CO2_MEASUREMENT],
                    seed: function(args) {
                        return SbmdUtils.result()
                            .dataModel.updateResource(RES_CO2, '0')
                            .success();
                    }
                },
                pm25Concentration: {
                    type: 'com.icontrol.ugm3',
                    optional: true,
                    modes: ['read', 'dynamic', 'emitEvents'],
                    prerequisites: [CL_PM25_MEASUREMENT],
                    seed: function(args) {
                        return SbmdUtils.result()
                            .dataModel.updateResource(RES_PM25, '0.0')
                            .success();
                    }
                }
            }
        }
    },

    attributeHandlers: {
        handleAirQuality: {
            aliases: ['airQualityValue'],
            handler: function(args) {
                var value = SbmdUtils.Tlv.decode(args.attribute.tlvBase64);
                var levels = ['unknown', 'good', 'fair', 'moderate', 'poor', 'veryPoor', 'extremelyPoor'];

                return SbmdUtils.result()
                    .dataModel.updateResource(RES_AIR_QUALITY, levels[value] || 'unknown')
                    .success();
            }
        },
        handleTemperature: {
            aliases: ['tempMeasuredValue'],
            handler: function(args) {
                var value = SbmdUtils.Tlv.decode(args.attribute.tlvBase64);

                if (value === null || value === -32768) {
                    return SbmdUtils.result()
                        .error('TLV decode failed for MeasuredValue');
                }

                return SbmdUtils.result()
                    .dataModel.updateResource(RES_TEMPERATURE, value.toString())
                    .success();
            }
        },
        handleHumidity: {
            aliases: ['humidityMeasuredValue'],
            handler: function(args) {
                var value = SbmdUtils.Tlv.decode(args.attribute.tlvBase64);

                if (value === null || value === 0xFFFF) {
                    return SbmdUtils.result()
                        .error('TLV decode failed for MeasuredValue');
                }

                var percent = Math.round(value / 100);

                return SbmdUtils.result()
                    .dataModel.updateResource(RES_HUMIDITY, percent.toString())
                    .success();
            }
        },
        handleCO2: {
            aliases: ['co2MeasuredValue'],
            handler: function(args) {
                var value = SbmdUtils.Tlv.decode(args.attribute.tlvBase64);

                if (value === null) {
                    return SbmdUtils.result().success();
                }

                return SbmdUtils.result()
                    .dataModel.updateResource(RES_CO2, Math.round(value).toString())
                    .success();
            }
        },
        handlePM25: {
            aliases: ['pm25MeasuredValue'],
            handler: function(args) {
                var value = SbmdUtils.Tlv.decode(args.attribute.tlvBase64);

                if (value === null) {
                    return SbmdUtils.result().success();
                }

                return SbmdUtils.result()
                    .dataModel.updateResource(RES_PM25, value.toFixed(1))
                    .success();
            }
        }
    }
});
