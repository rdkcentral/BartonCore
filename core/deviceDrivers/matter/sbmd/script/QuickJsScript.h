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
// Created by tlea on 12/5/25
//

#pragma once

#include "SbmdScript.h"
#include <map>
#include <memory>
#include <mutex>

// Forward declarations for QuickJS types
struct JSRuntime;
struct JSContext;
struct JSValue;

// Forward declaration for JsonCpp
namespace Json
{
    class Value;
}

namespace barton
{
    /**
     * QuickJS implementation of SbmdScript for mapping between Barton resources and
     * Matter attributes/commands using JavaScript.
     *
     * This class is thread-safe. All public methods are protected by an internal mutex.
     */
    class QuickJsScript : public SbmdScript
    {
    public:
        /**
         * Factory method to create a QuickJsScript instance.
         * @param deviceId The device identifier for this script context
         * @return A unique_ptr to a QuickJsScript, or nullptr if initialization failed
         */
        static std::unique_ptr<QuickJsScript> Create(const std::string &deviceId);

        ~QuickJsScript() override;

        bool AddAttributeReadMapper(const SbmdAttribute &attributeInfo,
                                    const std::string &script) override;

        bool AddAttributeWriteMapper(const SbmdAttribute &attributeInfo,
                                     const std::string &script) override;

        bool AddCommandExecuteMapper(const SbmdCommand &commandInfo,
                                     const std::string &script) override;

        bool AddCommandExecuteResponseMapper(const SbmdCommand &commandInfo,
                                             const std::string &script) override;

        /**
         * Add a write mapper for write operations that use commands.
         *
         * @see SbmdScript::AddCommandsWriteMapper
         */
        bool AddCommandsWriteMapper(const std::vector<SbmdCommand> &commands, const std::string &script) override;

        /**
         * Convert a Matter attribute value to a Barton resource string value.
         *
         * The script will be executed with a JSON object named "sbmdReadArgs" containing the JSON
         * representation of the attribute value in the following format:
         *
         * sbmdReadArgs = {
         *     "input" : <attribute value (unwrapped from TlvToJson's {"value": ...} wrapper)>,
         *     "deviceUuid" : <device UUID>,
         *     "clusterId" : <cluster ID>,
         *     "featureMap" : <cluster feature map>,
         *     "endpointId" : <endpoint ID>,
         *     "attributeId" : <attribute ID>,
         *     "attributeName" : <attribute name>,
         *     "attributeType" : <attribute type>
         * }
         *
         * The script should return a JSON object of the following format:
         *
         * {"output" : <Barton string representation of the attribute value>}
         *
         * @see SbmdScript::MapAttributeRead
         */
        bool MapAttributeRead(const SbmdAttribute &attributeInfo,
                              chip::TLV::TLVReader &reader,
                              std::string &outValue) override;

        /**
         * Convert a Barton resource string value to a Matter attribute value which can be sent
         * to the device over the air.
         *
         * The script will be executed with a JSON object named "sbmdWriteArgs" containing the JSON
         * representation of the Barton string value in the following format:
         *
         * sbmdWriteArgs = {
         *     "input" : <Barton string representation of the attribute value>,
         *     "deviceUuid" : <device UUID>,
         *     "clusterId" : <cluster ID>,
         *     "featureMap" : <cluster feature map>,
         *     "endpointId" : <endpoint ID>,
         *     "attributeId" : <attribute ID>,
         *     "attributeName" : <attribute name>,
         *     "attributeType" : <attribute type>
         * }
         *
         * The script should return a JSON object of the following format:
         *
         * {"output" : <JSON representation of the attribute TLV>}
         *
         * @see SbmdScript::MapAttributeWrite
         */
        bool MapAttributeWrite(const SbmdAttribute &attributeInfo,
                               const std::string &inValue,
                               chip::Platform::ScopedMemoryBuffer<uint8_t> &buffer,
                               size_t &encodedLength) override;

        /**
         * Convert a Barton command argument values to a Matter command which can be sent
         * to the device over the air. If a Matter command does not take any arguments,
         * it is not necessary to invoke this mapping method.
         *
         * The script will be executed with a JSON object named "sbmdCommandArgs" containing the JSON
         * representation of the Barton string values in the following format:
         *
         * sbmdCommandArgs = {
         *     "input" : <array of Barton string representations of the command arguments>,
         *     "deviceUuid" : <device UUID>,
         *     "clusterId" : <cluster ID>,
         *     "featureMap" : <cluster feature map>,
         *     "endpointId" : <endpoint ID>,
         *     "commandId" : <command ID>,
         *     "commandName" : <command name>
         * }
         *
         * The script should return a JSON object of the following format:
         *
         * {"output" : <JSON representation of the command args TLV>}
         *
         * @see SbmdScript::MapCommandExecute
         */
        bool MapCommandExecute(const SbmdCommand &commandInfo,
                               const std::vector<std::string> &argumentValues,
                               chip::Platform::ScopedMemoryBuffer<uint8_t> &buffer,
                               size_t &encodedLength) override;

        /**
         * Convert a Matter command response to a Barton resource string value.
         *
         * The script will be executed with a JSON object named "sbmdCommandResponseArgs" containing the JSON
         * representation of the command response in the following format:
         *
         * sbmdCommandResponseArgs = {
         *     "input" : <unwrapped command response value (the \"value\" field of the command response TLV JSON)>,
         *     "deviceUuid" : <device UUID>,
         *     "clusterId" : <cluster ID>,
         *     "featureMap" : <cluster feature map>,
         *     "endpointId" : <endpoint ID>,
         *     "commandId" : <command ID>,
         *     "commandName" : <command name>
         * }
         *
         * The script should return a JSON object of the following format:
         *
         * {"output" : <Barton string representation of the command response>}
         *
         * @see SbmdScript::MapCommandExecuteResponse
         */
        bool MapCommandExecuteResponse(const SbmdCommand &commandInfo,
                                       chip::TLV::TLVReader &reader,
                                       std::string &outValue) override;

        /**
         * Convert a Barton write value to a Matter command selection and arguments.
         *
         * The script will be executed with a JSON object named "sbmdWriteArgs" containing:
         *
         * sbmdWriteArgs = {
         *     "input" : <Barton string representation of the value to write>,
         *     "deviceUuid" : <device UUID>,
         *     "commands" : [<array of available command names>]
         * }
         *
         * The script should return a JSON object with the following fields:
         *
         * {"output" : <JSON representation of the command args TLV (or null if no args)>,
         *  "command" : <name of the command to execute (required when multiple commands are available;
         *               optional/ignored when only a single command is available)>}
         *
         * Note: When only one command is available, the "command" field is optional and
         * the single command will be auto-selected. The "command" field is required when
         * multiple commands are available.
         *
         * @see SbmdScript::MapWriteCommand
         */
        bool MapWriteCommand(const std::vector<SbmdCommand> &availableCommands,
                             const std::string &inValue,
                             std::string &selectedCommandName,
                             chip::Platform::ScopedMemoryBuffer<uint8_t> &buffer,
                             size_t &encodedLength) override;

        /**
         * Add an event mapper.
         *
         * @see SbmdScript::AddEventMapper
         */
        bool AddEventMapper(const SbmdEvent &eventInfo, const std::string &script) override;

        /**
         * Convert a Matter event value to a Barton resource string value.
         *
         * The script will be executed with a JSON object named "sbmdEventArgs" containing the JSON
         * representation of the event data in the following format:
         *
         * sbmdEventArgs = {
         *     "input" : <event data (unwrapped from TlvToJson's {"value": ...} wrapper)>,
         *     "deviceUuid" : <device UUID>,
         *     "clusterId" : <cluster ID>,
         *     "featureMap" : <cluster feature map>,
         *     "endpointId" : <endpoint ID>,
         *     "eventId" : <event ID>,
         *     "eventName" : <event name>,
         *     "eventType" : <event type>
         * }
         *
         * The script should return a JSON object of the following format:
         *
         * {"output" : <Barton string representation of the event data>}
         *
         * @see SbmdScript::MapEvent
         */
        bool MapEvent(const SbmdEvent &eventInfo,
                      chip::TLV::TLVReader &reader,
                      std::string &outValue) override;

    private:
        explicit QuickJsScript(const std::string &deviceId, JSRuntime *runtime, JSContext *ctx);

        mutable std::mutex qjsMtx;
        JSRuntime *runtime;
        JSContext *ctx;

        // Stored scripts for each mapper
        std::map<SbmdAttribute, std::string> attributeReadScripts;
        std::map<SbmdAttribute, std::string> attributeWriteScripts;
        std::map<SbmdCommand, std::string> commandExecuteScripts;
        std::map<SbmdCommand, std::string> commandExecuteResponseScripts;
        std::map<std::string, std::string> writeCommandsScripts; // commands key -> script
        std::map<SbmdEvent, std::string> eventScripts;

        /**
         * Generate a deterministic key from a commands vector for script lookup.
         */
        static std::string GenerateCommandsKey(const std::vector<SbmdCommand> &commands);

        /**
         * Execute a script.
         */
        bool ExecuteScript(const std::string &script,
                           const std::string &argumentName,
                           const JSValue &argumentJson,
                           JSValue &outJson);

        /**
         * Parse a JSON string into a QuickJS JSValue.
         */
        bool ParseJsonToJSValue(const std::string &jsonString, const std::string &sourceName, JSValue &outValue);

        /**
         * Extract the "output" field from a script result and parse it as JSON.
         * Frees the script result JSValue.
         */
        bool ExtractScriptOutputAsJson(JSValue &scriptResult, Json::Value &outJson);

        /**
         * Extract the "output" field from a script result as a string.
         * Frees the script result JSValue.
         */
        bool ExtractScriptOutputAsString(JSValue &scriptResult, std::string &outValue);

        /**
         * Encode a TLV-formatted JSON string to a buffer.
         */
        bool EncodeJsonToTlv(const std::string &tlvFormattedJson,
                             chip::Platform::ScopedMemoryBuffer<uint8_t> &buffer,
                             size_t &encodedLength);

        /**
         * Set a JavaScript variable from a string value
         */
        bool SetJsVariable(const std::string &name, const std::string &value);
    };

} // namespace barton
