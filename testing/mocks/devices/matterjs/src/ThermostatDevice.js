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
 * ThermostatDevice - A matter.js virtual thermostat for integration testing.
 *
 * Extends VirtualDevice with:
 *   - Thermostat device type (0x0301) with Thermostat cluster on endpoint 1
 *   - Side-band operations: setTemperature, setSystemMode, getState
 *   - Initial state: LocalTemperature=2100, OccupiedHeatingSetpoint=2000,
 *     OccupiedCoolingSetpoint=2600, SystemMode=Off
 *
 * Can be run directly:  node ThermostatDevice.js --passcode ... --discriminator ...
 */

import { pathToFileURL } from "node:url";
import { Endpoint } from "@matter/main";
import { ThermostatDevice as MatterThermostatDevice, ThermostatRequirements } from "@matter/main/devices";
import { Thermostat } from "@matter/main/clusters";
import { VirtualDevice } from "./VirtualDevice.js";
import { parseArgs } from "./parseArgs.js";

export class ThermostatDevice extends VirtualDevice {
    constructor(options = {}) {
        super({
            deviceName: "Virtual Thermostat",
            ...options,
        });

        this.registerOperation("setTemperature", (params) => this.handleSetTemperature(params));
        this.registerOperation("setSystemMode", (params) => this.handleSetSystemMode(params));
        this.registerOperation("getState", () => this.handleGetState());
    }

    getDeviceType() {
        return 0x0301;
    }

    getDeviceTypeDefinition() {
        return MatterThermostatDevice.with(ThermostatRequirements.ThermostatServer.with("Heating", "Cooling", "AutoMode"));
    }

    getEndpointConfig() {
        return {
            id: "thermostat-ep1",
            thermostat: {
                localTemperature: 2100,
                occupiedHeatingSetpoint: 2000,
                occupiedCoolingSetpoint: 2600,
                absMinHeatSetpointLimit: 700,
                absMaxHeatSetpointLimit: 3000,
                absMinCoolSetpointLimit: 1600,
                absMaxCoolSetpointLimit: 3200,
                controlSequenceOfOperation: Thermostat.ControlSequenceOfOperation.CoolingAndHeating,
                systemMode: Thermostat.SystemMode.Off,
                minSetpointDeadBand: 10,
            },
        };
    }

    createEndpoint() {
        return new Endpoint(this.getDeviceTypeDefinition(), this.getEndpointConfig());
    }

    async handleSetTemperature(params) {
        const temperature = params.temperature;

        if (temperature === undefined || temperature === null) {
            return { error: "temperature parameter required" };
        }

        await this.endpoint.act(async (agent) => {
            agent.thermostat.state.localTemperature = temperature;
        });

        return { localTemperature: temperature };
    }

    async handleSetSystemMode(params) {
        const mode = params.mode;

        const modeMap = {
            off: Thermostat.SystemMode.Off,
            auto: Thermostat.SystemMode.Auto,
            cool: Thermostat.SystemMode.Cool,
            heat: Thermostat.SystemMode.Heat,
        };

        const modeValue = modeMap[mode];

        if (modeValue === undefined) {
            return { error: `Unknown mode: ${mode}` };
        }

        await this.endpoint.act(async (agent) => {
            agent.thermostat.state.systemMode = modeValue;
        });

        return { systemMode: mode };
    }

    async handleGetState() {
        let state = {};

        await this.endpoint.act(async (agent) => {
            const tstat = agent.thermostat.state;

            const modeNames = {
                [Thermostat.SystemMode.Off]: "off",
                [Thermostat.SystemMode.Auto]: "auto",
                [Thermostat.SystemMode.Cool]: "cool",
                [Thermostat.SystemMode.Heat]: "heat",
                [Thermostat.SystemMode.EmergencyHeat]: "emergencyHeat",
                [Thermostat.SystemMode.Precooling]: "precooling",
                [Thermostat.SystemMode.FanOnly]: "fanOnly",
            };

            state = {
                localTemperature: tstat.localTemperature,
                occupiedHeatingSetpoint: tstat.occupiedHeatingSetpoint,
                occupiedCoolingSetpoint: tstat.occupiedCoolingSetpoint,
                systemMode: modeNames[tstat.systemMode] || tstat.systemMode.toString(),
                absMinHeatSetpointLimit: tstat.absMinHeatSetpointLimit,
                absMaxHeatSetpointLimit: tstat.absMaxHeatSetpointLimit,
                absMinCoolSetpointLimit: tstat.absMinCoolSetpointLimit,
                absMaxCoolSetpointLimit: tstat.absMaxCoolSetpointLimit,
            };
        });

        return state;
    }
}

// Entry point when run directly
if (import.meta.url === pathToFileURL(process.argv[1]).href) {
    const config = parseArgs(process.argv);
    const device = new ThermostatDevice(config);
    await device.start();
}
