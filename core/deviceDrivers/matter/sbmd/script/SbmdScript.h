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
// Created by tlea on 12/4/25
//

#pragma once

#include "lib/core/TLVReader.h"
#include "matter/sbmd/SbmdSpec.h"
#include <platform/CHIPDeviceLayer.h>

#include <string>
#include <vector>

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

        virtual bool AddAttributeReadMapper(const SbmdAttribute &attributeInfo,
                                            const std::string &script) = 0;

        virtual bool AddAttributeWriteMapper(const SbmdAttribute &attributeInfo,
                                             const std::string &script) = 0;

        virtual bool AddCommandExecuteMapper(const SbmdCommand &commandInfo,
                                             const std::string &script) = 0;

        /**
         * Provide details and a value about a Matter attribute and map it to a Barton resource
         * string value.
         */
        virtual bool MapAttributeRead(const SbmdAttribute &attributeInfo,
                                      chip::TLV::TLVReader &reader,
                                      std::string &outValue) = 0;

        /**
         * Provide details and input Barton resource string value and map it to a Matter attribute
         * value in the form of a TLVWriter that the caller could use to send to the device over
         * the air.
         */
        virtual bool MapAttributeWrite(const SbmdAttribute &attributeInfo,
                                       const std::string &inValue,
                                       chip::Platform::ScopedMemoryBuffer<uint8_t> &buffer,
                                       size_t &encodedLength) = 0;

        /**
         * Provide details and input Barton resource string argument values and map them to a
         * Matter command input in the form of a TLVWriter that the caller could use to send
         * to the device over the air.
         */
        virtual bool MapCommandExecute(const SbmdCommand &commandInfo,
                                       const std::vector<std::string> &argumentValues,
                                       chip::TLV::TLVWriter &writer) = 0;

    protected:
        std::string deviceId;
    };
} // namespace barton
