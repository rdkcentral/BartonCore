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

#include "SbmdDriver.h"

#include <memory>
#include <string>
#include <vector>

extern "C" {
#include "observability/observabilityMetrics.h"
}

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
         * Loads SBMD drivers (.sbmd.js) from configured directories.
         */
        bool RegisterDrivers();

        /**
         * Initialize all metric handles owned by SbmdFactory.
         */
        static void InitializeMetrics();

        /**
         * Release all metric handles owned by SbmdFactory.
         */
        static void ShutdownMetrics();

    private:
        SbmdFactory() = default;
        ~SbmdFactory() = default;

        /**
         * Load and register SBMD drivers (.sbmd.js) from a single directory.
         * Drivers are activated immediately and stored in drivers for lifetime management.
         */
        void RegisterDriversFromDirectory(const std::string &dirPath, bool &allRegistered);

        /**
         * Owned driver instances. These must outlive the SpecBasedMatterDeviceDriver
         * instances that reference them (those are owned by the C device manager).
         */
        std::vector<std::unique_ptr<SbmdDriver>> drivers;

        /**
         * Whether the mquickjs runtime, utilities bundle, and capture function
         * have been initialized for driver loading.
         */
        bool runtimeReady = false;

        // Observability metric handles
        static ObservabilityCounter *driverLoadFailureCounter;
        static ObservabilityHistogram *driverLoadDurationHisto;
        static ObservabilityHistogram *driverLoadHeapDeltaHisto;
        static ObservabilityGauge *registeredDriversGauge;
    };
} //namespace barton
