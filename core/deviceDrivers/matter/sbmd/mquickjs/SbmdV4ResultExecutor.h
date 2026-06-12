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
 * Walks and executes a v4 handler result chain ({ops, terminal}).
 *
 * The result chain is a JSValue with:
 *   - ops: array of non-terminal operation objects
 *   - terminal: a single terminal operation object
 *
 * This class extracts the ops and terminal from the JSValue using the
 * mquickjs API, then calls the appropriate executor methods. Device-level
 * operations (sendCommand, writeAttribute, etc.) are delegated to a
 * callback interface so the executor is decoupled from MatterDevice.
 *
 * All JSValue walking happens while the caller holds MQuickJsRuntime::GetMutex().
 * Non-terminal ops that don't need the JS context (updateResource, log, etc.)
 * are collected into a list and executed AFTER releasing the mutex.
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

extern "C" {
#include <mquickjs/mquickjs.h>
}

namespace barton
{
    /**
     * Parsed non-terminal operation from the ops array.
     */
    struct ResultOp
    {
        struct UpdateResource
        {
            std::optional<std::string> endpoint; // absent = use trigger endpoint
            std::string resource;
            std::string value;
        };

        struct SetMetadata
        {
            std::string endpoint;
            std::string resource;
            std::string key;
            std::string value;
        };

        struct SetPersistentData
        {
            std::string key;
            std::string value;
        };

        struct SetTransientData
        {
            std::string key;
            std::string value;
        };

        struct Log
        {
            std::string message;
        };

        using Data = std::variant<UpdateResource, SetMetadata, SetPersistentData, SetTransientData, Log>;
        Data data;
    };

    /**
     * Parsed terminal operation.
     */
    struct ResultTerminal
    {
        struct Success
        {
        };

        struct Error
        {
            std::string message;
        };

        struct SendCommand
        {
            uint32_t clusterId;
            uint32_t commandId;
            std::string tlvBase64; // raw base64 string, empty if no payload
            std::optional<uint32_t> endpointId;
            std::optional<uint16_t> timedInvokeTimeoutMs;
        };

        struct WriteAttribute
        {
            uint32_t clusterId;
            uint32_t attributeId;
            std::string tlvBase64;
            std::optional<uint32_t> endpointId;
        };

        struct RequestCommand
        {
            uint32_t clusterId;
            uint32_t commandId;
            std::string tlvBase64;
            std::optional<uint32_t> endpointId;
            std::optional<uint16_t> timedInvokeTimeoutMs;
            // Deferred fields — stored as JSValues for later handler invocation
            uint32_t responseCommandId;
            JSValue onResponse = JS_UNDEFINED;
            JSValue onError = JS_UNDEFINED;
            std::optional<uint32_t> timeoutMs;
        };

        struct ReadAttribute
        {
            uint32_t clusterId;
            uint32_t attributeId;
            std::optional<uint32_t> endpointId;
            JSValue onResponse = JS_UNDEFINED;
            JSValue onError = JS_UNDEFINED;
            std::optional<uint32_t> timeoutMs;
        };

        using Data = std::variant<Success, Error, SendCommand, WriteAttribute, RequestCommand, ReadAttribute>;
        Data data;
    };

    /**
     * Complete parsed result chain ready for execution.
     */
    struct ParsedResult
    {
        std::vector<ResultOp> ops;
        ResultTerminal terminal;
    };

    /**
     * Walks a v4 handler result JSValue and extracts it into ParsedResult.
     * Must be called while holding MQuickJsRuntime::GetMutex().
     */
    class SbmdV4ResultExecutor
    {
    public:
        /**
         * Parse a handler result JSValue into a ParsedResult.
         *
         * @param ctx The mquickjs context (caller must hold the mutex)
         * @param resultVal The {ops, terminal} JSValue from the handler
         * @return Parsed result, or std::nullopt on parse failure
         */
        static std::optional<ParsedResult> Parse(JSContext *ctx, JSValue resultVal);

    private:
        /**
         * Parse a single op from the ops array.
         */
        static std::optional<ResultOp> ParseOp(JSContext *ctx, JSValue opVal);

        /**
         * Parse the terminal object.
         */
        static std::optional<ResultTerminal> ParseTerminal(JSContext *ctx, JSValue termVal);
    };

} // namespace barton
