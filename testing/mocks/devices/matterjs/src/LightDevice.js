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
 * LightDevice - A matter.js virtual dimmable light for integration testing.
 *
 * Extends VirtualDevice with:
 *   - Dimmable Light device type (0x0101) with OnOff and LevelControl clusters
 *   - Side-band operations: on, off, toggle, getState
 *   - Initial state: off, level 1
 *
 * Can be run directly:  node LightDevice.js --passcode ... --discriminator ...
 */

import { pathToFileURL } from "node:url";
import { Endpoint } from "@matter/main";
import { DimmableLightDevice, DimmableLightRequirements } from "@matter/main/devices";
import { VirtualDevice } from "./VirtualDevice.js";
import { parseArgs } from "./parseArgs.js";

export class LightDevice extends VirtualDevice {
    constructor(options = {}) {
        super({
            deviceName: "Virtual Dimmable Light",
            ...options,
        });

        this.registerOperation("on", () => this.handleOn());
        this.registerOperation("off", () => this.handleOff());
        this.registerOperation("toggle", () => this.handleToggle());
        this.registerOperation("getState", () => this.handleGetState());
        this.registerOperation("setIdentifyTime", (params) => this.handleSetIdentifyTime(params));
    }

    getDeviceType() {
        // Matter Dimmable Light device type ID
        return 0x0101;
    }

    createEndpoints() {
        return [
            new Endpoint(
                DimmableLightDevice.with(
                    DimmableLightRequirements.OnOffServer,
                    DimmableLightRequirements.LevelControlServer,
                ),
                {
                    id: "light-ep1",
                    onOff: {
                        onOff: false,
                    },
                    levelControl: {
                        currentLevel: 1,
                        minLevel: 1,
                        maxLevel: 254,
                        onLevel: 254,
                    },
                },
            )
        ];
    }

    async handleOn() {
        await this.endpoints[0].act(async (agent) => {
            agent.onOff.state.onOff = true;
        });

        return { onOff: true };
    }

    async handleOff() {
        await this.endpoints[0].act(async (agent) => {
            agent.onOff.state.onOff = false;
        });

        return { onOff: false };
    }

    async handleToggle() {
        let newState;

        await this.endpoints[0].act(async (agent) => {
            newState = !agent.onOff.state.onOff;
            agent.onOff.state.onOff = newState;
        });

        return { onOff: newState };
    }

    async handleGetState() {
        let onOff;
        let currentLevel;

        await this.endpoints[0].act(async (agent) => {
            onOff = agent.onOff.state.onOff;
            currentLevel = agent.levelControl.state.currentLevel;
        });

        return {
            onOff,
            currentLevel,
        };
    }

    async handleSetIdentifyTime({ identifyTime } = {}) {
        if (typeof identifyTime !== "number" || !Number.isInteger(identifyTime) || identifyTime < 0 || identifyTime > 65535) {
            throw new Error(`Invalid identifyTime: ${identifyTime}. Must be a uint16 (0-65535).`);
        }

        await this.endpoints[0].act(async (agent) => {
            agent.identify.state.identifyTime = identifyTime;
        });

        return { identifyTime };
    }
}

// Entry point when run directly
if (import.meta.url === pathToFileURL(process.argv[1]).href) {
    const config = parseArgs(process.argv);
    const device = new LightDevice(config);
    await device.start();
}
