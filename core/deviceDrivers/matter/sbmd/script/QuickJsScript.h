//------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2025 Comcast Cable Communications Management, LLC
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

// Forward declarations for QuickJS types
struct JSRuntime;
struct JSContext;
struct JSValue;

namespace barton
{
    /**
     * QuickJS implementation of SbmdScript for mapping between Barton resources and
     * Matter attributes/commands using JavaScript.
     */
    class QuickJsScript : public SbmdScript
    {
    public:
        explicit QuickJsScript(const std::string &deviceId);
        ~QuickJsScript() override;

        bool AddAttributeReadMapper(const SbmdAttribute &attributeInfo,
                                    const std::string &script) override;

        bool AddAttributeWriteMapper(const SbmdAttribute &attributeInfo,
                                     const std::string &script) override;

        bool AddCommandExecuteMapper(const SbmdCommand &commandInfo,
                                     const std::string &script) override;

        bool MapAttributeRead(const SbmdAttribute &attributeInfo,
                              chip::TLV::TLVReader &reader,
                              std::string &outValue) override;

        bool MapAttributeWrite(const SbmdAttribute &attributeInfo,
                               const std::string &inValue,
                               chip::Platform::ScopedMemoryBuffer<uint8_t> &buffer,
                               size_t &encodedLength) override;

        bool MapCommandExecute(const SbmdCommand &commandInfo,
                               const std::vector<std::string> &argumentValues,
                               chip::TLV::TLVWriter &writer) override;

    private:
        JSRuntime *runtime;
        JSContext *ctx;

        // Stored scripts for each mapper
        std::map<SbmdAttribute, std::string> attributeReadScripts;
        std::map<SbmdAttribute, std::string> attributeWriteScripts;
        std::map<SbmdCommand, std::string> commandExecuteScripts;

        /**
         * Execute a script with a single argument from TLV
         */
        bool ExecuteScript(const std::string &script,
                           const std::string &argumentName,
                           const std::string &argumentValue,
                           std::string &outValue);

        /**
         * Set a JavaScript variable from a string value
         */
        bool SetJsVariable(const std::string &name, const std::string &value);
    };

} // namespace barton
