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
// Command Echo Test Driver
//
// A contrived SBMD driver used exclusively for testing the command handler
// pipeline. It registers commandHandlers that receive incoming commands
// and reflect their data into resources, allowing integration tests to
// verify the full command dispatch flow.
//
// This driver is NOT a production device driver. It lives in
// testing/resources/sbmd-specs/ and is only loaded when the test specs
// directory is included in the SBMD dirs configuration.
//

SbmdDriver({
    schemaVersion: '4.0',
    driverVersion: 1,
    name: 'Command Echo Test',

    constants: {
        // Use a fake cluster ID that won't conflict with real clusters
        CL_TEST: 0xfff10000,

        // Command IDs
        CMD_ECHO: 0x00,
        CMD_PING: 0x01,

        // Attribute for basic attribute handler test
        ATTR_STATUS: 0x00,

        EP: '1',
        RES_LAST_COMMAND: 'lastCommand',
        RES_ECHO_DATA: 'echoData',
        RES_STATUS: 'status'
    },

    barton: {
        deviceClass: 'commandEchoTest',
        deviceClassVersion: 1
    },

    matter: {
        deviceTypes: [0xfff10000],
        defaultTimeoutMs: 15000
    },

    reporting: {
        minSecs: 1,
        maxSecs: 300
    },

    aliases: {
        echoCmd: {clusterId: CL_TEST, commandId: CMD_ECHO},
        pingCmd: {clusterId: CL_TEST, commandId: CMD_PING},
        anyTestCmd: {clusterId: CL_TEST},
        testStatus: {clusterId: CL_TEST, attributeId: ATTR_STATUS, type: 'uint8'}
    },

    endpoints: {
        '1': {
            profile: 'commandEchoTest',
            profileVersion: 1,
            resources: {
                lastCommand: {
                    type: 'com.icontrol.string',
                    modes: ['read'],
                    seed: function (args) {
                        return Sbmd.result()
                            .dataModel.updateResource(EP, RES_LAST_COMMAND, 'none')
                            .success();
                    }
                },
                echoData: {
                    type: 'com.icontrol.string',
                    modes: ['read'],
                    seed: function (args) {
                        return Sbmd.result()
                            .dataModel.updateResource(EP, RES_ECHO_DATA, '')
                            .success();
                    }
                },
                status: {
                    type: 'com.icontrol.string',
                    modes: ['read'],
                    seed: function (args) {
                        return Sbmd.result()
                            .dataModel.updateResource(EP, RES_STATUS, '0')
                            .success();
                    }
                }
            }
        }
    },

    attributeHandlers: {
        handleStatus: {
            aliases: ['testStatus'],
            handler: function (args) {
                var value = Sbmd.Tlv.decode(args.attribute.tlvBase64);

                if (value === null) {
                    return Sbmd.result().error('TLV decode failed for Status');
                }

                return Sbmd.result()
                    .dataModel.updateResource(EP, RES_STATUS, String(value))
                    .success();
            }
        }
    },

    commandHandlers: {
        handleEcho: {
            aliases: ['echoCmd'],
            handler: handleEchoCommand
        },
        handlePing: {
            aliases: ['pingCmd'],
            handler: handlePingCommand
        },
        handleAnyCommand: {
            aliases: ['anyTestCmd'],
            handler: handleWildcardCommand
        }
    }
});

function handleEchoCommand(args) {
    return Sbmd.result()
        .dataModel.updateResource(EP, RES_LAST_COMMAND, 'echo')
        .dataModel.updateResource(EP, RES_ECHO_DATA, args.command.tlvBase64 || '')
        .success();
}

function handlePingCommand(args) {
    return Sbmd.result().dataModel.updateResource(EP, RES_LAST_COMMAND, 'ping').success();
}

function handleWildcardCommand(args) {
    // Wildcard handler records the raw command ID
    return Sbmd.result()
        .log(
            'wildcard command: clusterId=' +
                args.command.clusterId +
                ' commandId=' +
                args.command.commandId
        )
        .success();
}
