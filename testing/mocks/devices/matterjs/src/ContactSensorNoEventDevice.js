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
 * ContactSensorNoEventDevice - A matter.js virtual contact sensor that does NOT
 * emit BooleanState.StateChange.
 *
 * BooleanState.StateChange is optional conformance in Matter 1.5.1, so a
 * spec-conformant contact sensor may omit it and report state changes only via
 * the StateValue attribute. This device models exactly that case: it uses the
 * default (unaltered) Contact Sensor requirements, so the StateChange event is
 * not enabled and setStateValue reports only the attribute.
 *
 * It exists to verify that live fault reporting still works through the
 * driver's attribute handler when the event is absent.
 */

import {pathToFileURL} from 'node:url';
import {Endpoint} from '@matter/main';
import {ContactSensorDevice as MatterContactSensorDevice} from '@matter/main/devices';
import {VirtualDevice} from './VirtualDevice.js';
import {parseArgs} from './parseArgs.js';

export class ContactSensorNoEventDevice extends VirtualDevice {
    constructor(options = {}) {
        super({
            deviceName: 'Virtual Contact NoEvent',
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
        // No .alter enabling stateChange, so the optional event is not emitted.
        return [
            new Endpoint(MatterContactSensorDevice, {
                id: 'contact-noevt-ep1',
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
    const device = new ContactSensorNoEventDevice(config);
    await device.start();
}
