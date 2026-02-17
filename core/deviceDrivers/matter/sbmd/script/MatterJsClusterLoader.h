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
// Created by tlea on 2/17/26
//

#pragma once

#include <cstddef>

// Forward declarations for QuickJS types
struct JSContext;

namespace barton
{
    /**
     * Loader for the matter.js cluster bundle.
     *
     * This class loads the matter.js cluster definitions into a QuickJS context,
     * exposing a global 'MatterClusters' object with:
     * - All Matter cluster namespaces (LevelControl, OnOff, DoorLock, etc.)
     * - Each cluster contains TLV schema objects (TlvMoveToLevelRequest, etc.)
     * - Methods: encode(jsonData) -> Uint8Array, decode(Uint8Array) -> jsonData
     *
     * The bundle is loaded from the embedded header (MatterJsClustersEmbedded.h)
     * which is compiled into the binary.
     *
     * This component requires BCORE_MATTER_USE_MATTERJS=ON at build time.
     *
     * Example usage in SBMD scripts:
     * @code
     *   const { TlvMoveToLevelRequest } = MatterClusters.LevelControl.LevelControl;
     *   const encoded = TlvMoveToLevelRequest.encode({
     *     level: 50,
     *     transitionTime: null,
     *     optionsMask: {},
     *     optionsOverride: {}
     *   });
     * @endcode
     */
    class MatterJsClusterLoader
    {
    public:
        /**
         * Load the matter.js cluster bundle into the given QuickJS context.
         *
         * This creates a global 'MatterClusters' object in the context containing
         * all Matter cluster TLV schemas. The object is frozen after loading to
         * prevent modification by scripts.
         *
         * @param ctx The QuickJS context to load the bundle into
         * @return true if the bundle was loaded successfully, false otherwise
         */
        static bool LoadBundle(JSContext *ctx);

        /**
         * Check if the matter.js cluster bundle is available.
         *
         * @return true if the bundle was compiled in (BCORE_MATTER_USE_MATTERJS=ON)
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
        static bool FreezeGlobalObject(JSContext *ctx, const char *name);

        static const char *source_;
    };

} // namespace barton
