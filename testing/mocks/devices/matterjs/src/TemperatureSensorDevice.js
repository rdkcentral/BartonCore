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
 * TemperatureSensorDevice - A matter.js virtual temperature sensor
 * for integration testing.
 *
 * Extends VirtualDevice with:
 *   - Temperature Sensor device type (0x0302) with TemperatureMeasurement cluster
 *   - Side-band operations: setTemperature, getState
 *   - Initial state: 25.50°C (raw value 2550)
 *
 * Can be run directly:  node TemperatureSensorDevice.js --passcode ... --discriminator ...
 */

import { pathToFileURL } from "node:url";
import { Endpoint } from "@matter/main";
import {
    TemperatureSensorDevice as MatterTemperatureSensorDevice,
    TemperatureSensorRequirements,
} from "@matter/main/devices";
import { VirtualDevice } from "./VirtualDevice.js";
import { parseArgs } from "./parseArgs.js";

export class TemperatureSensorDevice extends VirtualDevice {
    constructor(options = {}) {
        super({
            deviceName: "Virtual Temperature Sensor",
            ...options,
        });

        this.registerOperation("setTemperature", (params) => this.handleSetTemperature(params));
        this.registerOperation("getState", () => this.handleGetState());
    }

    getDeviceType() { return 0x0302; }

    createEndpoints() {
        return [
            new Endpoint(
                MatterTemperatureSensorDevice.with(
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
            )
        ];
    }

    async handleSetTemperature({ value }) {
        await this.endpoints[0].act(async (agent) => {
            agent.temperatureMeasurement.state.measuredValue = value;
        });

        return { measuredValue: value };
    }

    async handleGetState() {
        let temperature;

        await this.endpoints[0].act(async (agent) => {
            temperature = agent.temperatureMeasurement.state.measuredValue;
        });

        return { temperature };
    }
}

// Run directly if this is the entry point
const thisUrl = pathToFileURL(process.argv[1]).href;

// Entry point when run directly
if (import.meta.url === thisUrl) {
    const config = parseArgs(process.argv);
    const device = new TemperatureSensorDevice(config);
    await device.start();
}
