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

#include "MatterDriverFactory.h"
#include "matter/MatterDeviceDriver.h"

using namespace barton;

extern "C" {
#include <device-driver/device-driver-manager.h>
#include <icLog/logging.h>
}

bool MatterDriverFactory::RegisterDriver(std::unique_ptr<MatterDeviceDriver> driver)
{
    if (!driver || !driver->GetDriver() || !driver->GetDriver()->driverName)
    {
        return false;
    }

    MatterDeviceDriver *rawDriver = driver.get();
    // Store driver name as a string for safety. The driver name is validated
    // earlier in this function and should remain constant after registration.
    std::string driverName = rawDriver->GetDriver()->driverName;

    icDebug("%s", driverName.c_str());

    // Check for duplicate driver name before registration
    if (drivers.find(driverName) != drivers.end())
    {
        icError("FATAL: Driver with name '%s' is already registered. "
                "Duplicate driver names are not allowed. "
                "Check SBMD specs for conflicting 'name' fields.", driverName.c_str());
        return false;
    }

    bool result = deviceDriverManagerRegisterDriver(rawDriver->GetDriver());
    if (!result)
    {
        // Registration failed; do not insert into drivers and allow driver to be destroyed.
        icError("Failed to register driver '%s' with device driver manager", driverName.c_str());
        return false;
    }

    // Use the stored string as the map key to ensure consistency
    drivers.emplace(driverName, rawDriver);

    // Release ownership - deviceDriverManager takes ownership and will handle cleanup
    // via the destroy callback (which calls delete on the MatterDeviceDriver).
    // Do NOT store in ownedDrivers to avoid double-free during shutdown.
    [[maybe_unused]] auto *released = driver.release();

    return true;
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
