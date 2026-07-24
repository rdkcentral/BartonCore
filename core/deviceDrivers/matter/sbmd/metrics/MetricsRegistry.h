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

#ifdef BARTON_CONFIG_SBMD_METRICS

#include <functional>
#include <vector>

namespace barton
{
    /**
     * Lightweight registry for SBMD metric lifecycle callbacks.
     *
     * Each metrics class self-registers at static-initialization time via
     * registerProvider(); Matter.cpp calls initializeAll() / shutdownAll()
     * without knowing who's registered.
     */
    class MetricsRegistry
    {
    public:
        struct Provider
        {
            std::function<void()> initialize;
            std::function<void()> shutdown;
        };

        static MetricsRegistry &instance();

        /** Called from each metrics class's .cpp at file scope — before main(). */
        static void registerProvider(Provider p);

        /** Initialize all registered providers in registration order. */
        static void initializeAll();

        /** Shutdown all registered providers in reverse registration order. */
        static void shutdownAll();

    private:
        std::vector<Provider> providers;
    };

} // namespace barton

#else

namespace barton
{
    class MetricsRegistry
    {
    public:
        static void initializeAll() {}
        static void shutdownAll() {}
    };
} // namespace barton

#endif // BARTON_CONFIG_SBMD_METRICS
