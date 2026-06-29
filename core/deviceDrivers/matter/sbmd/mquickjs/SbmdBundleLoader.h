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

extern "C" {
#include <mquickjs/mquickjs.h>
}

namespace barton
{
    /**
     * Loader for SBMD JavaScript bundles.
     *
     * Loads the SBMD bundle into a mquickjs context, exposing a
     * global 'Sbmd' object with:
     * - Base64: encode/decode utilities
     * - Tlv: TLV encoding/decoding for Matter types
     * - result(): builder for handler return values
     *
     * The bundle is assembled at build time from individual source files.
     */
    class SbmdBundleLoader
    {
    public:
        /**
         * Load all SBMD bundles into the given mquickjs context.
         *
         * This creates a global 'Sbmd' object in the context with all
         * sub-namespaces. The object is frozen after loading to prevent
         * modification by scripts.
         *
         * @param ctx The mquickjs context to load the bundles into
         * @return true if all bundles were loaded successfully, false otherwise
         */
        static bool LoadBundle(JSContext *ctx);

        /**
         * Check if the SBMD bundles are available.
         *
         * @return true if the bundles are available (should always be true when
         *         properly built)
         */
        static bool IsAvailable();

        /**
         * Get the source of the loaded bundles.
         *
         * @return "embedded" if loaded from compiled-in source, or "none" if not loaded
         */
        static const char *GetSource();

    private:
        static bool LoadFromEmbedded(JSContext *ctx);
        static bool ExecuteBundle(JSContext *ctx, const char *bundleSource, size_t length, const char *name);

        static const char *source;
    };

} // namespace barton
