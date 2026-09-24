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
 * WaterLeakDetectorDevice - A matter.js virtual water leak detector for
 * integration testing.
 *
 * Extends VirtualDevice with:
 *   - Water Leak Detector device type (0x0043) with BooleanState cluster on endpoint 1
 *   - Side-band operations: setStateValue, getState
 *   - Initial state: dry (stateValue=false, not faulted)
 *
 * The Water Leak Detector device type requires BooleanState.StateChange, so the
 * BooleanStateServer emits it automatically whenever stateValue changes.
 *
 * Can be run directly:  node WaterLeakDetectorDevice.js --passcode ... --discriminator ...
 */

import {pathToFileURL} from 'node:url';
import {Endpoint} from '@matter/main';
import {WaterLeakDetectorDevice as MatterWaterLeakDetectorDevice} from '@matter/main/devices';
import {VirtualDevice} from './VirtualDevice.js';
import {parseArgs} from './parseArgs.js';

export class WaterLeakDetectorDevice extends VirtualDevice {
    constructor(options = {}) {
        super({
            deviceName: 'Virtual Water Leak Detector',
            ...options
        });

        // StateValue=true means water detected (faulted)
        this.initialStateValue = false;

        this.registerOperation('setStateValue', ({stateValue}) => this.handleSetStateValue(stateValue));
        this.registerOperation('getState', () => this.handleGetState());
    }

    getDeviceType() {
        return 0x0043;
    }

    createEndpoints() {
        return [
            new Endpoint(MatterWaterLeakDetectorDevice, {
                id: 'water-leak-ep1',
                booleanState: {
                    stateValue: this.initialStateValue
                }
            })
        ];
    }

    async handleSetStateValue(stateValue) {
        await this.endpoints[0].act(async (agent) => {
            agent.booleanState.state.stateValue = stateValue;
        });

        return {stateValue};
    }

    async handleGetState() {
        let stateValue;

        await this.endpoints[0].act(async (agent) => {
            stateValue = agent.booleanState.state.stateValue;
        });

        return {stateValue};
    }
}

// Entry point when run directly
if (import.meta.url === pathToFileURL(process.argv[1]).href) {
    const config = parseArgs(process.argv);
    const device = new WaterLeakDetectorDevice(config);
    await device.start();
}
