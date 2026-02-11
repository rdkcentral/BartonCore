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
 * Created by Thomas Lea on 10/17/2025
 */

#pragma once

#include "../MatterDeviceDriver.h"
#include <memory>
#include <vector>

namespace barton
{
    class SbmdFactory
    {
    public:
        static SbmdFactory &Instance()
        {
            static SbmdFactory instance;
            return instance;
        }

        bool RegisterDrivers();

    private:
        SbmdFactory() = default;
        ~SbmdFactory();

        // Stores drivers created by this factory to ensure proper cleanup.
        // MatterDriverFactory holds raw pointers to these drivers. Since both are
        // singletons with static storage duration, and MatterDriverFactory is
        // initialized first, it will be destroyed last, ensuring the raw pointers
        // remain valid.
        std::vector<std::unique_ptr<MatterDeviceDriver>> drivers;
    };
} //namespace barton
