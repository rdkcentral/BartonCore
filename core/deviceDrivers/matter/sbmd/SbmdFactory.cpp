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
#define LOG_TAG     "SBMDFactory"
#define logFmt(fmt) "(%s): " fmt, __func__

#include "SbmdFactory.h"
#include "SbmdParser.h"
#include "SpecBasedMatterDeviceDriver.h"
#include "../MatterDriverFactory.h"

#include <filesystem>

extern "C" {
#include "deviceServiceConfiguration.h"
#include <icLog/logging.h>
}

using namespace barton;

bool SbmdFactory::RegisterDrivers()
{
    bool allRegistered = true;

    g_autofree gchar *sbmdDir = deviceServiceConfigurationGetSbmdDir();
    if (sbmdDir == nullptr || sbmdDir[0] == '\0')
    {
        icError("SBMD directory not configured. Set the SBMD directory using the '%s' property on the initialize params container.",
                B_CORE_INITIALIZE_PARAMS_CONTAINER_PROPERTY_NAMES[B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_SBMD_DIR]);
        return false;
    }

    std::error_code ec;
    bool exists = std::filesystem::exists(sbmdDir, ec);
    if (ec)
    {
        icError("Failed to check if SBMD directory exists %s: %s", sbmdDir, ec.message().c_str());
        return false;
    }
    if (!exists)
    {
        icWarn("SBMD specs directory does not exist: %s", sbmdDir);
        return false;
    }

    bool isDir = std::filesystem::is_directory(sbmdDir, ec);
    if (ec)
    {
        icError("Failed to check if SBMD path is a directory %s: %s", sbmdDir, ec.message().c_str());
        return false;
    }
    if (!isDir)
    {
        icWarn("SBMD specs path is not a directory: %s", sbmdDir);
        return false;
    }

    std::filesystem::directory_iterator dirIterator(sbmdDir, ec);
    if (ec)
    {
        icError("Failed to open SBMD directory %s: %s", sbmdDir, ec.message().c_str());
        return false;
    }

    try
    {
        for (const auto& entry : dirIterator)
        {
            if (entry.is_regular_file() && (entry.path().extension() == ".sbmd"))
            {
                try
                {
                    icDebug("Loading SBMD spec: %s", entry.path().c_str());

                    auto spec = SbmdParser::ParseFile(entry.path().string());
                    if (!spec)
                    {
                        icError("Failed to parse SBMD spec: %s", entry.path().c_str());
                        allRegistered = false;
                        continue;
                    }

                    auto driver = std::make_unique<SpecBasedMatterDeviceDriver>(spec);

                    if (!MatterDriverFactory::Instance().RegisterDriver(std::move(driver)))
                    {
                        icError("FATAL: Failed to register SBMD driver from: %s. "
                                "This is a fatal error. Matter subsystem will not be ready.", 
                                entry.path().c_str());
                        allRegistered = false;
                        continue;
                    }

                    icInfo("Successfully registered SBMD driver: %s", entry.path().filename().c_str());
                }
                catch (const std::exception& e)
                {
                    icError("Exception loading SBMD spec %s: %s", entry.path().c_str(), e.what());
                    allRegistered = false;
                }
            }
        }
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        icError("Filesystem error during SBMD directory iteration: %s", e.what());
        allRegistered = false;
    }

    return allRegistered;
}
