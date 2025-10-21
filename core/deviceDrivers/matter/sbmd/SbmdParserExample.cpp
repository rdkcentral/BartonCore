//------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2024 Comcast Cable Communications Management, LLC
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

/*
 * Example usage of SbmdParser
 * Created by Thomas Lea on 10/17/2025
 */

#include "SbmdParser.h"
#include <iostream>

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <sbmd-yaml-file>" << std::endl;
        return 1;
    }

    // Parse the SBMD file
    auto spec = barton::SbmdParser::ParseFile(argv[1]);

    if (!spec)
    {
        std::cerr << "Failed to parse SBMD file: " << argv[1] << std::endl;
        return 1;
    }

    // Print parsed information
    std::cout << "=== SBMD Spec ===" << std::endl;
    std::cout << "Name: " << spec->name << std::endl;
    std::cout << "Schema Version: " << spec->schemaVersion << std::endl;
    std::cout << "Driver Version: " << spec->driverVersion << std::endl;
    std::cout << std::endl;

    std::cout << "=== Barton Metadata ===" << std::endl;
    std::cout << "Device Class: " << spec->bartonMeta.deviceClass << std::endl;
    std::cout << "Device Class Version: " << spec->bartonMeta.deviceClassVersion << std::endl;
    std::cout << std::endl;

    std::cout << "=== Matter Metadata ===" << std::endl;
    std::cout << "Device Types: ";
    for (size_t i = 0; i < spec->matterMeta.deviceTypes.size(); ++i)
    {
        if (i > 0)
            std::cout << ", ";
        std::cout << "0x" << std::hex << spec->matterMeta.deviceTypes[i] << std::dec;
    }
    std::cout << std::endl;
    std::cout << "Revision: " << spec->matterMeta.revision << std::endl;
    std::cout << std::endl;

    std::cout << "=== Endpoints ===" << std::endl;
    for (const auto &endpoint : spec->endpoints)
    {
        std::cout << "Endpoint " << endpoint.id << ":" << std::endl;
        std::cout << "  Profile: " << endpoint.profile << " (v" << endpoint.profileVersion << ")" << std::endl;
        std::cout << "  Resources: " << endpoint.resources.size() << std::endl;

        for (const auto &resource : endpoint.resources)
        {
            std::cout << "    - " << resource.id << " (" << resource.type << ")" << std::endl;
            std::cout << "      Modes: ";
            for (size_t i = 0; i < resource.modes.size(); ++i)
            {
                if (i > 0) std::cout << ", ";
                std::cout << resource.modes[i];
            }
            std::cout << std::endl;

            if (resource.mapper.hasRead)
            {
                if (resource.mapper.readAttribute.has_value())
                {
                    std::cout << "      Read: cluster=0x" << std::hex << resource.mapper.readAttribute->clusterId
                              << " attr=0x" << resource.mapper.readAttribute->attributeId << std::dec << std::endl;
                }
                else if (resource.mapper.readCommand.has_value())
                {
                    std::cout << "      Read: cluster=0x" << std::hex << resource.mapper.readCommand->clusterId
                              << " cmd=0x" << resource.mapper.readCommand->commandId << std::dec << std::endl;
                }
            }

            if (resource.mapper.hasWrite)
            {
                if (resource.mapper.writeAttribute.has_value())
                {
                    std::cout << "      Write: cluster=0x" << std::hex << resource.mapper.writeAttribute->clusterId
                              << " attr=0x" << resource.mapper.writeAttribute->attributeId << std::dec << std::endl;
                }
                else if (resource.mapper.writeCommand.has_value())
                {
                    std::cout << "      Write: cluster=0x" << std::hex << resource.mapper.writeCommand->clusterId
                              << " cmd=0x" << resource.mapper.writeCommand->commandId << std::dec << std::endl;
                }
            }

            if (resource.mapper.hasExecute)
            {
                if (resource.mapper.executeAttribute.has_value())
                {
                    std::cout << "      Execute: cluster=0x" << std::hex << resource.mapper.executeAttribute->clusterId
                              << " attr=0x" << resource.mapper.executeAttribute->attributeId << std::dec << std::endl;
                }
                else if (resource.mapper.executeCommand.has_value())
                {
                    std::cout << "      Execute: cluster=0x" << std::hex << resource.mapper.executeCommand->clusterId
                              << " cmd=0x" << resource.mapper.executeCommand->commandId << std::dec << std::endl;
                }
            }
        }
        std::cout << std::endl;
    }

    return 0;
}
