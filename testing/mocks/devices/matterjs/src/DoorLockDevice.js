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
 * DoorLockDevice - A matter.js virtual door lock for integration testing.
 *
 * Extends VirtualDevice with:
 *   - DoorLock device type (0x000A) with DoorLock cluster on endpoint 1
 *   - Side-band operations: lock, unlock, getState
 *   - Initial state: locked
 */

import { Endpoint } from "@matter/main";
import { DoorLockDevice as MatterDoorLockDevice, DoorLockRequirements } from "@matter/main/devices";
import { DoorLock } from "@matter/main/clusters";
import { VirtualDevice } from "./VirtualDevice.js";

export class DoorLockDevice extends VirtualDevice {
    constructor(options = {}) {
        super({
            deviceName: "Virtual Door Lock",
            ...options,
        });

        this.registerOperation("lock", () => this.handleLock());
        this.registerOperation("unlock", () => this.handleUnlock());
        this.registerOperation("getState", () => this.handleGetState());
    }

    getDeviceType() {
        // Matter Door Lock device type ID
        return 0x000a;
    }

    createEndpoint() {
        return new Endpoint(MatterDoorLockDevice.with(DoorLockRequirements.DoorLockServer), {
            id: "doorlock-ep1",
            doorLock: {
                lockState: DoorLock.LockState.Locked,
                lockType: DoorLock.LockType.DeadBolt,
                actuatorEnabled: true,
                operatingMode: DoorLock.OperatingMode.Normal,
                supportedOperatingModes: {
                    normal: true,
                    vacation: true,
                    privacy: true,
                    noRemoteLockUnlock: true,
                    passage: true,
                    alwaysSet: 2047,
                },
            },
        });
    }

    async handleLock() {
        await this.endpoint.act(async (agent) => {
            agent.doorLock.state.lockState = DoorLock.LockState.Locked;
        });

        return { lockState: "locked" };
    }

    async handleUnlock() {
        await this.endpoint.act(async (agent) => {
            agent.doorLock.state.lockState = DoorLock.LockState.Unlocked;
        });

        return { lockState: "unlocked" };
    }

    async handleGetState() {
        let lockState;

        await this.endpoint.act(async (agent) => {
            const state = agent.doorLock.state;
            const lockStateValue = state.lockState;

            switch (lockStateValue) {
                case DoorLock.LockState.Locked:
                    lockState = "locked";
                    break;
                case DoorLock.LockState.Unlocked:
                    lockState = "unlocked";
                    break;
                case DoorLock.LockState.Unlatched:
                    lockState = "unlatched";
                    break;
                default:
                    lockState = "notFullyLocked";
                    break;
            }
        });

        return {
            lockState,
            users: [],
            pinCodes: [],
        };
    }
}
