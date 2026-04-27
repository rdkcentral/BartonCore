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

/**
 * ThermostatWithFanDevice - A matter.js virtual thermostat with Fan Control
 * cluster support for integration testing.
 *
 * Extends ThermostatDevice to add:
 *   - Fan Control cluster (0x0202) on the thermostat endpoint
 *   - Side-band operations: setFanMode, getFanState
 *   - Initial fan state: FanMode=Auto
 *
 * Can be run directly:  node ThermostatWithFanDevice.js --passcode ... --discriminator ...
 */

import { pathToFileURL } from "node:url";
import { ThermostatDevice as MatterThermostatDevice, ThermostatRequirements } from "@matter/main/devices";
import { FanControlServer } from "@matter/main/behaviors/fan-control";
import { FanControl } from "@matter/main/clusters";
import { ThermostatDevice } from "./ThermostatDevice.js";
import { parseArgs } from "./parseArgs.js";

export class ThermostatWithFanDevice extends ThermostatDevice {
    constructor(options = {}) {
        super({
            deviceName: "Virtual Thermostat With Fan",
            ...options,
        });

        this.registerOperation("setFanMode", (params) => this.handleSetFanMode(params));
        this.registerOperation("getFanState", () => this.handleGetFanState());
    }

    getDeviceTypeDefinition() {
        return MatterThermostatDevice.with(
            ThermostatRequirements.ThermostatServer.with("Heating", "Cooling", "AutoMode"),
            FanControlServer.with("MultiSpeed", "Auto"),
        );
    }

    getEndpointConfig() {
        const config = super.getEndpointConfig();

        config.fanControl = {
            fanMode: FanControl.FanMode.Auto,
            fanModeSequence: FanControl.FanModeSequence.OffLowMedHighAuto,
            percentSetting: 0,
            percentCurrent: 0,
            speedMax: 3,
            speedSetting: 0,
            speedCurrent: 0,
        };

        return config;
    }

    async handleSetFanMode(params) {
        const mode = params.mode;

        const modeMap = {
            off: FanControl.FanMode.Off,
            low: FanControl.FanMode.Low,
            medium: FanControl.FanMode.Medium,
            high: FanControl.FanMode.High,
            on: FanControl.FanMode.On,
            auto: FanControl.FanMode.Auto,
            smart: FanControl.FanMode.Smart,
        };

        const modeValue = modeMap[mode];

        if (modeValue === undefined) {
            return { error: `Unknown fan mode: ${mode}` };
        }

        await this.endpoint.act(async (agent) => {
            agent.fanControl.state.fanMode = modeValue;
        });

        return { fanMode: mode };
    }

    async handleGetFanState() {
        let state = {};

        await this.endpoint.act(async (agent) => {
            const fan = agent.fanControl.state;

            const modeNames = {
                [FanControl.FanMode.Off]: "off",
                [FanControl.FanMode.Low]: "low",
                [FanControl.FanMode.Medium]: "medium",
                [FanControl.FanMode.High]: "high",
                [FanControl.FanMode.On]: "on",
                [FanControl.FanMode.Auto]: "auto",
                [FanControl.FanMode.Smart]: "smart",
            };

            state = {
                fanMode: modeNames[fan.fanMode] || fan.fanMode.toString(),
                percentSetting: fan.percentSetting,
                percentCurrent: fan.percentCurrent,
            };
        });

        return state;
    }
}

// Entry point when run directly
if (import.meta.url === pathToFileURL(process.argv[1]).href) {
    const config = parseArgs(process.argv);
    const device = new ThermostatWithFanDevice(config);
    await device.start();
}
