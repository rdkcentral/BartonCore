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
 * DeferredCmdTestDevice - A matter.js virtual device for deferred-operation
 * integration testing.
 *
 * Uses the Dimmable Plug-in Unit device type (0x010B), which is not claimed by
 * any production SBMD driver.  The OnOff cluster is present so that Barton can
 * issue an OnOff Toggle command via requestCommand, exercising the SBMD deferred
 * code path.
 *
 * NOTE: This device exists solely to support the SBMD deferred-metrics
 * integration tests (see deferred-command-test.sbmd.js).  A future production
 * SBMD driver that naturally uses requestCommand can replace it.
 *
 * Side-band operations: getState
 * Can be run directly:  node DeferredCmdTestDevice.js --passcode ... --discriminator ...
 */

import {pathToFileURL} from 'node:url';
import {Endpoint} from '@matter/main';
import {DimmablePlugInUnitDevice, DimmablePlugInUnitRequirements} from '@matter/main/devices';
import {VirtualDevice} from './VirtualDevice.js';
import {parseArgs} from './parseArgs.js';

export class DeferredCmdTestDevice extends VirtualDevice {
    constructor(options = {}) {
        super({
            deviceName: 'Virtual Deferred Cmd Test Device',
            ...options
        });

        this.registerOperation('getState', () => this.handleGetState());
    }

    getDeviceType() {
        // Matter Dimmable Plug-in Unit device type ID (not claimed by any production driver)
        return 0x010b;
    }

    createEndpoints() {
        return [
            new Endpoint(
                DimmablePlugInUnitDevice.with(
                    DimmablePlugInUnitRequirements.OnOffServer,
                    DimmablePlugInUnitRequirements.LevelControlServer
                ),
                {
                    id: 'test-ep1',
                    onOff: {
                        onOff: false
                    },
                    levelControl: {
                        currentLevel: 1,
                        minLevel: 1,
                        maxLevel: 254,
                        onLevel: 254
                    }
                }
            )
        ];
    }

    async handleGetState() {
        let onOff;

        await this.endpoints[0].act(async (agent) => {
            onOff = agent.onOff.state.onOff;
        });

        return {onOff};
    }
}

// Entry point when run directly
if (import.meta.url === pathToFileURL(process.argv[1]).href) {
    const config = parseArgs(process.argv);
    const device = new DeferredCmdTestDevice(config);
    await device.start();
}
