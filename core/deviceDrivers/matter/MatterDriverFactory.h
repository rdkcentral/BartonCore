//------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2024 Comcast Cable Communications Management, LLC
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
// Created by tlea200 on 11/4/21.
//

#pragma once

#include "matter/MatterDeviceDriver.h"
#include "subsystems/matter/DeviceDataCache.h"
#include <map>
#include <memory>
#include <vector>

namespace barton
{
    class MatterDriverFactory
    {
    public:
        static MatterDriverFactory &Instance()
        {
            static MatterDriverFactory instance;
            return instance;
        }

        bool RegisterDriver(MatterDeviceDriver *driver);
        bool RegisterDriver(std::unique_ptr<MatterDeviceDriver> driver);
        MatterDeviceDriver *GetDriver(const DeviceDataCache *dataCache);

    private:
        MatterDriverFactory() = default;
        ~MatterDriverFactory() = default;
        std::map<const char *, MatterDeviceDriver *> drivers;
        std::vector<std::unique_ptr<MatterDeviceDriver>> ownedDrivers;
    };

} // namespace barton
