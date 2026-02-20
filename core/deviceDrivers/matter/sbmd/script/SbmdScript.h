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

#include "../SbmdSpec.h"
#include "lib/core/TLVReader.h"
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

        virtual bool AddCommandExecuteResponseMapper(const SbmdCommand &commandInfo,
                                                     const std::string &script) = 0;

        /**
         * Add a write mapper for write operations that use commands.
         * The script will be used when a write operation needs to select which command to execute.
         *
         * @param commands The commands available for this write mapper
         * @param script The JavaScript script for the mapper
         * @return true if the mapper was added successfully, false otherwise
         */
        virtual bool AddCommandsWriteMapper(const std::vector<SbmdCommand> &commands, const std::string &script) = 0;

        /**
         * Convert a Matter attribute value to a Barton resource string value.
         *
         * @param attributeInfo Information about the Matter attribute
         * @param reader TLV reader positioned at the attribute value after having read the attribute data
         *        from the device or cache.
         * @param outValue will contain a Barton string representation of the attribute value as converted
         *        by the script.
         * @return true if mapping was successful, false otherwise
         */
        virtual bool MapAttributeRead(const SbmdAttribute &attributeInfo,
                                      chip::TLV::TLVReader &reader,
                                      std::string &outValue) = 0;

        /**
         * Convert a Barton resource string value to a Matter attribute value which can be sent
         * to the device over the air.
         *
         * @param attributeInfo Information about the Matter attribute
         * @param inValue Barton string representation of the attribute value to convert
         * @param buffer TLV buffer to write the converted attribute value to
         * @param encodedLength Length of the encoded TLV data
         * @return true if mapping was successful, false otherwise
         */
        virtual bool MapAttributeWrite(const SbmdAttribute &attributeInfo,
                                       const std::string &inValue,
                                       chip::Platform::ScopedMemoryBuffer<uint8_t> &buffer,
                                       size_t &encodedLength) = 0;

        /**
         * Convert a list of Barton resource string argument values to a Matter command input
         * which can be sent to the device over the air. If a Matter command does not take
         * any arguments, it is not necessary to invoke this mapping method.
         *
         * @param commandInfo Information about the Matter command
         * @param argumentValues Barton string representations of the command arguments in order
         *        of the Barton data model definition for the command (might differ from Matter).
         * @param buffer TLV buffer to write the converted command input to
         * @param encodedLength Length of the encoded TLV data
         * @return true if mapping was successful, false otherwise
         */
        virtual bool MapCommandExecute(const SbmdCommand &commandInfo,
                                       const std::vector<std::string> &argumentValues,
                                       chip::Platform::ScopedMemoryBuffer<uint8_t> &buffer,
                                       size_t &encodedLength) = 0;

        /**
         * Convert a Matter command response TLV to a Barton resource string value.
         * This is optional - only needed when a command returns data that should be
         * converted to a Barton string response.
         *
         * @param commandInfo Information about the Matter command
         * @param reader TLV reader positioned at the command response data
         * @param outValue will contain a Barton string representation of the response as converted
         *        by the script.
         * @return true if mapping was successful, false otherwise
         */
        virtual bool MapCommandExecuteResponse(const SbmdCommand &commandInfo,
                                               chip::TLV::TLVReader &reader,
                                               std::string &outValue) = 0;

        /**
         * Convert a Barton write value to a Matter command selection and arguments.
         * Used for both single- and multi-command write mappers. When only one command
         * is available, it is auto-selected and the script does not need to specify a
         * command name. When multiple commands are available, the script must select
         * which command to execute by returning a "command" field in the output.
         *
         * @param availableCommands The list of commands the script can select from
         * @param inValue Barton string representation of the value to write
         * @param[out] selectedCommandName The name of the command selected by the script
         *             (or auto-selected when only one command is available)
         * @param[out] buffer TLV buffer to write the converted command arguments to
         * @param[out] encodedLength Length of the encoded TLV data
         * @return true if mapping was successful, false otherwise
         */
        virtual bool MapWriteCommand(const std::vector<SbmdCommand> &availableCommands,
                                     const std::string &inValue,
                                     std::string &selectedCommandName,
                                     chip::Platform::ScopedMemoryBuffer<uint8_t> &buffer,
                                     size_t &encodedLength) = 0;

        /**
         * Add an event mapper for reading event data.
         * The script will be used when an event is received from the device.
         *
         * @param eventInfo Information about the Matter event
         * @param script The JavaScript script for the mapper
         * @return true if the mapper was added successfully, false otherwise
         */
        virtual bool AddEventReadMapper(const SbmdEvent &eventInfo, const std::string &script) = 0;

        /**
         * Convert a Matter event value to a Barton resource string value.
         *
         * @param eventInfo Information about the Matter event
         * @param reader TLV reader positioned at the event data after having read it from the device
         * @param outValue will contain a Barton string representation of the event data as converted
         *        by the script.
         * @return true if mapping was successful, false otherwise
         */
        virtual bool MapEventRead(const SbmdEvent &eventInfo,
                                  chip::TLV::TLVReader &reader,
                                  std::string &outValue) = 0;

    protected:
        std::string deviceId;
    };
} // namespace barton
