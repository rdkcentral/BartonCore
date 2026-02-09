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

/*
 * Created by Thomas Lea on 10/17/2025
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace barton
{
    /**
     * Represents a Matter cluster attribute
     */
    struct SbmdAttribute
    {
        uint32_t clusterId;
        uint32_t attributeId;
        std::string name;
        std::string type;
        std::optional<std::string> resourceEndpointId; // Endpoint ID if parsed from an endpoint resource
        std::string resourceId;                        // Resource ID from the owning SbmdResource
        uint32_t featureMap = 0;                       // Feature map of the cluster, populated at bind time

        // Equality operator for map key usage
        bool operator==(const SbmdAttribute &other) const
        {
            return clusterId == other.clusterId && attributeId == other.attributeId &&
                   resourceEndpointId == other.resourceEndpointId && resourceId == other.resourceId;
        }

        // Less-than operator for std::map usage
        bool operator<(const SbmdAttribute &other) const
        {
            if (clusterId != other.clusterId)
                return clusterId < other.clusterId;
            if (attributeId != other.attributeId)
                return attributeId < other.attributeId;
            if (resourceEndpointId != other.resourceEndpointId)
                return resourceEndpointId < other.resourceEndpointId;
            return resourceId < other.resourceId;
        }
    };

    /**
     * Represents a Matter cluster command parameter
     */
    struct SbmdArgument
    {
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
        std::optional<uint32_t> timedInvokeTimeoutMs; // If set, command requires timed invoke with this timeout
        std::vector<SbmdArgument> args;
        std::optional<std::string> resourceEndpointId; // Endpoint ID if parsed from an endpoint resource
        std::string resourceId;                        // Resource ID from the owning SbmdResource
        uint32_t featureMap = 0;                       // Feature map of the cluster, populated at bind time

        // Equality operator for map key usage
        bool operator==(const SbmdCommand &other) const
        {
            return clusterId == other.clusterId && commandId == other.commandId &&
                   resourceEndpointId == other.resourceEndpointId && resourceId == other.resourceId;
        }

        // Less-than operator for std::map usage
        bool operator<(const SbmdCommand &other) const
        {
            if (clusterId != other.clusterId)
                return clusterId < other.clusterId;
            if (commandId != other.commandId)
                return commandId < other.commandId;
            if (resourceEndpointId != other.resourceEndpointId)
                return resourceEndpointId < other.resourceEndpointId;
            return resourceId < other.resourceId;
        }
    };

    /**
     * Represents a mapper configuration for a resource.
     * Each mapper type (read, write, execute) must have EITHER an attribute OR a command.
     */
    struct SbmdMapper
    {
        // Read mapping - must have either attribute or command
        bool hasRead = false;
        std::optional<SbmdAttribute> readAttribute;
        std::optional<SbmdCommand> readCommand;
        std::string readScript;

        // Write mapping - must have either attribute or command
        bool hasWrite = false;
        std::optional<SbmdAttribute> writeAttribute;
        std::optional<SbmdCommand> writeCommand;
        std::string writeScript;

        // Execute mapping - must have either attribute or command
        bool hasExecute = false;
        std::optional<SbmdAttribute> executeAttribute;
        std::optional<SbmdCommand> executeCommand;
        std::string executeScript;
        std::optional<std::string> executeResponseScript;
    };

    /**
     * Represents a device resource (property or function)
     */
    struct SbmdResource
    {
        std::string id;
        std::string type;                    // "boolean", "string", "number", "function", etc.
        std::vector<std::string> modes;      // "read", "write", "dynamic", "emitEvents", etc.
        std::optional<std::string> resourceEndpointId; // Endpoint ID if parsed from an endpoint resource
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
        std::vector<uint16_t> deviceTypes;
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
        std::string scriptType;
        SbmdBartonMeta bartonMeta;
        SbmdMatterMeta matterMeta;
        SbmdReporting reporting;
        std::vector<SbmdResource> resources;  // Top-level resources
        std::vector<SbmdEndpoint> endpoints;
    };

} // namespace barton

// Hash function for SbmdAttribute to support std::unordered_map
namespace std
{
    template<>
    struct hash<barton::SbmdAttribute>
    {
        std::size_t operator()(const barton::SbmdAttribute &attr) const noexcept
        {
            // Use boost::hash_combine pattern for better hash distribution
            std::size_t h1 = std::hash<uint32_t> {}(attr.clusterId);
            std::size_t h2 = std::hash<uint32_t> {}(attr.attributeId);
            return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
        }
    };

    template<>
    struct hash<barton::SbmdCommand>
    {
        std::size_t operator()(const barton::SbmdCommand &cmd) const noexcept
        {
            // Use boost::hash_combine pattern for better hash distribution
            std::size_t h1 = std::hash<uint32_t> {}(cmd.clusterId);
            std::size_t h2 = std::hash<uint32_t> {}(cmd.commandId);
            return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
        }
    };
} // namespace std
