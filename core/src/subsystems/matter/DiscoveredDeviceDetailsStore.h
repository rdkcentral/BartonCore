//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2021 Comcast Corporation
//
// All rights reserved.
//
// This software is protected by copyright laws of the United States
// and of foreign countries. This material may also be protected by
// patent laws of the United States and of foreign countries.
//
// This software is furnished under a license agreement and/or a
// nondisclosure agreement and may only be used or copied in accordance
// with the terms of those agreements.
//
// The mere transfer of this software does not imply any licenses of trade
// secrets, proprietary technology, copyrights, patents, trademarks, or
// any other form of intellectual property whatsoever.
//
// Comcast Corporation retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------

/*
 * Created by Thomas Lea on 11/17/21.
 */

#ifndef ZILKER_DISCOVEREDDEVICEDETAILSSTORE_H
#define ZILKER_DISCOVEREDDEVICEDETAILSSTORE_H

#include "DiscoveredDeviceDetails.h"
#include <memory>
#include <mutex>

namespace zilker
{
    class DiscoveredDeviceDetailsStore
    {

    public:
        static DiscoveredDeviceDetailsStore &Instance()
        {
            static DiscoveredDeviceDetailsStore instance;
            return instance;
        }

        /**
         * Get details about a discovered device
         * @param deviceId
         * @return
         */
        std::shared_ptr<DiscoveredDeviceDetails> Get(std::string deviceId);

        /**
         * Save the provided discovered device details in non-volatile storage.  Cleans up any temporarily stored
         * entries.
         * @param deviceId
         * @param details
         */
        void Put(const std::string &deviceId, std::shared_ptr<DiscoveredDeviceDetails> details);

        /**
         * Save details about a discovered device temporarily (volatile storage)
         * @param deviceId
         * @param details
         */
        void PutTemporarily(std::string deviceId, std::shared_ptr<DiscoveredDeviceDetails> details);

    private:
        DiscoveredDeviceDetailsStore() = default;
        ~DiscoveredDeviceDetailsStore() = default;

        std::mutex temporaryDeviceDetailsMtx;
        std::map<std::string, std::shared_ptr<DiscoveredDeviceDetails>> temporaryDeviceDetails;
    };
} // namespace zilker

#endif // ZILKER_DISCOVEREDDEVICEDETAILSSTORE_H
