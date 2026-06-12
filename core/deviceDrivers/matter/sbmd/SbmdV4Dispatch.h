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
 *
 * Dispatch table construction and handler lookup for v4 SBMD drivers.
 *
 * Maps incoming Matter attribute/event/command reports to the right handler
 * functions based on alias resolution and priority ordering.
 *
 * Priority order (all matching handlers fire):
 *   1. Specific — handler with a single alias resolving to one (clusterId, elementId)
 *   2. Multi   — handler with multiple aliases
 *   3. Wildcard — handler matching any element in a cluster
 *
 * The dispatch table is built at driver activation time from the registration's
 * alias map and handler declarations.
 */

#pragma once

#include "SbmdV4Registration.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

extern "C" {
#include <mquickjs/mquickjs.h>
}

namespace barton
{
    /**
     * Priority level for handler matching.
     */
    enum class HandlerPriority
    {
        Specific, // Single alias → single (clusterId, elementId)
        Multi,    // Multiple aliases → multiple (clusterId, elementId) pairs
        Wildcard  // Matches any element in a cluster
    };

    /**
     * A handler entry in the dispatch table — points back to the registration's
     * device handler and carries its resolved priority.
     */
    struct DispatchEntry
    {
        const SbmdV4DeviceHandler *handler; // Non-owning pointer into the registration
        HandlerPriority priority;
    };

    /**
     * Composite key for dispatch table lookup: (clusterId, elementId).
     * elementId is attributeId, eventId, or commandId depending on the table.
     */
    struct DispatchKey
    {
        uint32_t clusterId;
        uint32_t elementId;

        bool operator<(const DispatchKey &other) const
        {
            if (clusterId != other.clusterId)
            {
                return clusterId < other.clusterId;
            }

            return elementId < other.elementId;
        }

        bool operator==(const DispatchKey &other) const
        {
            return clusterId == other.clusterId && elementId == other.elementId;
        }
    };

    /**
     * A dispatch table that maps (clusterId, elementId) to a priority-sorted
     * list of handler entries. Also maintains a wildcard table keyed by
     * clusterId only.
     */
    class SbmdV4DispatchTable
    {
    public:
        /**
         * Build a dispatch table from the registration's aliases and a handler vector.
         *
         * @param aliases The registration's alias map (name → SbmdV4Alias)
         * @param handlers The device handler vector (attributeHandlers, eventHandlers, or commandHandlers)
         * @param aliasElementGetter Function to extract the relevant element ID from an alias
         *        (e.g. attributeId for attribute dispatch, eventId for event dispatch)
         */
        void Build(const std::unordered_map<std::string, SbmdV4Alias> &aliases,
                   const std::vector<SbmdV4DeviceHandler> &handlers);

        /**
         * Look up all matching handlers for a given (clusterId, elementId),
         * ordered by priority (specific first, then multi, then wildcard).
         *
         * @param clusterId The Matter cluster ID
         * @param elementId The attribute/event/command ID
         * @return Ordered list of matching handler entries (may be empty)
         */
        std::vector<const DispatchEntry *> Lookup(uint32_t clusterId, uint32_t elementId) const;

        /**
         * Clear all entries.
         */
        void Clear();

        /**
         * Get the number of specific+multi entries (for diagnostics).
         */
        size_t GetSpecificEntryCount() const;

        /**
         * Get the number of wildcard entries (for diagnostics).
         */
        size_t GetWildcardEntryCount() const;

    private:
        // Specific + multi entries: (clusterId, elementId) → sorted entries
        std::map<DispatchKey, std::vector<DispatchEntry>> specificTable;

        // Wildcard entries: clusterId → sorted entries (match any elementId in that cluster)
        std::map<uint32_t, std::vector<DispatchEntry>> wildcardTable;
    };

} // namespace barton
