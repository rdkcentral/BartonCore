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

#include "SbmdV4Driver.h"

#include <memory>
#include <string>
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

        /**
         * Register SBMD drivers from all configured directories.
         * Directories are specified as a semicolon-delimited list.
         * Loads both v3 (.sbmd) and v4 (.sbmd.js) drivers.
         */
        bool RegisterDrivers();

    private:
        SbmdFactory() = default;
        ~SbmdFactory() = default;

        /**
         * Load and register v3 SBMD drivers (.sbmd) from a single directory.
         */
        static void RegisterV3DriversFromDirectory(const std::string &dirPath, bool &allRegistered);

        /**
         * Load and register v4 SBMD drivers (.sbmd.js) from a single directory.
         * V4 drivers are activated immediately and stored in v4Drivers for lifetime management.
         */
        void RegisterV4DriversFromDirectory(const std::string &dirPath, bool &allRegistered);

        /**
         * Owned v4 driver instances. These must outlive the SpecBasedMatterDeviceDriver
         * instances that reference them (those are owned by the C device manager).
         */
        std::vector<std::unique_ptr<SbmdV4Driver>> v4Drivers;
    };
} //namespace barton
