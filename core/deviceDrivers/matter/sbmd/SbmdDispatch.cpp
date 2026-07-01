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
 * Created by tlea on 6/12/2026
 */

#define LOG_TAG "SbmdDispatch"
#define logFmt(fmt) "(%s): " fmt, __func__

#include "SbmdDispatch.h"

#include <algorithm>

extern "C" {
#include <icLog/logging.h>
}

namespace barton
{
    void SbmdDispatchTable::Build(const std::unordered_map<std::string, SbmdAlias> &aliases,
                                  const std::vector<SbmdDeviceHandler> &handlers)
    {
        Clear();

        for (const auto &handler : handlers)
        {
            // Determine priority based on alias count
            HandlerPriority priority =
                handler.aliases.size() == 1 ? HandlerPriority::Specific : HandlerPriority::Multi;

            DispatchEntry entry;
            entry.handler = &handler;
            entry.priority = priority;

            // Resolve each alias name to a dispatch key
            for (const auto &aliasName : handler.aliases)
            {
                auto aliasIt = aliases.find(aliasName);

                if (aliasIt == aliases.end())
                {
                    icWarn("handler '%s' references unknown alias '%s', skipping",
                           handler.name.c_str(),
                           aliasName.c_str());
                    continue;
                }

                const SbmdAlias &alias = aliasIt->second;

                // Determine the element ID from the alias — use whichever is set
                std::optional<uint32_t> elementId;

                if (alias.attributeId.has_value())
                {
                    elementId = alias.attributeId;
                }
                else if (alias.eventId.has_value())
                {
                    elementId = alias.eventId;
                }
                else if (alias.commandId.has_value())
                {
                    elementId = alias.commandId;
                }

                if (!elementId.has_value())
                {
                    // Wildcard — no specific element ID, matches all in this cluster
                    wildcardTable[alias.clusterId].push_back(
                        DispatchEntry{entry.handler, HandlerPriority::Wildcard});
                    continue;
                }

                DispatchKey key{alias.clusterId, elementId.value()};
                specificTable[key].push_back(entry);
            }
        }

        // Sort each entry list by priority (Specific < Multi < Wildcard)
        for (auto &[key, entries] : specificTable)
        {
            std::stable_sort(entries.begin(), entries.end(), [](const DispatchEntry &a, const DispatchEntry &b) {
                return static_cast<int>(a.priority) < static_cast<int>(b.priority);
            });
        }

        for (auto &[clusterId, entries] : wildcardTable)
        {
            std::stable_sort(entries.begin(), entries.end(), [](const DispatchEntry &a, const DispatchEntry &b) {
                return static_cast<int>(a.priority) < static_cast<int>(b.priority);
            });
        }
    }

    std::vector<const DispatchEntry *> SbmdDispatchTable::Lookup(uint32_t clusterId, uint32_t elementId) const
    {
        std::vector<const DispatchEntry *> result;

        // First, specific + multi matches
        auto it = specificTable.find(DispatchKey{clusterId, elementId});

        if (it != specificTable.end())
        {
            for (const auto &entry : it->second)
            {
                result.push_back(&entry);
            }
        }

        // Then, wildcard matches for this cluster
        auto wcIt = wildcardTable.find(clusterId);

        if (wcIt != wildcardTable.end())
        {
            for (const auto &entry : wcIt->second)
            {
                result.push_back(&entry);
            }
        }

        return result;
    }

    void SbmdDispatchTable::Clear()
    {
        specificTable.clear();
        wildcardTable.clear();
    }

    size_t SbmdDispatchTable::GetSpecificEntryCount() const
    {
        size_t count = 0;

        for (const auto &[key, entries] : specificTable)
        {
            count += entries.size();
        }

        return count;
    }

    size_t SbmdDispatchTable::GetWildcardEntryCount() const
    {
        size_t count = 0;

        for (const auto &[clusterId, entries] : wildcardTable)
        {
            count += entries.size();
        }

        return count;
    }

    std::set<uint32_t> SbmdDispatchTable::GetRegisteredClusterIds() const
    {
        std::set<uint32_t> clusterIds;

        for (const auto &[key, entries] : specificTable)
        {
            clusterIds.insert(key.clusterId);
        }

        for (const auto &[clusterId, entries] : wildcardTable)
        {
            clusterIds.insert(clusterId);
        }

        return clusterIds;
    }

} // namespace barton
