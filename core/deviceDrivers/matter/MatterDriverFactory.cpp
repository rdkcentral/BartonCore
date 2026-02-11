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

#include <memory>
#define logFmt(fmt) "MatterDriverFactory (%s): " fmt, __func__
#include "subsystems/matter/MatterCommon.h"

#include "MatterDriverFactory.h"
#include "matter/MatterDeviceDriver.h"
#include <iostream>
#include <sstream>

using namespace barton;

extern "C" {
#include <device-driver/device-driver-manager.h>
#include <icLog/logging.h>
}

// TODO: Remove when native drivers are removed in support of SBMD drivers only
bool MatterDriverFactory::RegisterDriver(MatterDeviceDriver *driver)
{
    bool result = false;
    if (driver != nullptr && driver->GetDriver() != nullptr && driver->GetDriver()->driverName != nullptr)
    {
        icDebug("%s", driver->GetDriver()->driverName);
        drivers.emplace(driver->GetDriver()->driverName, driver);

        result = deviceDriverManagerRegisterDriver(driver->GetDriver());
    }

    return result;
}

bool MatterDriverFactory::RegisterDriver(std::unique_ptr<MatterDeviceDriver> driver)
{
    if (!driver)
    {
        return false;
    }

    MatterDeviceDriver *rawDriver = driver.get();
    bool result = RegisterDriver(rawDriver);
    
    if (result)
    {
        // Take ownership of the driver
        ownedDrivers.push_back(std::move(driver));
    }
    
    return result;
}

MatterDeviceDriver *barton::MatterDriverFactory::GetDriver(const DeviceDataCache *dataCache)
{
    MatterDeviceDriver *result = nullptr;

    // iterate over the drivers and allow them to claim the device.  First one to claim it gets it.  Order
    // of drivers is deterministic since we store in std::map.
    for (auto const &entry : drivers)
    {
        if (entry.second->ClaimDevice(dataCache))
        {
            icInfo("%s claimed the device", entry.first.c_str());
            result = entry.second;
            break;
        }
    }

    return result;
}
