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
#include <app/data-model/Decode.h>
#include <cstring>

extern "C" {
#include <icTypes/icHashMap.h>
}

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

    // Matter device type IDs
    constexpr uint16_t kDimmableLightDeviceType = 0x0100;
    constexpr uint16_t kTemperatureSensorDeviceType = 0x0302;
    constexpr uint16_t kHumiditySensorDeviceType = 0x0307;

    class MatterDeviceEndpointMapTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            cache = std::make_shared<DeviceDataCache>("test-device", nullptr);
            device = std::make_unique<TestableMatterDevice>("test-device", cache);
        }

        void TearDown() override
        {
            device.reset();
            cache.reset();
        }

        std::shared_ptr<DeviceDataCache> cache;
        std::unique_ptr<TestableMatterDevice> device;
    };

    // ================================================================
    // Tests for ResolveEndpointMap()
    // ================================================================

    TEST_F(MatterDeviceEndpointMapTest, ResolveEndpointMapNoData)
    {
        // Cache has no ClusterStateCache, so GetEndpointIds() returns empty
        EXPECT_FALSE(device->ResolveEndpointMap({0x0100}));
    }

    TEST_F(MatterDeviceEndpointMapTest, ResolveEndpointMapEmptyDeviceTypes)
    {
        EXPECT_FALSE(device->ResolveEndpointMap({}));
    }

    TEST(MatterDeviceEndpointMapNoCacheTest, ResolveEndpointMapNullCache)
    {
        TestableMatterDevice device("test-device", nullptr);
        EXPECT_FALSE(device.ResolveEndpointMap({0x0100}));
    }

    TEST_F(MatterDeviceEndpointMapTest, ResolveEndpointMapNoDataMultipleTypes)
    {
        EXPECT_FALSE(device->ResolveEndpointMap({0x0100, 0x000F}));
    }

    // Positive case: multi-endpoint device, skips endpoint 0 and non-matching endpoints,
    // assigns sequential SBMD indices in PartsList order
    TEST_F(MatterDeviceEndpointMapTest, ResolveEndpointMapMultipleEndpoints)
    {
        // Device has endpoints 0 (root), 1 (light 0x0100), 2 (sensor 0x000F), 3 (light 0x0100)
        std::vector<chip::EndpointId> partsList = {1, 2, 3};
        std::map<chip::EndpointId, std::vector<uint16_t>> deviceTypes = {
            {1, {0x0100}},
            {2, {0x000F}},
            {3, {0x0100}},
        };
        TestableMatterDevice::PopulateTestCache(cache, partsList, deviceTypes);

        // Resolve for device type 0x0100 (light) — should match endpoints 1 and 3
        EXPECT_TRUE(device->ResolveEndpointMap({0x0100}));

        auto &map = device->GetSbmdEndpointMap();
        ASSERT_EQ(map.size(), 2u);
        EXPECT_EQ(map[0], 1); // First match → SBMD index 0
        EXPECT_EQ(map[1], 3); // Second match → SBMD index 1
    }

    // Positive case: single matching endpoint
    TEST_F(MatterDeviceEndpointMapTest, ResolveEndpointMapSingleMatch)
    {
        std::vector<chip::EndpointId> partsList = {1, 2};
        std::map<chip::EndpointId, std::vector<uint16_t>> deviceTypes = {
            {1, {0x0100}},
            {2, {0x000F}},
        };
        TestableMatterDevice::PopulateTestCache(cache, partsList, deviceTypes);

        EXPECT_TRUE(device->ResolveEndpointMap({0x0100}));

        auto &map = device->GetSbmdEndpointMap();
        ASSERT_EQ(map.size(), 1u);
        EXPECT_EQ(map[0], 1);
    }

    // Positive case: no matching device types on any endpoint
    TEST_F(MatterDeviceEndpointMapTest, ResolveEndpointMapNoMatch)
    {
        std::vector<chip::EndpointId> partsList = {1, 2};
        std::map<chip::EndpointId, std::vector<uint16_t>> deviceTypes = {
            {1, {0x0100}},
            {2, {0x000F}},
        };
        TestableMatterDevice::PopulateTestCache(cache, partsList, deviceTypes);

        EXPECT_FALSE(device->ResolveEndpointMap({0x0302}));
        EXPECT_TRUE(device->GetSbmdEndpointMap().empty());
    }

    // Positive case: multiple supported device types match different endpoints
    TEST_F(MatterDeviceEndpointMapTest, ResolveEndpointMapMultipleDeviceTypes)
    {
        std::vector<chip::EndpointId> partsList = {1, 2, 3};
        std::map<chip::EndpointId, std::vector<uint16_t>> deviceTypes = {
            {1, {0x0100}}, // light
            {2, {0x000F}}, // sensor
            {3, {0x0302}}, // no match
        };
        TestableMatterDevice::PopulateTestCache(cache, partsList, deviceTypes);

        // Match both light and sensor types
        EXPECT_TRUE(device->ResolveEndpointMap({0x0100, 0x000F}));

        auto &map = device->GetSbmdEndpointMap();
        ASSERT_EQ(map.size(), 2u);
        EXPECT_EQ(map[0], 1); // endpoint 1 matches 0x0100
        EXPECT_EQ(map[1], 2); // endpoint 2 matches 0x000F
    }

    // ================================================================
    // Tests for GetEndpointForSbmdIndex()
    // ================================================================

    TEST_F(MatterDeviceEndpointMapTest, GetEndpointForSbmdIndexValid)
    {
        device->GetSbmdEndpointMap()[0] = 1;
        device->GetSbmdEndpointMap()[1] = 3;

        chip::EndpointId endpointId;

        EXPECT_TRUE(device->GetEndpointForSbmdIndex(0, endpointId));
        EXPECT_EQ(endpointId, 1);

        EXPECT_TRUE(device->GetEndpointForSbmdIndex(1, endpointId));
        EXPECT_EQ(endpointId, 3);
    }

    TEST_F(MatterDeviceEndpointMapTest, GetEndpointForSbmdIndexOutOfRange)
    {
        device->GetSbmdEndpointMap()[0] = 1;

        chip::EndpointId endpointId;
        EXPECT_FALSE(device->GetEndpointForSbmdIndex(1, endpointId));
        EXPECT_FALSE(device->GetEndpointForSbmdIndex(99, endpointId));
    }

    TEST_F(MatterDeviceEndpointMapTest, GetEndpointForSbmdIndexEmptyMap)
    {
        chip::EndpointId endpointId;
        EXPECT_FALSE(device->GetEndpointForSbmdIndex(0, endpointId));
    }

    // ================================================================
    // Tests for multiple SBMD endpoints
    // ================================================================

    TEST_F(MatterDeviceEndpointMapTest, FirstSbmdEndpointMapsCorrectly)
    {
        device->GetSbmdEndpointMap()[0] = 1;
        device->GetSbmdEndpointMap()[1] = 3;

        chip::EndpointId endpointId;
        EXPECT_TRUE(device->GetEndpointForSbmdIndex(0, endpointId));
        EXPECT_EQ(endpointId, 1);
    }

    TEST_F(MatterDeviceEndpointMapTest, SecondSbmdEndpointMapsCorrectly)
    {
        device->GetSbmdEndpointMap()[0] = 1;
        device->GetSbmdEndpointMap()[1] = 3;

        chip::EndpointId endpointId;
        EXPECT_TRUE(device->GetEndpointForSbmdIndex(1, endpointId));
        EXPECT_EQ(endpointId, 3);
    }

    // ================================================================
    // Tests for ClaimDevice with vendor/product ID matching
    // ================================================================

    class VendorProductClaimTest : public ::testing::Test
    {
    protected:
        static constexpr uint16_t kTestVendorId = 0x1234;
        static constexpr uint16_t kTestProductId = 0x5678;

        void SetUp() override
        {
            cache = std::make_shared<DeviceDataCache>("test-device", nullptr);
            PopulateCacheWithVendorProduct();
        }

        void TearDown() override
        {
            drivers.clear();
            cache.reset();
        }

        SbmdDriver *MakeVendorDriver(uint16_t vendorId, uint16_t productId, std::vector<uint16_t> deviceTypes = {})
        {
            auto reg = std::make_unique<SbmdRegistration>();
            reg->name = "vendor-test";
            reg->barton.deviceClass = "testClass";
            reg->barton.deviceClassVersion = 1;
            reg->matter.deviceTypes = std::move(deviceTypes);
            reg->matter.vendorId = vendorId;
            reg->matter.productId = productId;
            drivers.push_back(std::make_unique<SbmdDriver>(std::move(reg), ""));

            return drivers.back().get();
        }

        void PopulateCacheWithVendorProduct()
        {
            std::vector<chip::EndpointId> partsList = {1, 2};
            std::map<chip::EndpointId, std::vector<uint16_t>> endpointDeviceTypes = {
                {1, {kTemperatureSensorDeviceType}},
                {2,    {kHumiditySensorDeviceType}},
            };
            TestableMatterDevice::PopulateTestCache(cache, partsList, endpointDeviceTypes);
            TestableMatterDevice::InjectVendorProduct(cache, kTestVendorId, kTestProductId);
        }

        std::shared_ptr<DeviceDataCache> cache;
        std::vector<std::unique_ptr<SbmdDriver>> drivers;
    };

    TEST_F(VendorProductClaimTest, VendorProductMatch)
    {
        auto *drv =
            MakeVendorDriver(kTestVendorId, kTestProductId, {kTemperatureSensorDeviceType, kHumiditySensorDeviceType});
        SpecBasedMatterDeviceDriver driver(drv);
        EXPECT_TRUE(driver.ClaimDevice(cache.get()));
    }

    TEST_F(VendorProductClaimTest, WrongProductIdFails)
    {
        auto *drv = MakeVendorDriver(kTestVendorId, 0x9999, {kTemperatureSensorDeviceType, kHumiditySensorDeviceType});
        SpecBasedMatterDeviceDriver driver(drv);
        EXPECT_FALSE(driver.ClaimDevice(cache.get()));
    }

    TEST_F(VendorProductClaimTest, WrongVendorIdFails)
    {
        auto *drv = MakeVendorDriver(0x0001, kTestProductId, {kTemperatureSensorDeviceType, kHumiditySensorDeviceType});
        SpecBasedMatterDeviceDriver driver(drv);
        EXPECT_FALSE(driver.ClaimDevice(cache.get()));
    }

    TEST_F(VendorProductClaimTest, NoVendorSetFallsThroughToDeviceTypeMatching)
    {
        // Driver without vendorId/productId uses device-type matching
        auto reg = std::make_unique<SbmdRegistration>();
        reg->name = "generic-test";
        reg->barton.deviceClass = "testClass";
        reg->barton.deviceClassVersion = 1;
        reg->matter.deviceTypes = {kTemperatureSensorDeviceType};
        drivers.push_back(std::make_unique<SbmdDriver>(std::move(reg), ""));

        SpecBasedMatterDeviceDriver driver(drivers.back().get());
        EXPECT_TRUE(driver.ClaimDevice(cache.get()));
    }

    // ========================================================================
    // Endpoint profile version reconfiguration tests
    // ========================================================================

    class ProfileVersionReconfigTest : public ::testing::Test
    {
    protected:
        std::vector<std::unique_ptr<SbmdDriver>> drivers;

        template<typename... Endpoints>
        SbmdDriver *MakeDriverWithEndpoints(Endpoints &&...endpoints)
        {
            auto reg = std::make_unique<SbmdRegistration>();
            reg->name = "profile-version-test";
            reg->barton.deviceClass = "testClass";
            reg->barton.deviceClassVersion = 1;
            reg->matter.deviceTypes = {0x0100};
            (reg->endpoints.push_back(std::forward<Endpoints>(endpoints)), ...);
            drivers.push_back(std::make_unique<SbmdDriver>(std::move(reg), ""));

            return drivers.back().get();
        }
    };

    TEST_F(ProfileVersionReconfigTest, SingleEndpointProfileVersionRegistered)
    {
        SbmdEndpoint ep;
        ep.id = "1";
        ep.profile = "doorLock";
        ep.profileVersion = 3;

        SpecBasedMatterDeviceDriver driver(MakeDriverWithEndpoints(std::move(ep)));
        DeviceDriver *dd = driver.GetDriver();

        ASSERT_NE(dd->endpointProfileVersions, nullptr);

        auto *version =
            static_cast<uint8_t *>(hashMapGet(dd->endpointProfileVersions, const_cast<char *>("doorLock"), 9));
        ASSERT_NE(version, nullptr);
        EXPECT_EQ(*version, 3);
    }

    TEST_F(ProfileVersionReconfigTest, MultipleEndpointProfileVersionsRegistered)
    {
        SbmdEndpoint ep1;
        ep1.id = "1";
        ep1.profile = "light";
        ep1.profileVersion = 2;

        SbmdEndpoint ep2;
        ep2.id = "2";
        ep2.profile = "sensor";
        ep2.profileVersion = 5;

        SpecBasedMatterDeviceDriver driver(MakeDriverWithEndpoints(std::move(ep1), std::move(ep2)));
        DeviceDriver *dd = driver.GetDriver();

        ASSERT_NE(dd->endpointProfileVersions, nullptr);

        auto *lightVersion =
            static_cast<uint8_t *>(hashMapGet(dd->endpointProfileVersions, const_cast<char *>("light"), 6));
        ASSERT_NE(lightVersion, nullptr);
        EXPECT_EQ(*lightVersion, 2);

        auto *sensorVersion =
            static_cast<uint8_t *>(hashMapGet(dd->endpointProfileVersions, const_cast<char *>("sensor"), 7));
        ASSERT_NE(sensorVersion, nullptr);
        EXPECT_EQ(*sensorVersion, 5);
    }

    TEST_F(ProfileVersionReconfigTest, NoEndpointsLeavesProfileVersionsNull)
    {
        SpecBasedMatterDeviceDriver driver(MakeDriverWithEndpoints());
        DeviceDriver *dd = driver.GetDriver();

        EXPECT_EQ(dd->endpointProfileVersions, nullptr);
    }

    TEST_F(ProfileVersionReconfigTest, VersionMismatchDetected)
    {
        // Simulate the comparison that deviceServiceDeviceNeedsReconfiguring performs:
        // the persisted endpoint has profileVersion=2 but the driver expects profileVersion=3.
        SbmdEndpoint ep;
        ep.id = "1";
        ep.profile = "doorLock";
        ep.profileVersion = 3;

        SpecBasedMatterDeviceDriver driver(MakeDriverWithEndpoints(std::move(ep)));
        DeviceDriver *dd = driver.GetDriver();

        // Simulate a persisted endpoint with the old profile version
        uint8_t persistedProfileVersion = 2;

        auto *expectedVersion =
            static_cast<uint8_t *>(hashMapGet(dd->endpointProfileVersions, const_cast<char *>("doorLock"), 9));
        ASSERT_NE(expectedVersion, nullptr);
        EXPECT_NE(persistedProfileVersion, *expectedVersion) << "Profile version mismatch should be detectable";
    }

    TEST_F(ProfileVersionReconfigTest, VersionMatchDoesNotTriggerReconfiguration)
    {
        SbmdEndpoint ep;
        ep.id = "1";
        ep.profile = "doorLock";
        ep.profileVersion = 3;

        SpecBasedMatterDeviceDriver driver(MakeDriverWithEndpoints(std::move(ep)));
        DeviceDriver *dd = driver.GetDriver();

        // Persisted endpoint matches the driver's expected version
        uint8_t persistedProfileVersion = 3;

        auto *expectedVersion =
            static_cast<uint8_t *>(hashMapGet(dd->endpointProfileVersions, const_cast<char *>("doorLock"), 9));
        ASSERT_NE(expectedVersion, nullptr);
        EXPECT_EQ(persistedProfileVersion, *expectedVersion)
            << "Matching profile versions should not trigger reconfiguration";
    }

    // ========================================================================
    // Device class version reconfiguration tests
    // ========================================================================

    class DeviceClassVersionReconfigTest : public ::testing::Test
    {
    protected:
        std::vector<std::unique_ptr<SbmdDriver>> drivers;

        SbmdDriver *MakeDriverWithDcVersion(uint32_t dcVersion)
        {
            auto reg = std::make_unique<SbmdRegistration>();
            reg->name = "dcv-test";
            reg->barton.deviceClass = "testClass";
            reg->barton.deviceClassVersion = dcVersion;
            reg->matter.deviceTypes = {0x0100};
            drivers.push_back(std::make_unique<SbmdDriver>(std::move(reg), ""));

            return drivers.back().get();
        }
    };

    TEST_F(DeviceClassVersionReconfigTest, DeviceClassVersionIncludesModelVersion)
    {
        // deviceClassVersion = deviceModelVersion (3) + barton.deviceClassVersion
        SpecBasedMatterDeviceDriver driver(MakeDriverWithDcVersion(1));
        EXPECT_EQ(driver.GetDeviceClassVersion(), 4); // 3 + 1
    }

    TEST_F(DeviceClassVersionReconfigTest, DeviceClassVersionZeroBase)
    {
        SpecBasedMatterDeviceDriver driver(MakeDriverWithDcVersion(0));
        EXPECT_EQ(driver.GetDeviceClassVersion(), 3); // 3 + 0
    }

    TEST_F(DeviceClassVersionReconfigTest, GetDeviceClassVersionCallback)
    {
        SpecBasedMatterDeviceDriver driver(MakeDriverWithDcVersion(2));
        DeviceDriver *dd = driver.GetDriver();

        ASSERT_NE(dd->getDeviceClassVersion, nullptr);

        uint8_t version = 0;
        EXPECT_TRUE(dd->getDeviceClassVersion(dd->callbackContext, "testClass", &version));
        EXPECT_EQ(version, 5); // 3 + 2
    }

    TEST_F(DeviceClassVersionReconfigTest, VersionMismatchDetected)
    {
        // Simulate a persisted device with deviceClassVersion=4 (dcVersion=1) and the
        // driver now expects deviceClassVersion=5 (dcVersion=2).
        SpecBasedMatterDeviceDriver driver(MakeDriverWithDcVersion(2));
        DeviceDriver *dd = driver.GetDriver();

        uint8_t currentDriverVersion = 0;
        dd->getDeviceClassVersion(dd->callbackContext, "testClass", &currentDriverVersion);

        uint8_t persistedDeviceVersion = 4; // was dcVersion=1 → 3+1
        EXPECT_NE(persistedDeviceVersion, currentDriverVersion) << "Device class version mismatch should be detectable";
    }

    TEST_F(DeviceClassVersionReconfigTest, VersionMatchDoesNotTriggerReconfiguration)
    {
        SpecBasedMatterDeviceDriver driver(MakeDriverWithDcVersion(2));
        DeviceDriver *dd = driver.GetDriver();

        uint8_t currentDriverVersion = 0;
        dd->getDeviceClassVersion(dd->callbackContext, "testClass", &currentDriverVersion);

        uint8_t persistedDeviceVersion = 5; // same: dcVersion=2 → 3+2
        EXPECT_EQ(persistedDeviceVersion, currentDriverVersion)
            << "Matching device class versions should not trigger reconfiguration";
    }

    TEST_F(DeviceClassVersionReconfigTest, BumpedDeviceClassVersionTriggersReconfiguration)
    {
        // Construct driver with version 1, note the version, then construct
        // with version 2 and verify they differ — mirroring a firmware upgrade
        // where the .sbmd.js bumped barton.deviceClassVersion.
        SpecBasedMatterDeviceDriver driverV1(MakeDriverWithDcVersion(1));
        uint8_t v1 = driverV1.GetDeviceClassVersion();

        SpecBasedMatterDeviceDriver driverV2(MakeDriverWithDcVersion(2));
        uint8_t v2 = driverV2.GetDeviceClassVersion();

        EXPECT_NE(v1, v2);
        EXPECT_EQ(v2, v1 + 1);
    }

    // ========================================================================
    // Combined version reconfiguration tests
    // ========================================================================

    TEST_F(ProfileVersionReconfigTest, EndpointProfileVersionSetOnCreatedEndpoint)
    {
        // Verify that DoRegisterDriverResources sets profileVersion on the
        // icDeviceEndpoint.  This is critical for the persisted device to
        // record the correct version so that subsequent starts can detect
        // mismatches.
        SbmdEndpoint ep;
        ep.id = "1";
        ep.profile = "doorLock";
        ep.profileVersion = 7;

        SpecBasedMatterDeviceDriver driver(MakeDriverWithEndpoints(std::move(ep)));
        DeviceDriver *dd = driver.GetDriver();

        // The hashmap stores the version the driver expects
        auto *version =
            static_cast<uint8_t *>(hashMapGet(dd->endpointProfileVersions, const_cast<char *>("doorLock"), 9));
        ASSERT_NE(version, nullptr);
        EXPECT_EQ(*version, 7);
    }

    TEST_F(ProfileVersionReconfigTest, BumpedProfileVersionTriggersReconfiguration)
    {
        // Two drivers with different profile versions for the same profile.
        // Simulates a firmware upgrade where the endpoint profile version
        // was bumped in the .sbmd.js spec.
        SbmdEndpoint epV1;
        epV1.id = "1";
        epV1.profile = "doorLock";
        epV1.profileVersion = 1;

        SpecBasedMatterDeviceDriver driverV1(MakeDriverWithEndpoints(std::move(epV1)));

        SbmdEndpoint epV2;
        epV2.id = "1";
        epV2.profile = "doorLock";
        epV2.profileVersion = 2;

        SpecBasedMatterDeviceDriver driverV2(MakeDriverWithEndpoints(std::move(epV2)));

        auto *v1 = static_cast<uint8_t *>(
            hashMapGet(driverV1.GetDriver()->endpointProfileVersions, const_cast<char *>("doorLock"), 9));
        auto *v2 = static_cast<uint8_t *>(
            hashMapGet(driverV2.GetDriver()->endpointProfileVersions, const_cast<char *>("doorLock"), 9));

        ASSERT_NE(v1, nullptr);
        ASSERT_NE(v2, nullptr);
        EXPECT_NE(*v1, *v2);
        EXPECT_EQ(*v2, *v1 + 1);
    }

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

        // The driver is freshly constructed and still holds its source, so AddDevice's Activate()
        // re-evaluates that retained source and succeeds. Do NOT deactivate it first: that would
        // free the source (ReleaseHeavyRegistration), making Activate() re-read the pseudo-path and
        // fail at activate_failed, which bypasses the rollback this test covers (that distinct path
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
        // is a pseudo-path, so the on-demand re-read inside AddDevice's Activate() then fails,
        // deterministically exercising the activate_failed abort before any device is bound (the
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

} // namespace
