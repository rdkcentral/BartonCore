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

//
// Created by tlea on 12/4/25
//

#pragma once

#include "SbmdSpec.h"
#include "ScriptResult.h"
#include "lib/core/TLVReader.h"
#include <platform/CHIPDeviceLayer.h>

#include <map>
#include <optional>
#include <string>

namespace barton
{
    /**
     * This is the base class for SBMD scripts. Implementations can use whatever scripting
     * language or engine they wish, as long as they implement this interface.
     *
     * This class maps Barton resource strings to/from Matter attributes and command input/outputs.
     * Once Barton is converted to use more complex types than strings, this class will be updated.
     */
    class SbmdScript
    {
    public:
        SbmdScript(const std::string &deviceId) : deviceId(deviceId) {}

        virtual ~SbmdScript() = default;

        /**
         * Set the cluster feature maps for this script context.
         * These are looked up from the device cache and passed to all mapper scripts.
         *
         * @param maps Map of clusterId to featureMap
         */
        virtual void SetClusterFeatureMaps(const std::map<uint32_t, uint32_t> &maps) = 0;

        virtual bool AddAttributeReadMapper(const SbmdAttribute &attributeInfo,
                                            const std::string &script) = 0;

        virtual bool AddCommandExecuteResponseMapper(const SbmdCommand &commandInfo,
                                                     const std::string &script) = 0;

        /**
         * Convert a Matter attribute value to a Barton resource string value.
         *
         * Script input JSON:
         * {
         *     "tlvBase64": <base64-encoded TLV attribute value>,
         *     "deviceUuid": <device UUID>,
         *     "clusterFeatureMaps": { "<clusterId>": <featureMap>, ... },
         *     "clusterId": <cluster ID>,
         *     "endpointId": <endpoint ID>,
         *     "attributeId": <attribute ID>,
         *     "attributeName": <attribute name from spec>,
         *     "attributeType": <attribute type from spec>
         * }
         *
         * Script output JSON — one of:
         * { "value": <string|number|boolean> }  // update resource (non-string coerced to string)
         * { }  or  { "value": null }            // suppress — no update, no error
         * { "error": <string> }                 // signal a failure
         *
         * @param attributeInfo Information about the Matter attribute
         * @param reader TLV reader positioned at the attribute value
         * @return ScriptResult containing the mapped value, a suppress signal, or an error
         */
        virtual ScriptResult MapAttributeRead(const SbmdAttribute &attributeInfo, chip::TLV::TLVReader &reader) = 0;

        /**
         * Convert a Matter command response TLV to a Barton resource string value.
         * This is optional - only needed when a command returns data that should be
         * converted to a Barton string response.
         *
         * Script input JSON:
         * {
         *     "tlvBase64": <base64-encoded TLV command response>,
         *     "deviceUuid": <device UUID>,
         *     "clusterFeatureMaps": { "<clusterId>": <featureMap>, ... },
         *     "clusterId": <cluster ID>,
         *     "endpointId": <endpoint ID>,
         *     "commandId": <command ID>,
         *     "commandName": <command name from spec>
         * }
         *
         * Script output JSON — one of:
         * { "value": <string|number|boolean> }  // return response (non-string coerced to string)
         * { }  or  { "value": null }            // suppress — no response value
         * { "error": <string> }                 // signal a failure
         *
         * @param commandInfo Information about the Matter command
         * @param reader TLV reader positioned at the command response data
         * @return ScriptResult containing the mapped value, a suppress signal, or an error
         */
        virtual ScriptResult MapCommandExecuteResponse(const SbmdCommand &commandInfo,
                                                       chip::TLV::TLVReader &reader) = 0;

        /**
         * Add a write mapper script for the specified resource.
         * The script returns fully-specified operations (invoke or write) including cluster/command/attribute IDs.
         *
         * @param resourceKey Unique key identifying the resource (endpointId:resourceId)
         * @param script The JavaScript script for the mapper
         * @return true if the mapper was added successfully, false otherwise
         */
        virtual bool AddWriteMapper(const std::string &resourceKey, const std::string &script) = 0;

        /**
         * Add an execute mapper script for the specified resource.
         * The script returns fully-specified operations (invoke) including cluster/command IDs.
         *
         * @param resourceKey Unique key identifying the resource (endpointId:resourceId)
         * @param script The JavaScript script for the mapper
         * @param responseScript Optional response script for processing command responses
         * @return true if the mapper was added successfully, false otherwise
         */
        virtual bool AddExecuteMapper(const std::string &resourceKey,
                                      const std::string &script,
                                      const std::optional<std::string> &responseScript) = 0;

        /**
         * Execute a write mapper script and get the operation to perform.
         * The script returns either an 'invoke' (command) or 'write' (attribute) operation
         * with all necessary details including cluster ID, command/attribute ID, and TLV payload.
         *
         * Script input JSON:
         * {
         *     "input": <Barton string value to write>,
         *     "deviceUuid": <device UUID>,
         *     "clusterFeatureMaps": { "<clusterId>": <featureMap>, ... },
         *     "endpointId": <endpoint ID from resource, if specified>,
         *     "resourceId": <resource identifier>
         * }
         *
         * Script should return one of:
         *
         * For command invocation:
         * {
         *     "invoke": {
         *         "endpointId": <optional endpoint ID>,
         *         "clusterId": <cluster ID (number)>,
         *         "commandId": <command ID (number)>,
         *         "timedInvokeTimeoutMs": <optional timed invoke timeout>,
         *         "tlvBase64": <optional TLV-encoded command data as base64 string>
         *     }
         * }
         *
         * For attribute write:
         * {
         *     "write": {
         *         "endpointId": <optional endpoint ID>,
         *         "clusterId": <cluster ID (number)>,
         *         "attributeId": <attribute ID (number)>,
         *         "tlvBase64": <TLV-encoded attribute value as base64 string>
         *     }
         * }
         *
         * @param resourceKey Unique key identifying the resource (endpointId:resourceId)
         * @param endpointId The endpoint ID from the resource (may be empty for device-level)
         * @param resourceId The resource identifier
         * @param inValue Barton string representation of the value to write
         * @return ScriptResult containing the invoke/write operation, or an error
         */
        virtual ScriptResult MapWrite(const std::string &resourceKey,
                                      const std::string &endpointId,
                                      const std::string &resourceId,
                                      const std::string &inValue) = 0;

        /**
         * Execute an execute mapper script and get the operation to perform.
         * The script returns 'invoke' (command) operations with all details.
         *
         * Script input JSON:
         * {
         *     "input": <Barton string argument(s) for execute>,
         *     "deviceUuid": <device UUID>,
         *     "clusterFeatureMaps": { "<clusterId>": <featureMap>, ... },
         *     "endpointId": <endpoint ID from resource, if specified>,
         *     "resourceId": <resource identifier>
         * }
         *
         * Script output JSON (same format as MapWrite invoke):
         * {
         *     "invoke": {
         *         "endpointId": <optional endpoint ID>,
         *         "clusterId": <cluster ID (number)>,
         *         "commandId": <command ID (number)>,
         *         "timedInvokeTimeoutMs": <optional timed invoke timeout>,
         *         "tlvBase64": <optional TLV-encoded command data as base64 string>
         *     }
         * }
         *
         * @param resourceKey Unique key identifying the resource (endpointId:resourceId)
         * @param endpointId The endpoint ID from the resource (may be empty for device-level)
         * @param resourceId The resource identifier
         * @param inValue Barton string argument(s) for the execute
         * @return ScriptResult containing the invoke operation, or an error
         */
        virtual ScriptResult MapExecute(const std::string &resourceKey,
                                        const std::string &endpointId,
                                        const std::string &resourceId,
                                        const std::string &inValue) = 0;

        /**
         * Add an event mapper script for the specified event.
         * The script converts event TLV data to a Barton resource string value.
         *
         * @param eventInfo Information about the Matter event
         * @param script The JavaScript script for the mapper
         * @return true if the mapper was added successfully, false otherwise
         */
        virtual bool AddEventMapper(const SbmdEvent &eventInfo, const std::string &script) = 0;

        /**
         * Convert a Matter event TLV to a Barton resource string value.
         *
         * Script input JSON (available as `sbmdEventArgs` in the script):
         * {
         *     "tlvBase64": <base64-encoded TLV event data>,
         *     "deviceUuid": <device UUID>,
         *     "clusterFeatureMaps": { "<clusterId>": <featureMap>, ... },
         *     "clusterId": <cluster ID>,
         *     "endpointId": <endpoint ID>,
         *     "eventId": <event ID>,
         *     "eventName": <event name from spec>
         * }
         *
         * Script output JSON — one of:
         * { "value": <string|number|boolean> }  // update resource (non-string coerced to string)
         * { }  or  { "value": null }            // suppress — no update, no error
         * { "error": <string> }                 // signal a failure
         *
         * If the script omits the "value" key but returns a plain object (e.g., returns {}),
         * or returns { "value": null }, the event is intentionally suppressed: MapEvent returns
         * a suppress ScriptResult.
         * The caller MUST check result.IsSuppressed() and skip updateResource in that case.
         * This is useful when an event type carries multiple operation sub-types, only some of
         * which represent a resource state change. For example, a LockOperation event may carry
         * a lock, unlock, or door-sense operation; a script can return {} for sub-types it does
         * not need to propagate, avoiding spurious resource updates.
         *
         * If the script returns a non-object (undefined, null, a primitive), that is always
         * treated as a script error — MapEvent returns an error ScriptResult.
         *
         * @param eventInfo Information about the Matter event
         * @param reader TLV reader positioned at the event data
         * @return ScriptResult containing the mapped value, a suppress signal, or an error
         */
        virtual ScriptResult MapEvent(const SbmdEvent &eventInfo, chip::TLV::TLVReader &reader) = 0;

    protected:
        std::string deviceId;
    };
} // namespace barton
