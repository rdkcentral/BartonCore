//
// Created by tlea200 on 11/4/21.
//

#define logFmt(fmt) "MatterDriverFactory (%s): " fmt, __func__
#include "subsystems/matter/MatterCommon.h"

#include "MatterDriverFactory.h"
#include "matter/MatterDeviceDriver.h"
#include <iostream>
#include <sstream>

using namespace zilker;

extern "C" {
#include <device-driver/device-driver-manager.h>
#include <icLog/logging.h>
}

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

MatterDeviceDriver *zilker::MatterDriverFactory::GetDriver(DiscoveredDeviceDetails *details)
{
    MatterDeviceDriver *result = nullptr;

    if (details != nullptr)
    {
        // iterate over the drivers and allow them to claim the device.  First one to claim it gets it.  Order
        // of drivers is deterministic since we store in std::map.
        for (auto const &entry : drivers)
        {
            if (entry.second->ClaimDevice(details))
            {
                icInfo("%s claimed the device", entry.first);
                result = entry.second;
                break;
            }
        }
    }

    return result;
}
