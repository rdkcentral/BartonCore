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

        bool AddCommandExecuteResponseMapper(const SbmdCommand &commandInfo,
                                             const std::string &script) override;

        /**
         * @see SbmdScript::AddWriteMapper
         */
        bool AddWriteMapper(const std::string &resourceKey, const std::string &script) override;

        /**
         * @see SbmdScript::AddExecuteMapper
         */
        bool AddExecuteMapper(const std::string &resourceKey,
                              const std::string &script,
                              const std::optional<std::string> &responseScript) override;

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
         * @see SbmdScript::MapWrite
         */
        bool MapWrite(const std::string &resourceKey,
                      const std::string &endpointId,
                      const std::string &resourceId,
                      const std::string &inValue,
                      ScriptWriteResult &result) override;

        /**
         * @see SbmdScript::MapExecute
         */
        bool MapExecute(const std::string &resourceKey,
                        const std::string &endpointId,
                        const std::string &resourceId,
                        const std::string &inValue,
                        ScriptWriteResult &result) override;

    private:
        explicit QuickJsScript(const std::string &deviceId);

        // Mutex for protecting script collections (separate from QuickJS context mutex)
        mutable std::mutex scriptsMutex;

        // Stored scripts for each mapper
        std::map<SbmdAttribute, std::string> attributeReadScripts;
        std::map<SbmdCommand, std::string> commandExecuteResponseScripts;
        std::map<std::string, std::string> writeScripts;           // resourceKey -> script
        std::map<std::string, std::string> executeScripts;         // resourceKey -> script
        std::map<std::string, std::string> executeResponseScripts; // resourceKey -> response script

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
         * Extract the "output" field from a script result as a string.
         * Frees the script result JSValue.
         */
        bool ExtractScriptOutputAsString(JSValue &scriptResult, std::string &outValue);

        /**
         * Set a JavaScript variable from a string value.
         */
        bool SetJsVariable(const std::string &name, const std::string &value);
    };

} // namespace barton
