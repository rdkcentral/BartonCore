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
#include "SpecBasedMatterDeviceDriver.h"
#include "../MatterDriverFactory.h"

#include "mquickjs/MQuickJsRuntime.h"
#include "mquickjs/SbmdBundleLoader.h"
#include "mquickjs/SbmdHandlerInvoker.h"
#include "mquickjs/SbmdLoader.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

extern "C" {
#include "deviceServiceConfiguration.h"
#include <icLog/logging.h>
}

using namespace barton;

// Observability metric handles
ObservabilityCounter *SbmdFactory::driverLoadFailureCounter = nullptr;
ObservabilityHistogram *SbmdFactory::driverLoadDurationHisto = nullptr;
ObservabilityHistogram *SbmdFactory::driverLoadHeapDeltaHisto = nullptr;
ObservabilityGauge *SbmdFactory::registeredDriversGauge = nullptr;

bool SbmdFactory::RegisterDrivers()
{
    bool allRegistered = true;

    g_autofree gchar *sbmdDirs = deviceServiceConfigurationGetSbmdDirs();
    if (sbmdDirs == nullptr || sbmdDirs[0] == '\0')
    {
#ifdef BARTON_CONFIG_MATTER_SBMD_SPECS_DIR
        icWarn("SBMD directories not configured via '%s' property. Falling back to default: %s",
               B_CORE_INITIALIZE_PARAMS_CONTAINER_PROPERTY_NAMES[B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_SBMD_DIRS],
               BARTON_CONFIG_MATTER_SBMD_SPECS_DIR);

        // Transfer ownership to sbmdDirs so g_autofree handles cleanup
        g_clear_pointer(&sbmdDirs, g_free);
        sbmdDirs = g_strdup(BARTON_CONFIG_MATTER_SBMD_SPECS_DIR);
#else
        icError("SBMD directories not configured. Set the SBMD directories using the '%s' property on the initialize "
                "params container.",
                B_CORE_INITIALIZE_PARAMS_CONTAINER_PROPERTY_NAMES[B_CORE_INITIALIZE_PARAMS_CONTAINER_PROP_SBMD_DIRS]);
        return false;
#endif
    }

    // Split the semicolon-delimited list of directories
    std::istringstream stream(sbmdDirs);
    std::string dirPath;

    while (std::getline(stream, dirPath, ';'))
    {
        // Skip empty entries (e.g., from trailing semicolons)
        if (dirPath.empty())
        {
            continue;
        }

        RegisterDriversFromDirectory(dirPath, allRegistered);
    }

    return allRegistered;
}

void SbmdFactory::RegisterDriversFromDirectory(const std::string &dirPath, bool &allRegistered)
{
    std::error_code ec;

    bool exists = std::filesystem::exists(dirPath, ec);

    if (ec)
    {
        icError("Failed to check if SBMD directory exists %s: %s", dirPath.c_str(), ec.message().c_str());
        allRegistered = false;
        return;
    }

    if (!exists)
    {
        icWarn("SBMD specs directory does not exist: %s", dirPath.c_str());
        allRegistered = false;
        return;
    }

    if (!std::filesystem::is_directory(dirPath, ec) || ec)
    {
        return;
    }

    std::filesystem::directory_iterator dirIterator(dirPath, ec);

    if (ec)
    {
        return;
    }

    // Ensure the shared JS runtime is initialized before loading any drivers.
    // SbmdScriptImpl::Create lazily initializes, but drivers
    // need it at factory registration time.
    // Note: do NOT hold the JS mutex across these calls — LoadBundle and
    // InjectCaptureFunction may acquire it internally.
    if (!runtimeReady)
    {
        MQuickJsRuntime::InitializeMetrics();

        if (!MQuickJsRuntime::IsInitialized())
        {
            if (!MQuickJsRuntime::Initialize(BARTON_CONFIG_MQUICKJS_MEMSIZE_BYTES))
            {
                icError("Failed to initialize mquickjs runtime for drivers");
                allRegistered = false;
                return;
            }
        }

        auto *ctx = MQuickJsRuntime::GetSharedContext();

        if (!SbmdBundleLoader::LoadBundle(ctx))
        {
            icError("Failed to load SBMD bundles for drivers");
            allRegistered = false;
            return;
        }

        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());

            if (!SbmdLoader::InjectCaptureFunction(ctx))
            {
                icError("Failed to inject SbmdDriver capture function");
                allRegistered = false;
                return;
            }
        }

        runtimeReady = true;
        SbmdHandlerInvoker::InitializeMetrics();
        SbmdFactory::InitializeMetrics();
        SpecBasedMatterDeviceDriver::InitializeMetrics();
        icInfo("mquickjs runtime initialized for SBMD drivers");
    }

    try
    {
        for (const auto &entry : dirIterator)
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".js")
            {
                continue;
            }

            // Check for .sbmd.js double extension
            auto stem = entry.path().stem(); // e.g. "light.sbmd"

            if (stem.extension() != ".sbmd")
            {
                continue;
            }

            try
            {
                icDebug("Loading SBMD driver: %s", entry.path().c_str());

                // Read file contents
                std::ifstream file(entry.path(), std::ios::binary | std::ios::ate);

                if (!file.is_open())
                {
                    icError("Failed to open SBMD driver: %s", entry.path().c_str());
                    observabilityCounterAddWithAttrs(
                        driverLoadFailureCounter, 1, "driver", stem.string().c_str(), "reason", "file_read", nullptr);
                    allRegistered = false;
                    continue;
                }

                auto fileSize = file.tellg();
                file.seekg(0, std::ios::beg);
                std::string source(static_cast<size_t>(fileSize), '\0');
                file.read(source.data(), fileSize);

                if (!file)
                {
                    icError("Failed to read SBMD driver: %s", entry.path().c_str());
                    observabilityCounterAddWithAttrs(
                        driverLoadFailureCounter, 1, "driver", stem.string().c_str(), "reason", "file_read", nullptr);
                    allRegistered = false;
                    continue;
                }

                // Load the driver registration under the JS mutex
                auto loadStart = std::chrono::steady_clock::now();
                std::unique_ptr<SbmdRegistration> registration;
                JSMemoryUsage usageBefore = {};
                {
                    std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
                    auto *ctx = MQuickJsRuntime::GetSharedContext();
                    JS_GetMemoryUsage(ctx, &usageBefore, 0);
                    registration =
                        SbmdLoader::LoadDriver(ctx, entry.path().string(), source.c_str(), source.size());
                }

                if (!registration)
                {
                    icError("Failed to load SBMD driver: %s", entry.path().c_str());
                    observabilityCounterAddWithAttrs(
                        driverLoadFailureCounter, 1, "driver", stem.string().c_str(), "reason", "eval_failed", nullptr);
                    allRegistered = false;
                    continue;
                }

                // Create the driver and activate it
                auto sbmdDriver = std::make_unique<SbmdDriver>(std::move(registration), std::move(source));
                JSMemoryUsage usageAfter = {};

                {
                    std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
                    auto *ctx = MQuickJsRuntime::GetSharedContext();

                    if (!sbmdDriver->Activate(ctx))
                    {
                        icError("Failed to activate SBMD driver: %s", entry.path().c_str());
                        observabilityCounterAddWithAttrs(driverLoadFailureCounter,
                                                         1,
                                                         "driver",
                                                         stem.string().c_str(),
                                                         "reason",
                                                         "activation_failed",
                                                         nullptr);
                        allRegistered = false;
                        continue;
                    }

                    JS_GetMemoryUsage(ctx, &usageAfter, 0);
                }

                // Create the SpecBasedMatterDeviceDriver wrapper
                auto driver = std::make_unique<SpecBasedMatterDeviceDriver>(sbmdDriver.get());

                if (!MatterDriverFactory::Instance().RegisterDriver(std::move(driver)))
                {
                    icError("FATAL: Failed to register SBMD driver from: %s", entry.path().c_str());
                    allRegistered = false;
                    continue;
                }

                // Store the driver for lifetime management
                drivers.push_back(std::move(sbmdDriver));

                double loadDurationMs =
                    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - loadStart).count();
                observabilityHistogramRecordWithAttrs(
                    driverLoadDurationHisto, loadDurationMs, "driver", stem.string().c_str(), nullptr);
                double heapDelta =
                    static_cast<double>(usageAfter.heap_used) - static_cast<double>(usageBefore.heap_used);
                observabilityHistogramRecordWithAttrs(
                    driverLoadHeapDeltaHisto, heapDelta, "driver", stem.string().c_str(), nullptr);

                icInfo("Successfully registered SBMD driver: %s", entry.path().filename().c_str());
            }
            catch (const std::exception &e)
            {
                icError("Exception loading SBMD driver %s: %s", entry.path().c_str(), e.what());
                allRegistered = false;
            }
        }
    }
    catch (const std::filesystem::filesystem_error &e)
    {
        icError("Filesystem error during SBMD directory iteration: %s", e.what());
        allRegistered = false;
    }

    observabilityGaugeRecord(registeredDriversGauge, static_cast<int64_t>(drivers.size()));
}

void SbmdFactory::InitializeMetrics()
{
    driverLoadFailureCounter =
        observabilityCounterCreate("sbmd.driver.load.failure", "Number of SBMD driver load failures", "{failure}");
    driverLoadDurationHisto = observabilityHistogramCreate(
        "sbmd.driver.load.duration_ms", "Time to load and activate each SBMD driver", "ms");
    driverLoadHeapDeltaHisto = observabilityHistogramCreate(
        "sbmd.driver.load.heap_bytes",
        "Change in heap_used from before LoadDriver to after Activate for each SBMD driver",
        "By");
    registeredDriversGauge = observabilityGaugeCreate(
        "sbmd.driver.registered.count", "Number of SBMD drivers successfully registered", "{driver}");
}

void SbmdFactory::ShutdownMetrics()
{
    observabilityCounterRelease(driverLoadFailureCounter);
    driverLoadFailureCounter = nullptr;
    observabilityHistogramRelease(driverLoadDurationHisto);
    driverLoadDurationHisto = nullptr;
    observabilityHistogramRelease(driverLoadHeapDeltaHisto);
    driverLoadHeapDeltaHisto = nullptr;
    observabilityGaugeRelease(registeredDriversGauge);
    registeredDriversGauge = nullptr;
    runtimeReady = false;
}
