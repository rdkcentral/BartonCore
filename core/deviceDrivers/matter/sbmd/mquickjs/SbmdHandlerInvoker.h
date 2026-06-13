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
 * Handler invocation for SBMD drivers.
 *
 * Builds the JS `args` object, calls a handler function, parses the result
 * chain, and executes non-terminal ops. Terminal execution is left to the
 * caller since it requires device/session context.
 *
 * All methods require the caller to hold MQuickJsRuntime::GetMutex().
 */

#pragma once

#include "../SbmdRegistration.h"
#include "SbmdResultExecutor.h"

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

extern "C" {
#include <mquickjs/mquickjs.h>
}

namespace barton
{
    /**
     * Context for a handler invocation — carries device-specific data
     * needed to build the `args` JS object and execute result ops.
     */
    struct HandlerContext
    {
        std::string deviceUuid;
        std::string endpointId;                          // The trigger endpoint
        std::map<uint32_t, uint32_t> clusterFeatureMaps; // clusterId → featureBitmap
    };

    /**
     * Callback to fetch a cached attribute value by alias name.
     *
     * The implementation should resolve the alias to (clusterId, attributeId),
     * read the TLV from the device data cache, and return it as a base64 string.
     * Returns nullopt if the attribute is not cached or the alias is unknown.
     */
    using AttributeSupplementFetcher = std::function<std::optional<std::string>(const std::string &aliasName)>;

    /**
     * Callback to fetch a resource value by path.
     *
     * Path format: "endpointId/resourceId" for endpoint resources, or
     * "resourceId" for device-level resources.
     * Returns nullopt if the resource is not found.
     */
    using ResourceSupplementFetcher = std::function<std::optional<std::string>(const std::string &path)>;

    /**
     * Invokes handler functions and parses their results.
     *
     * Usage:
     *   1. Build trigger-specific args via BuildAttributeArgs / BuildResourceArgs / etc.
     *   2. Call InvokeHandler with the handler JSValue and args
     *   3. Process the returned ParsedResult (execute ops, handle terminal)
     */
    class SbmdHandlerInvoker
    {
    public:
        /**
         * Build an args object for an attribute handler invocation.
         *
         * @param ctx JS context (caller holds mutex)
         * @param hctx Device/handler context
         * @param clusterId The triggering cluster ID
         * @param attributeId The triggering attribute ID
         * @param tlvBase64 The TLV-encoded attribute value as base64 (may be empty)
         * @return JS args object, or JS_EXCEPTION on failure
         */
        static JSValue BuildAttributeArgs(JSContext *ctx,
                                          const HandlerContext &hctx,
                                          uint32_t clusterId,
                                          uint32_t attributeId,
                                          const std::string &tlvBase64);

        /**
         * Build an args object for a resource handler (seed/read/write/execute).
         *
         * @param ctx JS context (caller holds mutex)
         * @param hctx Device/handler context
         * @param resourceId The resource ID
         * @param input The input value (null for seed/read, value for write, arg for execute)
         * @return JS args object, or JS_EXCEPTION on failure
         */
        static JSValue BuildResourceArgs(JSContext *ctx,
                                         const HandlerContext &hctx,
                                         const std::string &resourceId,
                                         const std::optional<std::string> &input);

        /**
         * Call a handler function with the given args object.
         *
         * @param ctx JS context (caller holds mutex)
         * @param handler The handler JSValue (must be a function)
         * @param args The args object (consumed by the call)
         * @return Parsed result chain, or nullopt on failure
         */
        static std::optional<ParsedResult> InvokeHandler(JSContext *ctx, JSValue handler, JSValue args);

        /**
         * Execute the non-terminal ops from a parsed result.
         * Calls updateResource, setMetadata, setPersistentData, setTransientData, log.
         *
         * @param hctx Handler context (for device UUID and default endpoint)
         * @param ops The ops to execute
         */
        static void ExecuteOps(const HandlerContext &hctx, const std::vector<ResultOp> &ops);

        /**
         * Add supplements to an args object. Fetches pre-declared attribute and
         * resource values and attaches them as `args.supplements`.
         *
         * If supplements is empty (no attributes and no resources), this is a no-op.
         *
         * @param ctx JS context (caller holds mutex)
         * @param args The args object to augment (modified in place)
         * @param supplements The supplement declarations
         * @param attrFetcher Callback to fetch attribute values by alias name
         * @param resFetcher Callback to fetch resource values by path
         */
        static void AddSupplements(JSContext *ctx,
                                   JSValue args,
                                   const SbmdSupplements &supplements,
                                   const AttributeSupplementFetcher &attrFetcher,
                                   const ResourceSupplementFetcher &resFetcher);

        /**
         * Build an args object for a deferred command response handler.
         *
         * Creates: { deviceUuid, endpointId, clusterFeatureMaps, response: { clusterId, commandId, data } }
         *
         * @param ctx JS context (caller holds mutex)
         * @param hctx Device/handler context
         * @param clusterId The response cluster ID
         * @param commandId The response command ID
         * @param tlvBase64 The TLV-encoded response data as base64 (may be empty if no data)
         * @return JS args object, or JS_EXCEPTION on failure
         */
        static JSValue BuildCommandResponseArgs(JSContext *ctx,
                                                const HandlerContext &hctx,
                                                uint32_t clusterId,
                                                uint32_t commandId,
                                                const std::string &tlvBase64);

        /**
         * Build an args object for a deferred attribute read response handler.
         *
         * Creates: { deviceUuid, endpointId, clusterFeatureMaps, attribute: { clusterId, attributeId, value } }
         *
         * @param ctx JS context (caller holds mutex)
         * @param hctx Device/handler context
         * @param clusterId The attribute cluster ID
         * @param attributeId The attribute ID
         * @param tlvBase64 The TLV-encoded attribute value as base64
         * @return JS args object, or JS_EXCEPTION on failure
         */
        static JSValue BuildAttributeReadResponseArgs(JSContext *ctx,
                                                      const HandlerContext &hctx,
                                                      uint32_t clusterId,
                                                      uint32_t attributeId,
                                                      const std::string &tlvBase64);

        /**
         * Build an args object for a deferred error handler.
         *
         * Creates: { deviceUuid, endpointId, clusterFeatureMaps, error: { type, message } }
         *
         * @param ctx JS context (caller holds mutex)
         * @param hctx Device/handler context
         * @param errorType The error type string (e.g., "timeout", "commandFailed")
         * @param errorMessage A descriptive error message
         * @return JS args object, or JS_EXCEPTION on failure
         */
        static JSValue BuildDeferredErrorArgs(JSContext *ctx,
                                              const HandlerContext &hctx,
                                              const std::string &errorType,
                                              const std::string &errorMessage);

    private:
        /**
         * Build the common base args object with deviceUuid, endpointId, clusterFeatureMaps.
         */
        static JSValue BuildBaseArgs(JSContext *ctx, const HandlerContext &hctx);
    };

} // namespace barton
