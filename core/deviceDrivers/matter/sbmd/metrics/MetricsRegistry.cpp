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

#include "MetricsRegistry.h"

#ifdef BARTON_CONFIG_SBMD_METRICS

namespace barton
{
    MetricsRegistry &MetricsRegistry::instance()
    {
        static MetricsRegistry reg;

        return reg;
    }

    void MetricsRegistry::registerProvider(Provider p)
    {
        instance().providers.push_back(std::move(p));
    }

    void MetricsRegistry::initializeAll()
    {
        for (auto &p : instance().providers)
        {
            p.initialize();
        }
    }

    void MetricsRegistry::shutdownAll()
    {
        auto &v = instance().providers;

        for (auto it = v.rbegin(); it != v.rend(); ++it)
        {
            it->shutdown();
        }
    }

} // namespace barton

#endif // BARTON_CONFIG_SBMD_METRICS
