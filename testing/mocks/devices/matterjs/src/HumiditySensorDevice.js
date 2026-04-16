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
 * HumiditySensorDevice - A matter.js virtual humidity sensor
 * for integration testing.
 *
 * Extends VirtualDevice with:
 *   - Humidity Sensor device type (0x0307) with RelativeHumidityMeasurement cluster
 *   - Side-band operations: setHumidity, getState
 *   - Initial state: 50.00% (raw value 5000)
 *
 * Can be run directly:  node HumiditySensorDevice.js --passcode ... --discriminator ...
 */

import {Endpoint} from "@matter/main";
import {
    HumiditySensorDevice as MatterHumiditySensorDevice,
    HumiditySensorRequirements,
} from "@matter/main/devices";
import {pathToFileURL} from "node:url";

import {parseArgs} from "./parseArgs.js";
import {VirtualDevice} from "./VirtualDevice.js";

export class HumiditySensorDevice extends VirtualDevice
{
    constructor(options = {})
    {
        super({
            deviceName: "Virtual Humidity Sensor",
            ...options,
        });

        this.registerOperation("setHumidity", (params) => this.handleSetHumidity(params));
        this.registerOperation("getState", () => this.handleGetState());
    }

    getDeviceType() { return 0x0307; }

    createEndpoints()
    {
        return [new Endpoint(
            MatterHumiditySensorDevice.with(
                HumiditySensorRequirements.RelativeHumidityMeasurementServer,
                ),
            {
                id: "humidity-ep1",
                relativeHumidityMeasurement: {
                                              measuredValue: 5000,
                                              minMeasuredValue: 0,
                                              maxMeasuredValue: 10000,
                                              },
        },
            )];
    }

    async handleSetHumidity({value})
    {
        await this.endpoints[0].act(
            async (agent) => { agent.relativeHumidityMeasurement.state.measuredValue = value; });

        return {measuredValue: value};
    }

    async handleGetState()
    {
        let humidity;

        await this.endpoints[0].act(
            async (agent) => { humidity = agent.relativeHumidityMeasurement.state.measuredValue; });

        return {humidity};
    }
}

// Run directly if this is the entry point
const thisUrl = pathToFileURL(process.argv[1]).href;

// Entry point when run directly
if (import.meta.url === thisUrl)
{
    const config = parseArgs(process.argv);
    const device = new HumiditySensorDevice(config);
    await device.start();
}
