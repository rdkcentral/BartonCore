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

#include "SbmdSpec.h"
#include <string>
#include <memory>

namespace YAML
{
    class Node;
}

namespace barton
{
    /**
     * Parser for SBMD (Schema-Based Matter Driver) YAML files
     */
    class SbmdParser
    {
    public:
        /**
         * Parse an SBMD YAML file from a file path
         * @param filePath Path to the YAML file
         * @return Parsed SbmdSpec, or nullptr on error
         */
        static std::unique_ptr<SbmdSpec> ParseFile(const std::string &filePath);

        /**
         * Parse an SBMD YAML string
         * @param yamlContent YAML content as a string
         * @return Parsed SbmdSpec, or nullptr on error
         */
        static std::unique_ptr<SbmdSpec> ParseString(const std::string &yamlContent);

    private:
        // Common parsing implementation
        static std::unique_ptr<SbmdSpec> ParseYamlNode(const YAML::Node &root);

        // Helper methods for parsing different sections
        static bool ParseBartonMeta(const YAML::Node &node, SbmdBartonMeta &meta);
        static bool ParseMatterMeta(const YAML::Node &node, SbmdMatterMeta &meta);
        static bool ParseReporting(const YAML::Node &node, SbmdReporting &reporting);
        static bool ParseResource(const YAML::Node &node, SbmdResource &resource);
        static bool ParseEndpoint(const YAML::Node &node, SbmdEndpoint &endpoint);
        static bool ParseMapper(const YAML::Node &node, SbmdMapper &mapper);
        static bool ParseAttribute(const YAML::Node &node, SbmdAttribute &attribute);
        static bool ParseCommand(const YAML::Node &node, SbmdCommand &command);

        // Utility methods
        static uint32_t ParseHexOrDecimal(const std::string &value);
        static std::vector<std::string> ParseStringArray(const YAML::Node &node);
    };

} // namespace barton
