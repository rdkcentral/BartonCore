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
