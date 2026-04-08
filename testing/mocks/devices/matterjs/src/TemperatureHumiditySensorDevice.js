//------------------------------ tabstop = 4 ----------------------------------
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
//------------------------------ tabstop = 4 ----------------------------------

//
// Created by Kevin Funderburg on 04/08/2026
//

/**
 * TemperatureHumiditySensorDevice - A matter.js virtual composite sensor
 * for integration testing.
 *
 * Extends VirtualDevice with two endpoints:
 *   - EP 1: Temperature Sensor (device type 0x0302) with TemperatureMeasurement cluster
 *   - EP 2: Humidity Sensor (device type 0x0307) with RelativeHumidityMeasurement cluster
 *
 * Side-band operations:
 *   - setTemperature({ value }): Set temperature in 0.01°C units
 *   - setHumidity({ value }): Set humidity in 0.01% units
 *   - getState: Get current temperature and humidity values
 *
 * Can be run directly:  node TemperatureHumiditySensorDevice.js --passcode ... --discriminator ...
 */

import {Endpoint} from "@matter/main";
import {
    HumiditySensorDevice,
    HumiditySensorRequirements,
    TemperatureSensorDevice,
    TemperatureSensorRequirements,
} from "@matter/main/devices";
import {pathToFileURL} from "node:url";

import {parseArgs} from "./parseArgs.js";
import {VirtualDevice} from "./VirtualDevice.js";

export class TemperatureHumiditySensorDevice extends VirtualDevice
{
    constructor(options = {})
    {
        super({
            // Abbreviated to fit Matter's 32-char productName limit
            deviceName: "Virtual TempHumidity Sensor",
            ...options,
        });

        this.registerOperation("setTemperature", (params) => this.handleSetTemperature(params));
        this.registerOperation("setHumidity", (params) => this.handleSetHumidity(params));
        this.registerOperation("getState", () => this.handleGetState());
    }

    // productDescription.deviceType is used for commissioning advertisements — it
    // tells commissioners what kind of device this is before they connect. The
    // Matter spec only allows a single value here, so for a composite device we
    // pick 0x0302 (temperature sensor) as the primary. This matches what the SDK
    // would infer automatically from the first child endpoint.
    getDeviceType() { return 0x0302; }

    createEndpoints()
    {
        const tempEndpoint = new Endpoint(
            TemperatureSensorDevice.with(
                TemperatureSensorRequirements.TemperatureMeasurementServer,
                ),
            {
                id: "temperature-ep1",
                temperatureMeasurement: {
                                         measuredValue: 2550,
                                         minMeasuredValue: -4000,
                                         maxMeasuredValue: 12500,
                                         },
        },
        );

        const humidityEndpoint = new Endpoint(
            HumiditySensorDevice.with(
                HumiditySensorRequirements.RelativeHumidityMeasurementServer,
                ),
            {
                id: "humidity-ep2",
                relativeHumidityMeasurement: {
                                              measuredValue: 5000,
                                              minMeasuredValue: 0,
                                              maxMeasuredValue: 10000,
                                              },
        },
        );

        return [tempEndpoint, humidityEndpoint];
    }

    async handleSetTemperature({value})
    {
        await this.endpoints[0].act(async (agent) => { agent.temperatureMeasurement.state.measuredValue = value; });

        return {measuredValue: value};
    }

    async handleSetHumidity({value})
    {
        await this.endpoints[1].act(
            async (agent) => { agent.relativeHumidityMeasurement.state.measuredValue = value; });

        return {measuredValue: value};
    }

    async handleGetState()
    {
        let temperature, humidity;

        await this.endpoints[0].act(
            async (agent) => { temperature = agent.temperatureMeasurement.state.measuredValue; });

        await this.endpoints[1].act(
            async (agent) => { humidity = agent.relativeHumidityMeasurement.state.measuredValue; });

        return {temperature, humidity};
    }
}

// Run directly if this is the entry point
const thisUrl = pathToFileURL(process.argv[1]).href;

// Entry point when run directly
if (import.meta.url === thisUrl)
{
    const config = parseArgs(process.argv);
    const device = new TemperatureHumiditySensorDevice(config);
    await device.start();
}
