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
 * ContactSensorDevice - A matter.js virtual contact sensor for integration
 * testing.
 *
 * Extends VirtualDevice with:
 *   - Contact Sensor device type (0x0015) with BooleanState cluster on endpoint 1
 *   - Side-band operations: setStateValue, getState
 *   - Initial state: closed (stateValue=true, not faulted)
 *
 * BooleanState.StateChange is optional on the base Contact Sensor device, so it
 * is enabled here; the BooleanStateServer then emits it automatically whenever
 * stateValue changes.
 *
 * Can be run directly:  node ContactSensorDevice.js --passcode ... --discriminator ...
 */

import {pathToFileURL} from 'node:url';
import {Endpoint} from '@matter/main';
import {ContactSensorDevice as MatterContactSensorDevice, ContactSensorRequirements} from '@matter/main/devices';
import {VirtualDevice} from './VirtualDevice.js';
import {parseArgs} from './parseArgs.js';

const BooleanStateServerWithEvents = ContactSensorRequirements.BooleanStateServer.alter({
    events: {stateChange: {optional: false}}
});

export class ContactSensorDevice extends VirtualDevice {
    constructor(options = {}) {
        super({
            deviceName: 'Virtual Contact Sensor',
            ...options
        });

        // StateValue=true means closed (contact present / not faulted)
        this.initialStateValue = true;

        this.registerOperation('setStateValue', ({stateValue}) => this.handleSetStateValue(stateValue));
        this.registerOperation('getState', () => this.handleGetState());
    }

    getDeviceType() {
        return 0x0015;
    }

    createEndpoints() {
        return [
            new Endpoint(MatterContactSensorDevice.with(BooleanStateServerWithEvents), {
                id: 'contact-ep1',
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
    const device = new ContactSensorDevice(config);
    await device.start();
}
