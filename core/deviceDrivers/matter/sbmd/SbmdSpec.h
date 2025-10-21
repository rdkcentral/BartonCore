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
 * Created by Thomas Lea on 10/17/2025
 */

#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace barton
{
    /**
     * Represents a Matter cluster attribute or command parameter
     */
    struct SbmdAttribute
    {
        uint32_t clusterId;
        uint32_t attributeId;
        std::string name;
        std::string type;
    };

    /**
     * Represents a Matter cluster command
     */
    struct SbmdCommand
    {
        uint32_t clusterId;
        uint32_t commandId;
        std::string name;
        std::vector<SbmdAttribute> args;
        std::string featureMask;  // Optional feature mask for conditional execution
    };

    /**
     * Represents a mapper configuration for a resource
     */
    struct SbmdMapper
    {
        // Read mapping
        bool hasRead = false;
        SbmdAttribute readAttribute;
        std::string readScript;

        // Write mapping
        bool hasWrite = false;
        SbmdCommand writeCommand;
        std::string writeScript;

        // Execute mapping
        bool hasExecute = false;
        SbmdCommand executeCommand;
        std::string executeScript;
    };

    /**
     * Represents a device resource (property or function)
     */
    struct SbmdResource
    {
        std::string id;
        std::string type;                    // "boolean", "string", "number", "function", etc.
        std::vector<std::string> modes;      // "read", "write", "dynamic", "emitEvents", etc.
        std::string cachingPolicy;           // "always", "never", etc.
        SbmdMapper mapper;
    };

    /**
     * Represents a device endpoint with its profile and resources
     */
    struct SbmdEndpoint
    {
        std::string id;
        std::string profile;
        uint32_t profileVersion;
        std::vector<SbmdResource> resources;
    };

    /**
     * Barton-specific metadata
     */
    struct SbmdBartonMeta
    {
        std::string deviceClass;
        uint32_t deviceClassVersion;
    };

    /**
     * Matter-specific metadata
     */
    struct SbmdMatterMeta
    {
        uint32_t deviceType;
        uint32_t revision;
    };

    /**
     * Reporting configuration for attribute subscriptions
     */
    struct SbmdReporting
    {
        uint16_t minSecs = 0; // Minimum reporting interval in seconds
        uint16_t maxSecs = 0; // Maximum reporting interval in seconds
    };

    /**
     * Complete SBMD specification for a device driver
     */
    struct SbmdSpec
    {
        std::string schemaVersion;
        std::string driverVersion;
        std::string name;
        SbmdBartonMeta bartonMeta;
        SbmdMatterMeta matterMeta;
        SbmdReporting reporting;
        std::vector<SbmdResource> resources;  // Top-level resources
        std::vector<SbmdEndpoint> endpoints;
    };

} // namespace barton
