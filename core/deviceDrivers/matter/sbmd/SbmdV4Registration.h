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
 * C++ data structures extracted from a v4 SbmdDriver({...}) registration object.
 * These hold the metadata and handler references for a single .sbmd.js driver.
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

extern "C" {
#include <mquickjs/mquickjs.h>
}

namespace barton
{
    /**
     * A resolved alias — a named reference to a Matter cluster element.
     * Exactly one of attributeId, eventId, or commandId is set.
     */
    struct SbmdV4Alias
    {
        std::string name;
        uint32_t clusterId = 0;
        std::optional<uint32_t> attributeId;
        std::optional<uint32_t> eventId;
        std::optional<uint32_t> commandId;
        std::string type; // Documentation-only type string
    };

    /**
     * Supplement declarations for a handler — what data to pre-fetch before calling it.
     */
    struct SbmdV4Supplements
    {
        std::vector<std::string> attributes; // Alias names to resolve and fetch from device data cache
        std::vector<std::string> resources;  // Resource paths ("endpointId/resourceId") to fetch
    };

    /**
     * A resource handler declaration (seed, read, write, or execute).
     * For simple declarations (just a function), only handler is set.
     * For object declarations, supplements and handler are both set.
     */
    struct SbmdV4ResourceHandler
    {
        JSValue handler = JS_UNDEFINED; // GC-rooted function reference
        SbmdV4Supplements supplements;
    };

    /**
     * A v4 resource declaration within an endpoint.
     */
    struct SbmdV4Resource
    {
        std::string id;
        std::string type;
        std::vector<std::string> modes;
        bool optional = false;
        std::vector<std::string> prerequisites; // Alias names for prerequisite checks

        std::optional<SbmdV4ResourceHandler> seed;
        std::optional<SbmdV4ResourceHandler> read;
        std::optional<SbmdV4ResourceHandler> write;
        std::optional<SbmdV4ResourceHandler> execute;
    };

    /**
     * A v4 endpoint declaration containing resources.
     */
    struct SbmdV4Endpoint
    {
        std::string id;
        std::string profile;
        uint32_t profileVersion = 0;
        std::vector<SbmdV4Resource> resources;
    };

    /**
     * An attribute/event/command handler registration.
     */
    struct SbmdV4DeviceHandler
    {
        std::string name;                    // Handler registration name
        std::vector<std::string> aliases;    // Alias names this handler matches
        JSValue handler = JS_UNDEFINED;      // GC-rooted function reference
        SbmdV4Supplements supplements;
    };

    /**
     * Barton device class metadata.
     */
    struct SbmdV4BartonMeta
    {
        std::string deviceClass;
        uint32_t deviceClassVersion = 0;
    };

    /**
     * Matter device type matching metadata.
     */
    struct SbmdV4MatterMeta
    {
        std::vector<uint16_t> deviceTypes;
        std::optional<uint32_t> revision;
        std::vector<uint32_t> featureClusters;
        std::optional<uint16_t> vendorId;
        std::optional<uint16_t> productId;
        std::optional<uint32_t> defaultTimeoutMs;
    };

    /**
     * Reporting configuration for attribute subscriptions.
     */
    struct SbmdV4Reporting
    {
        uint16_t minSecs = 0;
        uint16_t maxSecs = 0;
    };

    /**
     * Complete v4 registration extracted from a SbmdDriver({...}) call.
     * Metadata fields are always populated. Handler JSValues are only valid
     * when the driver is activated (GC-rooted).
     */
    struct SbmdV4Registration
    {
        // Metadata — always available
        std::string schemaVersion;
        std::string driverVersion;
        std::string name;
        std::string filePath; // Source file path for diagnostics

        SbmdV4BartonMeta barton;
        SbmdV4MatterMeta matter;
        SbmdV4Reporting reporting;

        // Aliases — keyed by name
        std::unordered_map<std::string, SbmdV4Alias> aliases;

        // Endpoints with resources
        std::vector<SbmdV4Endpoint> endpoints;

        // Device-initiated message handlers
        std::vector<SbmdV4DeviceHandler> attributeHandlers;
        std::vector<SbmdV4DeviceHandler> eventHandlers;
        std::vector<SbmdV4DeviceHandler> commandHandlers;

        // Whether handler JSValues are currently GC-rooted (driver is activated)
        bool activated = false;
    };

} // namespace barton
