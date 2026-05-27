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

#pragma once

#include <platform/CHIPDeviceLayer.h>

#include <optional>
#include <string>
#include <variant>

// Forward declaration
namespace Json
{
    class Value;
}

namespace barton
{
    /**
     * Result from a write/execute mapper script.
     * The script returns either an 'invoke' (command) or 'write' (attribute) operation
     * with all the details needed to perform the operation.
     *
     * Moved here from SbmdScript.h so that ScriptResult (which references ScriptWriteResult
     * in its variant) can be defined in this single header without circular includes.
     */
    struct ScriptWriteResult
    {
        enum class OperationType
        {
            Invoke, // Command invocation
            Write   // Attribute write
        };

        OperationType type = OperationType::Invoke;

        // Common fields
        std::optional<chip::EndpointId> endpointId; // Optional - uses default if not specified
        chip::ClusterId clusterId = 0;

        // For Invoke operations
        chip::CommandId commandId = 0;
        std::optional<uint16_t> timedInvokeTimeoutMs; // For timed commands

        // For Write operations
        chip::AttributeId attributeId = 0;

        // TLV encoded payload (decoded from base64)
        chip::Platform::ScopedMemoryBuffer<uint8_t> tlvBuffer;
        size_t tlvLength = 0;
    };

    /**
     * Typed result returned by all SbmdScript mapper methods.
     *
     * A ScriptResult holds two optional fields — error and operation — whose
     * presence or absence determines the observable outcome:
     *
     *   - isError()      — error field is set; the script failed
     *   - hasOperation() — operation field is set; the script produced an action
     *   - isSuppressed() — neither field is set; the script ran successfully
     *                       but intentionally produced no action (derived state)
     *
     * ScriptResult is move-only because ScriptWriteResult contains a
     * chip::Platform::ScopedMemoryBuffer.
     */
    class ScriptResult
    {
    public:
        /**
         * Operation payload for read, event, seedFrom, and commandResponse mappers.
         * Carries the Barton resource string value to publish.
         */
        struct ResourceUpdate
        {
            std::string value;
        };

        ScriptResult() = default;
        ~ScriptResult() = default;

        ScriptResult(const ScriptResult &) = delete;
        ScriptResult &operator=(const ScriptResult &) = delete;

        ScriptResult(ScriptResult &&) = default;
        ScriptResult &operator=(ScriptResult &&) = default;

        /**
         * Returns true if the error field is set (script reported failure).
         */
        bool IsError() const { return error.has_value(); }

        /**
         * Returns true if neither error nor operation is set.
         * This is a derived convenience: the script ran successfully but
         * intentionally produced no action.
         */
        bool IsSuppressed() const { return !error.has_value() && !operation.has_value(); }

        /**
         * Returns true if the operation field is set (and no error).
         */
        bool HasOperation() const { return operation.has_value(); }

        /**
         * Returns the error message. Only valid when IsError() is true.
         */
        const std::string &ErrorMessage() const { return error.value(); }

        /**
         * Returns the operation variant. Only valid when HasOperation() is true.
         */
        const std::variant<ResourceUpdate, ScriptWriteResult> &Operation() const { return operation.value(); }

        /**
         * Parse a Json::Value object into a ScriptResult according to SBMD script
         * JSON schema v3.0.
         *
         * Valid top-level keys: "value", "invoke", "write", "error".
         * An empty object {} produces a suppress result.
         * More than one key present simultaneously returns an error result.
         *
         * @param jv The JSON object returned by the script (must be an object type)
         * @return A ScriptResult representing the parse outcome
         */
        static ScriptResult FromJsonValue(const Json::Value &jv);

        /**
         * Construct an error ScriptResult with the given message.
         */
        static ScriptResult MakeError(std::string message)
        {
            ScriptResult r;
            r.error = std::move(message);
            return r;
        }

        /**
         * Construct a suppress ScriptResult (no error, no operation).
         */
        static ScriptResult MakeSuppress()
        {
            return ScriptResult {};
        }

        /**
         * Construct a ResourceUpdate ScriptResult.
         */
        static ScriptResult MakeResourceUpdate(std::string value)
        {
            ScriptResult r;
            r.operation = ResourceUpdate {std::move(value)};
            return r;
        }

        /**
         * Construct a ScriptWriteResult operation ScriptResult.
         */
        static ScriptResult MakeWriteResult(ScriptWriteResult writeResult)
        {
            ScriptResult r;
            r.operation = std::move(writeResult);
            return r;
        }

    private:
        std::optional<std::string> error;
        std::optional<std::variant<ResourceUpdate, ScriptWriteResult>> operation;
    };

} // namespace barton
