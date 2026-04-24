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

#define LOG_TAG "SbmdParser"
#define logFmt(fmt) "(%s): " fmt, __func__

#include "SbmdParser.h"
#include <yaml-cpp/yaml.h>

extern "C" {
#include <icLog/logging.h>
}

namespace barton
{

    namespace
    {
        const SbmdAlias *FindAlias(const std::vector<SbmdAlias> &aliases, const std::string &name)
        {
            for (const auto &alias : aliases)
            {
                if (alias.name == name)
                {
                    return &alias;
                }
            }

            return nullptr;
        }

        bool ValidateMapper(const SbmdMapper &mapper, const std::string &resourceId)
        {
            // Validate read mapper
            if (mapper.hasRead)
            {
                // Script must be non-empty
                if (mapper.readScript.empty())
                {
                    icError("Resource %s has read enabled but readScript is empty", resourceId.c_str());
                    return false;
                }

                // Must use attribute (commands not supported for read)
                if (!mapper.readAttribute.has_value())
                {
                    icError("Resource %s has read enabled but no readAttribute specified", resourceId.c_str());
                    return false;
                }

                if (mapper.readCommand.has_value())
                {
                    icError("Resource %s uses readCommand which is not yet supported", resourceId.c_str());
                    return false;
                }
            }

            // Validate write mapper
            if (mapper.hasWrite)
            {
                // Script must be non-empty - write mappers are script-only
                if (mapper.writeScript.empty())
                {
                    icError("Resource %s has write enabled but writeScript is empty", resourceId.c_str());
                    return false;
                }
            }

            // Validate execute mapper
            if (mapper.hasExecute)
            {
                // Script must be non-empty - execute mappers are script-only
                if (mapper.executeScript.empty())
                {
                    icError("Resource %s has execute enabled but executeScript is empty", resourceId.c_str());
                    return false;
                }
            }

            // Validate event mapper
            if (mapper.event.has_value())
            {
                if (mapper.eventScript.empty())
                {
                    icError("Resource %s has event mapper but eventScript is empty", resourceId.c_str());
                    return false;
                }
            }

            return true;
        }

        /**
         * Helper to set resource and endpoint IDs on all mapper attributes and commands.
         */
        void SetMapperIds(SbmdResource &resource, const std::optional<std::string> &endpointId = std::nullopt)
        {
            resource.resourceEndpointId = endpointId;

            auto setAttrIds = [&](std::optional<SbmdAttribute> &attr) {
                if (attr.has_value())
                {
                    attr.value().resourceEndpointId = endpointId;
                    attr.value().resourceId = resource.id;
                }
            };

            auto setCmdIds = [&](std::optional<SbmdCommand> &cmd) {
                if (cmd.has_value())
                {
                    cmd.value().resourceEndpointId = endpointId;
                    cmd.value().resourceId = resource.id;
                }
            };

            auto setCmdsIds = [&](std::vector<SbmdCommand> &cmds) {
                for (auto &cmd : cmds)
                {
                    cmd.resourceEndpointId = endpointId;
                    cmd.resourceId = resource.id;
                }
            };

            if (resource.mapper.hasRead)
            {
                setAttrIds(resource.mapper.readAttribute);
                setCmdIds(resource.mapper.readCommand);
            }

            if (resource.mapper.event.has_value())
            {
                resource.mapper.event.value().resourceEndpointId = endpointId;
                resource.mapper.event.value().resourceId = resource.id;
            }
            // Note: Write and execute mappers are script-only, no metadata to set IDs on
        }
} // anonymous namespace

std::shared_ptr<SbmdSpec> SbmdParser::ParseYamlNode(const YAML::Node &root)
{
    auto spec = std::make_shared<SbmdSpec>();

    // Parse top-level fields
    if (!root["schemaVersion"])
    {
        icError("SBMD spec is missing required 'schemaVersion' field");

        return nullptr;
    }

    spec->schemaVersion = root["schemaVersion"].as<std::string>();

    if (spec->schemaVersion != "2.0")
    {
        icError("Unsupported SBMD schemaVersion '%s'; expected '2.0'", spec->schemaVersion.c_str());

        return nullptr;
    }

    if (root["driverVersion"])
    {
        spec->driverVersion = root["driverVersion"].as<std::string>();
    }

    if (root["name"])
    {
        spec->name = root["name"].as<std::string>();
    }

    if (root["scriptType"])
    {
        spec->scriptType = root["scriptType"].as<std::string>();
    }

    // Parse bartonMeta
    if (root["bartonMeta"])
    {
        if (!ParseBartonMeta(root["bartonMeta"], spec->bartonMeta))
        {
            icError("Failed to parse bartonMeta section");
            return nullptr;
        }
    }

    // Parse matterMeta
    if (root["matterMeta"])
    {
        if (!ParseMatterMeta(root["matterMeta"], spec->matterMeta))
        {
            icError("Failed to parse matterMeta section");
            return nullptr;
        }
    }

    // Parse reporting
    if (root["reporting"])
    {
        if (!ParseReporting(root["reporting"], spec->reporting))
        {
            icError("Failed to parse reporting section");
            return nullptr;
        }
    }

    // Parse top-level resources
    if (root["resources"] && root["resources"].IsSequence())
    {
        for (const auto &resourceNode : root["resources"])
        {
            SbmdResource resource;
            if (ParseResource(resourceNode, resource, spec->matterMeta.aliases))
            {
                SetMapperIds(resource);
                spec->resources.push_back(resource);
            }
            else
            {
                icError("Failed to parse top-level resource, aborting spec load");
                return nullptr;
            }
        }
    }

    // Parse endpoints
    if (root["endpoints"] && root["endpoints"].IsSequence())
    {
        for (const auto &endpointNode : root["endpoints"])
        {
            SbmdEndpoint endpoint;
            if (ParseEndpoint(endpointNode, endpoint, spec->matterMeta.aliases))
            {
                spec->endpoints.push_back(endpoint);
            }
            else
            {
                icError("Failed to parse endpoint, aborting spec load");
                return nullptr;
            }
        }
    }

    return spec;
}

std::shared_ptr<SbmdSpec> SbmdParser::ParseFile(const std::string &filePath)
{
    try
    {
        icDebug("Parsing SBMD file: %s", filePath.c_str());

        YAML::Node root = YAML::LoadFile(filePath);
        auto spec = ParseYamlNode(root);

        if (spec)
        {
            icInfo("Successfully parsed SBMD spec: %s (v%s)", spec->name.c_str(), spec->driverVersion.c_str());
        }
        return spec;
    }
    catch (const YAML::Exception &e)
    {
        icError("YAML parsing error: %s", e.what());
        return nullptr;
    }
    catch (const std::exception &e)
    {
        icError("Error parsing SBMD file: %s", e.what());
        return nullptr;
    }
}

std::shared_ptr<SbmdSpec> SbmdParser::ParseString(const std::string &yamlContent)
{
    try
    {
        icDebug("Parsing SBMD from string");

        YAML::Node root = YAML::Load(yamlContent);
        auto spec = ParseYamlNode(root);

        if (spec)
        {
            icInfo("Successfully parsed SBMD spec from string: %s", spec->name.c_str());
        }
        return spec;
    }
    catch (const YAML::Exception &e)
    {
        icError("YAML parsing error: %s", e.what());
        return nullptr;
    }
    catch (const std::exception &e)
    {
        icError("Error parsing SBMD string: %s", e.what());
        return nullptr;
    }
}

bool SbmdParser::ParseBartonMeta(const YAML::Node &node, SbmdBartonMeta &meta)
{
    if (!node.IsMap())
    {
        icError("bartonMeta is not a map");
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
        icError("matterMeta is not a map");
        return false;
    }

    if (node["deviceTypes"] && node["deviceTypes"].IsSequence())
    {
        for (const auto &deviceTypeNode : node["deviceTypes"])
        {
            std::string deviceTypeStr = deviceTypeNode.as<std::string>();
            uint16_t deviceType = static_cast<uint16_t>(ParseHexOrDecimal(deviceTypeStr));
            meta.deviceTypes.push_back(deviceType);
        }
    }

    if (node["revision"])
    {
        meta.revision = node["revision"].as<uint32_t>();
    }

    // Parse optional featureClusters
    if (node["featureClusters"] && node["featureClusters"].IsSequence())
    {
        for (const auto &clusterNode : node["featureClusters"])
        {
            std::string clusterStr = clusterNode.as<std::string>();
            uint32_t clusterId = ParseHexOrDecimal(clusterStr);
            meta.featureClusters.push_back(clusterId);
        }
    }

    // Parse optional aliases
    if (node["aliases"])
    {
        if (!node["aliases"].IsSequence())
        {
            icError("matterMeta.aliases must be a sequence");
            return false;
        }

        for (const auto &aliasNode : node["aliases"])
        {
            SbmdAlias alias;

            if (!ParseAlias(aliasNode, alias))
            {
                icError("Failed to parse alias in matterMeta");
                return false;
            }

            if (FindAlias(meta.aliases, alias.name) != nullptr)
            {
                icError("Duplicate alias name '%s' in matterMeta.aliases", alias.name.c_str());
                return false;
            }

            meta.aliases.push_back(std::move(alias));
        }
    }

    return true;
}

bool SbmdParser::ParseReporting(const YAML::Node &node, SbmdReporting &reporting)
{
    if (!node.IsMap())
    {
        icError("reporting is not a map");
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

bool SbmdParser::ParseResource(const YAML::Node &node, SbmdResource &resource, const std::vector<SbmdAlias> &aliases)
{
    if (!node.IsMap())
    {
        icError("resource is not a map");
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

    if (node["optional"])
    {
        resource.optional = node["optional"].as<bool>();
    }

    if (node["mapper"])
    {
        if (!ParseMapper(node["mapper"], resource.mapper, aliases))
        {
            icError("Failed to parse mapper for resource %s", resource.id.c_str());
            return false;
        }

        if (!ValidateMapper(resource.mapper, resource.id))
        {
            icError("Mapper validation failed for resource %s", resource.id.c_str());
            return false;
        }
    }

    // Parse prerequisites if present
    bool prerequisitesDeclared = false;

    if (node["prerequisites"])
    {
        std::vector<SbmdPrerequisite> prereqs;

        if (!ParsePrerequisites(node["prerequisites"], prereqs, aliases))
        {
            icError("Failed to parse prerequisites for resource %s", resource.id.c_str());
            return false;
        }

        resource.prerequisites = std::move(prereqs);
        prerequisitesDeclared = true;
    }

    // Enforce that every resource must declare prerequisites
    if (!prerequisitesDeclared)
    {
        icError("Resource '%s' is missing required 'prerequisites' field "
                "(use 'prerequisites: none' to explicitly opt out of gating)",
                resource.id.c_str());
        return false;
    }

    return true;
}

bool SbmdParser::ParseEndpoint(const YAML::Node &node, SbmdEndpoint &endpoint, const std::vector<SbmdAlias> &aliases)
{
    if (!node.IsMap())
    {
        icError("endpoint is not a map");
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
            if (ParseResource(resourceNode, resource, aliases))
            {
                SetMapperIds(resource, endpoint.id);
                endpoint.resources.push_back(resource);
            }
            else
            {
                icError("Failed to parse resource in endpoint %s", endpoint.id.c_str());
                return false;
            }
        }
    }

    return true;
}

bool SbmdParser::ParseMapper(const YAML::Node &node, SbmdMapper &mapper, const std::vector<SbmdAlias> &aliases)
{
    if (!node.IsMap())
    {
        icError("mapper is not a map");
        return false;
    }

    // Parse read mapping
    if (node["read"])
    {
        const YAML::Node &readNode = node["read"];
        mapper.hasRead = true;

        if (readNode["alias"])
        {
            if (readNode["command"])
            {
                icError("read mapper cannot have both 'alias' and 'command'");
                return false;
            }

            std::string aliasName = readNode["alias"].as<std::string>();
            const SbmdAlias *alias = FindAlias(aliases, aliasName);

            if (!alias)
            {
                icError("read mapper references unknown alias '%s'", aliasName.c_str());
                return false;
            }

            if (!alias->attribute.has_value())
            {
                icError("read mapper alias '%s' must be an attribute alias (not event)", aliasName.c_str());
                return false;
            }

            mapper.readAttribute = alias->attribute;
        }
        else if (readNode["command"])
        {
            SbmdCommand cmd;

            if (!ParseCommand(readNode["command"], cmd))
            {
                icError("Failed to parse read command");
                return false;
            }

            mapper.readCommand = cmd;
        }
        else
        {
            icError("read mapper must have either 'alias' or 'command'");
            return false;
        }

        if (readNode["script"])
        {
            mapper.readScript = readNode["script"].as<std::string>();
        }
    }

    // Parse write mapping - script-only, no metadata
    if (node["write"])
    {
        const YAML::Node &writeNode = node["write"];
        mapper.hasWrite = true;

        if (writeNode["script"])
        {
            mapper.writeScript = writeNode["script"].as<std::string>();
        }
    }

    // Parse execute mapping - script-only, no metadata
    if (node["execute"])
    {
        const YAML::Node &executeNode = node["execute"];
        mapper.hasExecute = true;

        if (executeNode["script"])
        {
            mapper.executeScript = executeNode["script"].as<std::string>();
        }

        if (executeNode["scriptResponse"])
        {
            mapper.executeResponseScript = executeNode["scriptResponse"].as<std::string>();
        }
    }

    // Parse event mapping
    if (node["event"])
    {
        const YAML::Node &eventNode = node["event"];

        if (eventNode["alias"])
        {
            std::string aliasName = eventNode["alias"].as<std::string>();
            const SbmdAlias *alias = FindAlias(aliases, aliasName);

            if (!alias)
            {
                icError("event mapper references unknown alias '%s'", aliasName.c_str());
                return false;
            }

            if (!alias->event.has_value())
            {
                icError("event mapper alias '%s' must be an event alias (not attribute)", aliasName.c_str());
                return false;
            }

            mapper.event = alias->event;
        }
        else
        {
            icError("event mapper must specify 'alias'");
            return false;
        }

        if (eventNode["script"])
        {
            mapper.eventScript = eventNode["script"].as<std::string>();
        }
    }

    return true;
}

bool SbmdParser::ParseAlias(const YAML::Node &node, SbmdAlias &alias)
{
    if (!node.IsMap())
    {
        icError("alias entry is not a map");
        return false;
    }

    if (!node["name"])
    {
        icError("alias entry is missing required 'name' field");
        return false;
    }

    alias.name = node["name"].as<std::string>();

    if (alias.name.empty())
    {
        icError("alias 'name' must not be empty");
        return false;
    }

    bool hasAttribute = node["attribute"].IsDefined();
    bool hasEvent = node["event"].IsDefined();

    if (hasAttribute && hasEvent)
    {
        icError("alias '%s' must not have both 'attribute' and 'event'", alias.name.c_str());
        return false;
    }

    if (!hasAttribute && !hasEvent)
    {
        icError("alias '%s' must have either 'attribute' or 'event'", alias.name.c_str());
        return false;
    }

    if (hasAttribute)
    {
        SbmdAttribute attr;

        if (!ParseAttribute(node["attribute"], attr))
        {
            icError("Failed to parse attribute in alias '%s'", alias.name.c_str());
            return false;
        }

        alias.attribute = attr;
    }
    else
    {
        SbmdEvent evt;

        if (!ParseEvent(node["event"], evt))
        {
            icError("Failed to parse event in alias '%s'", alias.name.c_str());
            return false;
        }

        alias.event = evt;
    }

    return true;
}

bool SbmdParser::ParseAttribute(const YAML::Node &node, SbmdAttribute &attribute)
{
    if (!node.IsMap())
    {
        icError("attribute is not a map");
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
        icError("command is not a map");
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

    // Parse timed invoke timeout (if specified, command requires timed invoke)
    if (node["timedInvokeTimeoutMs"])
    {
        uint32_t timeoutValue = node["timedInvokeTimeoutMs"].as<uint32_t>();
        if (timeoutValue > UINT16_MAX)
        {
            if (!command.name.empty())
            {
                icLogError(LOG_TAG, logFmt("timedInvokeTimeoutMs value %u for command '%s' exceeds maximum allowed value of %u"),
                           timeoutValue, command.name.c_str(), UINT16_MAX);
            }
            else
            {
                icLogError(LOG_TAG, logFmt("timedInvokeTimeoutMs value %u exceeds maximum allowed value of %u"),
                           timeoutValue, UINT16_MAX);
            }
            return false;
        }
        command.timedInvokeTimeoutMs = static_cast<uint16_t>(timeoutValue);
    }

    // Parse command arguments
    if (node["args"] && node["args"].IsSequence())
    {
        for (const auto &argNode : node["args"])
        {
            SbmdArgument arg;
            if (argNode["name"])
            {
                arg.name = argNode["name"].as<std::string>();
            }
            if (argNode["type"])
            {
                arg.type = argNode["type"].as<std::string>();
            }
            command.args.push_back(arg);
        }
    }

    return true;
}

bool SbmdParser::ParseEvent(const YAML::Node &node, SbmdEvent &event)
{
    if (!node.IsMap())
    {
        icError("event is not a map");
        return false;
    }

    if (node["clusterId"])
    {
        std::string clusterId = node["clusterId"].as<std::string>();
        event.clusterId = ParseHexOrDecimal(clusterId);
    }

    if (node["eventId"])
    {
        std::string eventId = node["eventId"].as<std::string>();
        event.eventId = ParseHexOrDecimal(eventId);
    }

    if (node["name"])
    {
        event.name = node["name"].as<std::string>();
    }

    return true;
}

uint32_t SbmdParser::ParseHexOrDecimal(const std::string &value)
{
    if (value.empty())
    {
        return 0;
    }

    try
    {
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
    catch (const std::invalid_argument &e)
    {
        icLogError(LOG_TAG, "(%s): Invalid numeric value '%s': %s", __func__, value.c_str(), e.what());
        return 0;
    }
    catch (const std::out_of_range &e)
    {
        icLogError(LOG_TAG, "(%s): Numeric value '%s' out of range: %s", __func__, value.c_str(), e.what());
        return 0;
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

bool SbmdParser::ParsePrerequisites(const YAML::Node &node,
                                    std::vector<SbmdPrerequisite> &out,
                                    const std::vector<SbmdAlias> &aliases)
{
    // prerequisites: none (null or scalar "none") -> empty vector, always register
    if (!node.IsDefined() || node.IsNull() || (node.IsScalar() && node.as<std::string>() == "none"))
    {
        out.clear();
        return true;
    }

    if (!node.IsSequence())
    {
        icError("prerequisites must be a sequence, 'none', or null");
        return false;
    }

    if (node.size() == 0)
    {
        icError("prerequisites sequence must not be empty; use 'prerequisites: none' to indicate no prerequisites");
        return false;
    }

    for (const auto &entry : node)
    {
        if (!entry.IsMap())
        {
            icError("each prerequisite entry must be a map");
            return false;
        }

        for (const auto &kv : entry)
        {
            if (kv.first.as<std::string>() != "alias")
            {
                icError("prerequisite entry has unexpected key '%s'; only 'alias' is allowed",
                        kv.first.as<std::string>().c_str());
                return false;
            }
        }

        if (!entry["alias"].IsDefined())
        {
            icError("prerequisite entry must have an 'alias' key referencing a name in matterMeta.aliases");
            return false;
        }

        std::string aliasName = entry["alias"].as<std::string>();
        const SbmdAlias *alias = FindAlias(aliases, aliasName);

        if (!alias)
        {
            icError("prerequisite references unknown alias '%s'", aliasName.c_str());
            return false;
        }

        SbmdPrerequisite prereq;

        if (alias->attribute.has_value())
        {
            prereq.clusterId = alias->attribute->clusterId;
            prereq.attributeIds = {alias->attribute->attributeId};
        }
        else if (alias->event.has_value())
        {
            prereq.clusterId = alias->event->clusterId;
            // event prerequisite: cluster presence is sufficient (no attribute check)
        }
        else
        {
            icError("alias '%s' has neither attribute nor event (internal error)", aliasName.c_str());
            return false;
        }

        out.push_back(std::move(prereq));
    }

    return true;
}

} // namespace barton
