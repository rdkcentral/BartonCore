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

#define LOG_TAG "SbmdParser"
#define logFmt(fmt) "(%s): " fmt, __func__

#include "SbmdParser.h"
#include <yaml-cpp/yaml.h>

extern "C" {
#include <icLog/logging.h>
}

namespace barton
{

std::unique_ptr<SbmdSpec> SbmdParser::ParseFile(const std::string &filePath)
{
    try
    {
        icLogDebug(LOG_TAG, "Parsing SBMD file: %s", filePath.c_str());

        YAML::Node root = YAML::LoadFile(filePath);
        auto spec = std::make_unique<SbmdSpec>();

        // Parse top-level fields
        if (root["schemaVersion"])
        {
            spec->schemaVersion = root["schemaVersion"].as<std::string>();
        }

        if (root["driverVersion"])
        {
            spec->driverVersion = root["driverVersion"].as<std::string>();
        }

        if (root["name"])
        {
            spec->name = root["name"].as<std::string>();
        }

        // Parse bartonMeta
        if (root["bartonMeta"])
        {
            if (!ParseBartonMeta(root["bartonMeta"], spec->bartonMeta))
            {
                icLogError(LOG_TAG, "Failed to parse bartonMeta section");
                return nullptr;
            }
        }

        // Parse matterMeta
        if (root["matterMeta"])
        {
            if (!ParseMatterMeta(root["matterMeta"], spec->matterMeta))
            {
                icLogError(LOG_TAG, "Failed to parse matterMeta section");
                return nullptr;
            }
        }

        // Parse reporting
        if (root["reporting"])
        {
            if (!ParseReporting(root["reporting"], spec->reporting))
            {
                icLogError(LOG_TAG, "Failed to parse reporting section");
                return nullptr;
            }
        }

        // Parse top-level resources
        if (root["resources"] && root["resources"].IsSequence())
        {
            for (const auto &resourceNode : root["resources"])
            {
                SbmdResource resource;
                if (ParseResource(resourceNode, resource))
                {
                    spec->resources.push_back(resource);
                }
            }
        }

        // Parse endpoints
        if (root["endpoints"] && root["endpoints"].IsSequence())
        {
            for (const auto &endpointNode : root["endpoints"])
            {
                SbmdEndpoint endpoint;
                if (ParseEndpoint(endpointNode, endpoint))
                {
                    spec->endpoints.push_back(endpoint);
                }
                else
                {
                    icLogWarn(LOG_TAG, "Failed to parse endpoint, skipping");
                }
            }
        }

        icLogInfo(LOG_TAG, "Successfully parsed SBMD spec: %s (v%s)",
                  spec->name.c_str(), spec->driverVersion.c_str());
        return spec;
    }
    catch (const YAML::Exception &e)
    {
        icLogError(LOG_TAG, "YAML parsing error: %s", e.what());
        return nullptr;
    }
    catch (const std::exception &e)
    {
        icLogError(LOG_TAG, "Error parsing SBMD file: %s", e.what());
        return nullptr;
    }
}

std::unique_ptr<SbmdSpec> SbmdParser::ParseString(const std::string &yamlContent)
{
    try
    {
        icLogDebug(LOG_TAG, "Parsing SBMD from string");

        YAML::Node root = YAML::Load(yamlContent);
        auto spec = std::make_unique<SbmdSpec>();

        // Parse top-level fields
        if (root["schemaVersion"])
        {
            spec->schemaVersion = root["schemaVersion"].as<std::string>();
        }

        if (root["driverVersion"])
        {
            spec->driverVersion = root["driverVersion"].as<std::string>();
        }

        if (root["name"])
        {
            spec->name = root["name"].as<std::string>();
        }

        // Parse bartonMeta
        if (root["bartonMeta"])
        {
            if (!ParseBartonMeta(root["bartonMeta"], spec->bartonMeta))
            {
                icLogError(LOG_TAG, "Failed to parse bartonMeta section");
                return nullptr;
            }
        }

        // Parse matterMeta
        if (root["matterMeta"])
        {
            if (!ParseMatterMeta(root["matterMeta"], spec->matterMeta))
            {
                icLogError(LOG_TAG, "Failed to parse matterMeta section");
                return nullptr;
            }
        }

        // Parse reporting
        if (root["reporting"])
        {
            if (!ParseReporting(root["reporting"], spec->reporting))
            {
                icLogError(LOG_TAG, "Failed to parse reporting section");
                return nullptr;
            }
        }

        // Parse top-level resources
        if (root["resources"] && root["resources"].IsSequence())
        {
            for (const auto &resourceNode : root["resources"])
            {
                SbmdResource resource;
                if (ParseResource(resourceNode, resource))
                {
                    spec->resources.push_back(resource);
                }
            }
        }

        // Parse endpoints
        if (root["endpoints"] && root["endpoints"].IsSequence())
        {
            for (const auto &endpointNode : root["endpoints"])
            {
                SbmdEndpoint endpoint;
                if (ParseEndpoint(endpointNode, endpoint))
                {
                    spec->endpoints.push_back(endpoint);
                }
                else
                {
                    icLogWarn(LOG_TAG, "Failed to parse endpoint, skipping");
                }
            }
        }

        icLogInfo(LOG_TAG, "Successfully parsed SBMD spec from string: %s", spec->name.c_str());
        return spec;
    }
    catch (const YAML::Exception &e)
    {
        icLogError(LOG_TAG, "YAML parsing error: %s", e.what());
        return nullptr;
    }
    catch (const std::exception &e)
    {
        icLogError(LOG_TAG, "Error parsing SBMD string: %s", e.what());
        return nullptr;
    }
}

bool SbmdParser::ParseBartonMeta(const YAML::Node &node, SbmdBartonMeta &meta)
{
    if (!node.IsMap())
    {
        icLogError(LOG_TAG, "bartonMeta is not a map");
        return false;
    }

    if (node["deviceClass"])
    {
        meta.deviceClass = node["deviceClass"].as<std::string>();
    }

    if (node["deviceClassVersion"])
    {
        meta.deviceClassVersion = node["deviceClassVersion"].as<uint32_t>();
    }

    return true;
}

bool SbmdParser::ParseMatterMeta(const YAML::Node &node, SbmdMatterMeta &meta)
{
    if (!node.IsMap())
    {
        icLogError(LOG_TAG, "matterMeta is not a map");
        return false;
    }

    if (node["deviceType"])
    {
        std::string deviceType = node["deviceType"].as<std::string>();
        meta.deviceType = ParseHexOrDecimal(deviceType);
    }

    if (node["revision"])
    {
        meta.revision = node["revision"].as<uint32_t>();
    }

    return true;
}

bool SbmdParser::ParseReporting(const YAML::Node &node, SbmdReporting &reporting)
{
    if (!node.IsMap())
    {
        icLogError(LOG_TAG, "reporting is not a map");
        return false;
    }

    if (node["minSecs"])
    {
        reporting.minSecs = node["minSecs"].as<uint16_t>();
    }

    if (node["maxSecs"])
    {
        reporting.maxSecs = node["maxSecs"].as<uint16_t>();
    }

    return true;
}

bool SbmdParser::ParseResource(const YAML::Node &node, SbmdResource &resource)
{
    if (!node.IsMap())
    {
        icLogError(LOG_TAG, "resource is not a map");
        return false;
    }

    if (node["id"])
    {
        resource.id = node["id"].as<std::string>();
    }

    if (node["type"])
    {
        resource.type = node["type"].as<std::string>();
    }

    if (node["modes"])
    {
        resource.modes = ParseStringArray(node["modes"]);
    }

    if (node["cachingPolicy"])
    {
        resource.cachingPolicy = node["cachingPolicy"].as<std::string>();
    }

    if (node["mapper"])
    {
        if (!ParseMapper(node["mapper"], resource.mapper))
        {
            icLogWarn(LOG_TAG, "Failed to parse mapper for resource %s", resource.id.c_str());
        }
    }

    return true;
}

bool SbmdParser::ParseEndpoint(const YAML::Node &node, SbmdEndpoint &endpoint)
{
    if (!node.IsMap())
    {
        icLogError(LOG_TAG, "endpoint is not a map");
        return false;
    }

    if (node["id"])
    {
        endpoint.id = node["id"].as<std::string>();
    }

    if (node["profile"])
    {
        endpoint.profile = node["profile"].as<std::string>();
    }

    if (node["profileVersion"])
    {
        endpoint.profileVersion = node["profileVersion"].as<uint32_t>();
    }

    if (node["resources"] && node["resources"].IsSequence())
    {
        for (const auto &resourceNode : node["resources"])
        {
            SbmdResource resource;
            if (ParseResource(resourceNode, resource))
            {
                endpoint.resources.push_back(resource);
            }
        }
    }

    return true;
}

bool SbmdParser::ParseMapper(const YAML::Node &node, SbmdMapper &mapper)
{
    if (!node.IsMap())
    {
        icLogError(LOG_TAG, "mapper is not a map");
        return false;
    }

    // Parse read mapping
    if (node["read"])
    {
        const YAML::Node &readNode = node["read"];
        mapper.hasRead = true;

        if (readNode["attribute"])
        {
            ParseAttribute(readNode["attribute"], mapper.readAttribute);
        }

        if (readNode["script"])
        {
            mapper.readScript = readNode["script"].as<std::string>();
        }
    }

    // Parse write mapping
    if (node["write"])
    {
        const YAML::Node &writeNode = node["write"];
        mapper.hasWrite = true;

        if (writeNode["command"])
        {
            ParseCommand(writeNode["command"], mapper.writeCommand);
        }

        if (writeNode["script"])
        {
            mapper.writeScript = writeNode["script"].as<std::string>();
        }
    }

    // Parse execute mapping
    if (node["execute"])
    {
        const YAML::Node &executeNode = node["execute"];
        mapper.hasExecute = true;

        if (executeNode["command"])
        {
            ParseCommand(executeNode["command"], mapper.executeCommand);
        }

        if (executeNode["script"])
        {
            mapper.executeScript = executeNode["script"].as<std::string>();
        }
    }

    return true;
}

bool SbmdParser::ParseAttribute(const YAML::Node &node, SbmdAttribute &attribute)
{
    if (!node.IsMap())
    {
        icLogError(LOG_TAG, "attribute is not a map");
        return false;
    }

    if (node["clusterId"])
    {
        std::string clusterId = node["clusterId"].as<std::string>();
        attribute.clusterId = ParseHexOrDecimal(clusterId);
    }

    if (node["attributeId"])
    {
        std::string attributeId = node["attributeId"].as<std::string>();
        attribute.attributeId = ParseHexOrDecimal(attributeId);
    }

    if (node["name"])
    {
        attribute.name = node["name"].as<std::string>();
    }

    if (node["type"])
    {
        attribute.type = node["type"].as<std::string>();
    }

    return true;
}

bool SbmdParser::ParseCommand(const YAML::Node &node, SbmdCommand &command)
{
    if (!node.IsMap())
    {
        icLogError(LOG_TAG, "command is not a map");
        return false;
    }

    if (node["clusterId"])
    {
        std::string clusterId = node["clusterId"].as<std::string>();
        command.clusterId = ParseHexOrDecimal(clusterId);
    }

    if (node["commandId"])
    {
        std::string commandId = node["commandId"].as<std::string>();
        command.commandId = ParseHexOrDecimal(commandId);
    }

    if (node["name"])
    {
        command.name = node["name"].as<std::string>();
    }

    if (node["featureMask"])
    {
        command.featureMask = node["featureMask"].as<std::string>();
    }

    // Parse command arguments
    if (node["args"] && node["args"].IsSequence())
    {
        for (const auto &argNode : node["args"])
        {
            SbmdAttribute arg;
            if (argNode["name"])
            {
                arg.name = argNode["name"].as<std::string>();
            }
            if (argNode["type"])
            {
                arg.type = argNode["type"].as<std::string>();
            }
            if (argNode["featureMask"])
            {
                // Store feature mask in the type field for args
                // This is a simplification; you might want a separate field
                arg.type += " (" + argNode["featureMask"].as<std::string>() + ")";
            }
            command.args.push_back(arg);
        }
    }

    return true;
}

uint32_t SbmdParser::ParseHexOrDecimal(const std::string &value)
{
    if (value.empty())
    {
        return 0;
    }

    // Check if it's a hex string (starts with "0x" or "0X")
    if (value.size() > 2 && value[0] == '0' && (value[1] == 'x' || value[1] == 'X'))
    {
        return static_cast<uint32_t>(std::stoul(value, nullptr, 16));
    }
    else
    {
        return static_cast<uint32_t>(std::stoul(value));
    }
}

std::vector<std::string> SbmdParser::ParseStringArray(const YAML::Node &node)
{
    std::vector<std::string> result;

    if (!node.IsSequence())
    {
        return result;
    }

    for (const auto &item : node)
    {
        result.push_back(item.as<std::string>());
    }

    return result;
}

} // namespace barton
