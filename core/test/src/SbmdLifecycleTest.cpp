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

#include "MatterDeviceTestHelpers.h"
#include "deviceDrivers/matter/sbmd/SbmdDriver.h"
#include "deviceDrivers/matter/sbmd/SpecBasedMatterDeviceDriver.h"
#include "deviceDrivers/matter/sbmd/mquickjs/MQuickJsRuntime.h"
#include "deviceDrivers/matter/sbmd/mquickjs/SbmdBundleLoader.h"
#include "deviceDrivers/matter/sbmd/mquickjs/SbmdLoader.h"
#include <cstring>
#include <memory>

using namespace barton;

namespace barton
{
    // Friended (see SpecBasedMatterDeviceDriver.h) so the lifecycle tests can drive the private
    // bind-lifecycle guard directly: the in-flight counter, the shared idle-teardown primitive, and
    // the protected last-device hook.
    class TestableSpecBasedMatterDeviceDriver : public SpecBasedMatterDeviceDriver
    {
    public:
        using SpecBasedMatterDeviceDriver::SpecBasedMatterDeviceDriver;

        // Activate the underlying driver and mirror the active-driver bookkeeping AddDevice performs,
        // so a later DeactivateIfIdle keeps the gauge balanced.
        bool ActivateForTest()
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());

            if (!driver->Activate(MQuickJsRuntime::Instance().GetSharedContext()))
            {
                return false;
            }

            activeDriverCount.fetch_add(1);
            return true;
        }

        void SetBindsInProgress(int n) { bindsInProgress.store(n); }

        void CallOnLastDeviceRemoved() { OnLastDeviceRemoved(); }

        void CallDeactivateIfIdle()
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            DeactivateIfIdle();
        }
    };
} // namespace barton

namespace
{
    ::testing::Environment *const chipEnv = ::testing::AddGlobalTestEnvironment(new ChipPlatformEnvironment);

    // ========================================================================
    // In-flight-bind lifecycle guard tests
    //
    // AddDevice brackets itself with the InFlightBind RAII guard (increment bindsInProgress on
    // entry; decrement and DeactivateIfIdle on exit). These tests pin that guard's contract
    // directly and deterministically via the friended Testable subclass — no threads, no barrier,
    // no production test seam: a bind in flight defers last-device teardown, the deferred teardown
    // is retried once idle, and a bound device suppresses teardown entirely.
    // ========================================================================

    const char *kGuardDriverSource = R"(
        SbmdDriver({
            schemaVersion: "4.0",
            driverVersion: 1,
            name: "GuardTestDriver",
            constants: { CL_ON_OFF: 6, ATTR_ON_OFF: 0 },
            barton: { deviceClass: "guardLight", deviceClassVersion: 1 },
            matter: { deviceTypes: [0x0100] },
            aliases: { onOff: { clusterId: CL_ON_OFF, attributeId: ATTR_ON_OFF, type: "bool" } },
            endpoints: {
                "1": {
                    profile: "guardLight",
                    profileVersion: 1,
                    resources: { isOn: { type: "boolean", modes: ["read"], read: readIsOn } },
                },
            },
        });

        function readIsOn(args) { return Sbmd.result().success(); }
    )";

    class SbmdLifecycleGuardTest : public ::testing::Test
    {
    protected:
        static void SetUpTestSuite()
        {
            ASSERT_TRUE(MQuickJsRuntime::Instance().Initialize(512 * 1024));
            auto *ctx = MQuickJsRuntime::Instance().GetSharedContext();
            ASSERT_NE(ctx, nullptr);
            ASSERT_TRUE(SbmdBundleLoader::LoadBundle(ctx));
            ASSERT_TRUE(SbmdLoader::InjectCaptureFunction(ctx));
        }

        static void TearDownTestSuite() { MQuickJsRuntime::Instance().Shutdown(); }

        std::unique_ptr<SbmdDriver> CreateGuardDriver()
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            auto reg = SbmdLoader::LoadDriver(MQuickJsRuntime::Instance().GetSharedContext(),
                                              "<guard-test>",
                                              kGuardDriverSource,
                                              strlen(kGuardDriverSource));

            if (!reg)
            {
                return nullptr;
            }

            return std::make_unique<SbmdDriver>(std::move(reg), kGuardDriverSource);
        }
    };

    TEST_F(SbmdLifecycleGuardTest, TeardownDeferredWhileBindInFlightThenRetriedWhenIdle)
    {
        auto sbmd = CreateGuardDriver();
        ASSERT_NE(sbmd, nullptr);

        TestableSpecBasedMatterDeviceDriver driver(sbmd.get());
        ASSERT_TRUE(driver.ActivateForTest());
        ASSERT_TRUE(sbmd->IsActivated());

        // A bind is in flight (activated, device not yet inserted): a racing last-device removal
        // must not shed the driver's state out from under it.
        driver.SetBindsInProgress(1);
        driver.CallOnLastDeviceRemoved();
        EXPECT_TRUE(sbmd->IsActivated()) << "teardown must be deferred while a bind is in flight";

        // The bind finishes without leaving a device bound: the deferred teardown is retried as the
        // in-flight count returns to zero, so the driver does not stay active forever.
        driver.SetBindsInProgress(0);
        driver.CallDeactivateIfIdle();
        EXPECT_FALSE(sbmd->IsActivated()) << "deferred teardown must be retried once the driver is idle";
    }

    TEST_F(SbmdLifecycleGuardTest, DeactivateIfIdleIsNoOpWhileDeviceBound)
    {
        auto sbmd = CreateGuardDriver();
        ASSERT_NE(sbmd, nullptr);

        TestableSpecBasedMatterDeviceDriver driver(sbmd.get());

        // Bind a resolvable device through the real AddDevice path, then confirm an idle-teardown
        // attempt is a no-op because a device is still bound.
        auto cache = std::make_shared<DeviceDataCache>("idle-dev", nullptr);
        TestableMatterDevice::PopulateTestCache(cache,
                                                {
                                                    1
        },
                                                {{1, {0x0100}}});
        auto device = std::make_unique<TestableMatterDevice>("idle-dev", cache);

        ASSERT_TRUE(driver.AddDevice(std::move(device)));
        ASSERT_TRUE(sbmd->IsActivated());

        driver.CallDeactivateIfIdle();
        EXPECT_TRUE(sbmd->IsActivated()) << "a bound device must keep the driver active";
    }

    TEST_F(SbmdLifecycleGuardTest, FailedBindDeactivatesOnExit)
    {
        auto sbmd = CreateGuardDriver();
        ASSERT_NE(sbmd, nullptr);

        TestableSpecBasedMatterDeviceDriver driver(sbmd.get());

        // The driver is freshly constructed and still holds its source, so AddDevice's activation
        // re-evaluates that retained source and succeeds. Do NOT deactivate it first: that would
        // free the source (ReleaseHeavyRegistration), making activation re-read the pseudo-path and
        // fail with read_failed, which bypasses the rollback this test covers (that distinct path
        // is exercised by FailedActivationAbortsBind below). The unpopulated cache then makes
        // ResolveEndpointMap fail after activation, so the bind fails on the post-activation path and
        // the real InFlightBind RAII destructor must roll the activation back.
        auto cache = std::make_shared<DeviceDataCache>("bind-fail", nullptr);
        auto device = std::make_unique<TestableMatterDevice>("bind-fail", cache);

        EXPECT_FALSE(driver.AddDevice(std::move(device)));
        EXPECT_FALSE(sbmd->IsActivated()) << "a failed bind must deactivate the driver it activated";
    }

    TEST_F(SbmdLifecycleGuardTest, FailedActivationAbortsBind)
    {
        auto sbmd = CreateGuardDriver();
        ASSERT_NE(sbmd, nullptr);

        TestableSpecBasedMatterDeviceDriver driver(sbmd.get());

        // Reduce the driver to its claim metadata so it releases its retained source. Its file path
        // is a pseudo-path, so the on-demand re-read inside AddDevice's activation then fails,
        // deterministically exercising the read_failed abort before any device is bound (the
        // complement of FailedBindDeactivatesOnExit's post-activation rollback).
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            sbmd->ReduceToClaimMetadata();
        }
        ASSERT_FALSE(sbmd->IsActivated());

        auto cache = std::make_shared<DeviceDataCache>("activate-fail", nullptr);
        auto device = std::make_unique<TestableMatterDevice>("activate-fail", cache);

        EXPECT_FALSE(driver.AddDevice(std::move(device)));
        EXPECT_FALSE(sbmd->IsActivated()) << "a failed activation must leave the driver inactive and bind nothing";
    }

    TEST_F(SbmdLifecycleGuardTest, BindAcceptsEndpointsSharingAProfileAtDifferentVersions)
    {
        // Construction caches one version per profile (the first endpoint's). The activation-time
        // version check must compare exactly that, or a spec that registered fine could never bind.
        const char *source = R"(
            SbmdDriver({
                schemaVersion: "4.0",
                driverVersion: 1,
                name: "SharedProfileDriver",
                constants: {},
                barton: { deviceClass: "guardLight", deviceClassVersion: 1 },
                matter: { deviceTypes: [0x0100] },
                endpoints: {
                    "1": { profile: "guardLight", profileVersion: 1, resources: {} },
                    "2": { profile: "guardLight", profileVersion: 2, resources: {} },
                },
            });
        )";

        std::unique_ptr<SbmdDriver> sbmd;
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            auto reg = SbmdLoader::LoadDriver(
                MQuickJsRuntime::Instance().GetSharedContext(), "<shared-profile>", source, strlen(source));
            ASSERT_NE(reg, nullptr);
            sbmd = std::make_unique<SbmdDriver>(std::move(reg), source);
        }

        TestableSpecBasedMatterDeviceDriver driver(sbmd.get());

        auto cache = std::make_shared<DeviceDataCache>("shared-profile-dev", nullptr);
        TestableMatterDevice::PopulateTestCache(cache,
                                                {
                                                    1
        },
                                                {{1, {0x0100}}});
        auto device = std::make_unique<TestableMatterDevice>("shared-profile-dev", cache);

        EXPECT_TRUE(driver.AddDevice(std::move(device)));
        EXPECT_TRUE(sbmd->IsActivated());
    }
} // namespace
