// ------------------------------ tabstop = 4 ----------------------------------
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
// ------------------------------ tabstop = 4 ----------------------------------

//
// Occupancy Sensor SBMD Driver
//
// Maps Matter Occupancy Sensor device type to Barton sensor device class.
// Occupancy bitmap: bit 0 = occupied.
//

SbmdDriver({
    schemaVersion: '4.0',
    driverVersion: 1,
    name: 'Occupancy Sensor',

    constants: {
        CL_OCCUPANCY_SENSING: 0x0406,
        ATTR_OCCUPANCY: 0x0000,
        RES_FAULTED: 'faulted'
    },

    barton: {
        deviceClass: 'sensor',
        deviceClassVersion: 1
    },

    matter: {
        deviceTypes: [0x0107],
        revision: 1
    },

    reporting: {
        minSecs: 1,
        maxSecs: 3600
    },

    aliases: {
        occupancy: {
            clusterId: CL_OCCUPANCY_SENSING,
            attributeId: ATTR_OCCUPANCY,
            type: 'uint8'
        }
    },

    endpoints: {
        '1': {
            profile: 'sensor',
            profileVersion: 2,
            resources: {
                faulted: {
                    type: 'com.icontrol.boolean',
                    modes: ['read'],
                    prerequisites: [CL_OCCUPANCY_SENSING]
                }
            }
        }
    },

    attributeHandlers: {
        handleOccupancy: {
            aliases: ['occupancy'],
            handler: function (args) {
                var value = Sbmd.Tlv.decode(args.attribute.tlvBase64);

                // Bit 0 of occupancy bitmap = occupied = faulted
                var occupied = (value & 0x01) !== 0;

                return Sbmd.result()
                    .dataModel.updateResource(args.endpointId, RES_FAULTED, occupied ? 'true' : 'false')
                    .success();
            }
        }
    }
});
