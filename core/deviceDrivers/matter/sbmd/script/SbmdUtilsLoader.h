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

//
// Created by tlea on 2/19/26
//

#pragma once

#include <cstddef>
#include <quickjs/quickjs.h>

namespace barton
{
    /**
     * Loader for the SBMD utilities bundle.
     *
     * This class loads the SBMD utilities into a QuickJS context, exposing a
     * global 'SbmdUtils' object with:
     * - Base64: encode/decode utilities
     * - Tlv: TLV encoding/decoding for Matter types
     * - Response: helpers for building invoke/write responses
     *
     * Unlike the MatterClusters bundle, this is always loaded into every
     * SBMD QuickJS context since it provides essential utilities for all
     * SBMD scripts regardless of whether they use matter.js.
     *
     * Example usage in SBMD scripts:
     * @code
     *   // Decode TLV attribute value
     *   const value = SbmdUtils.Tlv.decode(sbmdReadArgs.tlvBase64);
     *
     *   // Encode a value for attribute write
     *   const tlv = SbmdUtils.Tlv.encode(42, 'uint16');
     *   return SbmdUtils.Response.write(0x0008, 0x0000, tlv);
     *
     *   // Create invoke response for command
     *   return SbmdUtils.Response.invoke(0x0006, 0x0001);  // On command
     * @endcode
     */
    class SbmdUtilsLoader
    {
    public:
        /**
         * Load the SBMD utilities bundle into the given QuickJS context.
         *
         * This creates a global 'SbmdUtils' object in the context. The object
         * is frozen after loading to prevent modification by scripts.
         *
         * @param ctx The QuickJS context to load the utilities into
         * @return true if the utilities were loaded successfully, false otherwise
         */
        static bool LoadBundle(JSContext *ctx);

        /**
         * Check if the SBMD utilities bundle is available.
         *
         * @return true if the bundle is available (should always be true when
         *         properly built)
         */
        static bool IsAvailable();

        /**
         * Get the source of the loaded bundle.
         *
         * @return "embedded" if loaded from compiled-in source, or "none" if not loaded
         */
        static const char *GetSource();

    private:
        static bool LoadFromEmbedded(JSContext *ctx);
        static bool ExecuteBundle(JSContext *ctx, const char *bundleSource, size_t length);

        static const char *source_;
    };

} // namespace barton
