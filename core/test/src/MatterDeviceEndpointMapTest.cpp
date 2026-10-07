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
#include <chrono>
#include <cstring>
#include <future>
#include <thread>

extern "C" {
#include <icTypes/icHashMap.h>
}

using namespace barton;

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
    // In-flight-bind lifecycle race tests
    //
    // These deterministically overlap an in-progress AddDevice (past activation, before the device
    // is inserted) with a last-device removal, using the OnBindActivatedTestHook test seam as a
    // barrier. They verify the guard that keeps a racing removal from shedding an active bind, and
    // that a failed bind still tears the driver down when it is the last to hold it.
    // ========================================================================

    const char *kRaceDriverSource = R"(
        SbmdDriver({
            schemaVersion: "4.0",
            driverVersion: 1,
            name: "RaceTestDriver",
            constants: { CL_ON_OFF: 6, ATTR_ON_OFF: 0 },
            barton: { deviceClass: "raceLight", deviceClassVersion: 1 },
            matter: { deviceTypes: [0x0100] },
            aliases: { onOff: { clusterId: CL_ON_OFF, attributeId: ATTR_ON_OFF, type: "bool" } },
            endpoints: {
                "1": {
                    profile: "raceLight",
                    profileVersion: 1,
                    resources: { isOn: { type: "boolean", modes: ["read"], read: readIsOn } },
                },
            },
        });

        function readIsOn(args) { return Sbmd.result().success(); }
    )";

    // Exposes the protected last-device hook and barriers the activation seam so a test thread can
    // step the bind deterministically.
    class TestableLifecycleDriver : public SpecBasedMatterDeviceDriver
    {
    public:
        using SpecBasedMatterDeviceDriver::SpecBasedMatterDeviceDriver;

        void CallOnLastDeviceRemoved() { OnLastDeviceRemoved(); }

        std::promise<void> reachedHook;
        std::promise<void> proceed;
        std::atomic<bool> hookEnabled {false};

    protected:
        void OnBindActivatedTestHook() override
        {
            if (!hookEnabled.load())
            {
                return;
            }

            reachedHook.set_value();
            proceed.get_future().wait();
        }
    };

    class SbmdLifecycleRaceTest : public ::testing::Test
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

        std::unique_ptr<SbmdDriver> CreateRaceDriver()
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::Instance().GetMutex());
            auto reg = SbmdLoader::LoadDriver(MQuickJsRuntime::Instance().GetSharedContext(),
                                              "<race-test>",
                                              kRaceDriverSource,
                                              strlen(kRaceDriverSource));

            if (!reg)
            {
                return nullptr;
            }

            return std::make_unique<SbmdDriver>(std::move(reg), kRaceDriverSource);
        }

        // Drives AddDevice on a worker thread, pauses it at the post-activation seam, runs the
        // supplied overlap action (a concurrent last-device removal), then releases the bind.
        void RunOverlappingBind(TestableLifecycleDriver &driver,
                                SbmdDriver *sbmd,
                                std::unique_ptr<MatterDevice> device,
                                bool &bindResult)
        {
            driver.hookEnabled = true;

            auto reached = driver.reachedHook.get_future();

            std::thread binder([&driver, &device, &bindResult] { bindResult = driver.AddDevice(std::move(device)); });

            ASSERT_EQ(reached.wait_for(std::chrono::seconds(10)), std::future_status::ready)
                << "bind never reached the post-activation seam";

            // The bind has activated the driver but not yet inserted its device.
            EXPECT_TRUE(sbmd->IsActivated());

            // Simulate the prior last device being removed while the bind is in flight.
            driver.CallOnLastDeviceRemoved();
            EXPECT_TRUE(sbmd->IsActivated()) << "an in-flight bind must defer last-device deactivation";

            driver.proceed.set_value();
            binder.join();
        }
    };

    TEST_F(SbmdLifecycleRaceTest, FailedBindDeactivatesAfterRacingRemoval)
    {
        auto sbmd = CreateRaceDriver();
        ASSERT_NE(sbmd, nullptr);

        TestableLifecycleDriver driver(sbmd.get());

        // An unpopulated cache makes ResolveEndpointMap fail, so the bind fails after activation.
        auto cache = std::make_shared<DeviceDataCache>("race-fail", nullptr);
        auto device = std::make_unique<TestableMatterDevice>("race-fail", cache);

        bool bindResult = true;
        RunOverlappingBind(driver, sbmd.get(), std::move(device), bindResult);

        EXPECT_FALSE(bindResult) << "bind should fail when the device has no resolvable endpoints";
        EXPECT_FALSE(sbmd->IsActivated()) << "a failed bind that is the last holder must deactivate the driver on exit";
    }

    TEST_F(SbmdLifecycleRaceTest, SuccessfulBindRetainsDriverAfterRacingRemoval)
    {
        auto sbmd = CreateRaceDriver();
        ASSERT_NE(sbmd, nullptr);

        TestableLifecycleDriver driver(sbmd.get());

        // A cache with a matching endpoint lets ResolveEndpointMap and the bind succeed.
        auto cache = std::make_shared<DeviceDataCache>("race-ok", nullptr);
        TestableMatterDevice::PopulateTestCache(cache,
                                                {
                                                    1
        },
                                                {{1, {0x0100}}});
        auto device = std::make_unique<TestableMatterDevice>("race-ok", cache);

        bool bindResult = false;
        RunOverlappingBind(driver, sbmd.get(), std::move(device), bindResult);

        EXPECT_TRUE(bindResult) << "bind should succeed with a resolvable device";
        EXPECT_TRUE(sbmd->IsActivated()) << "the driver stays active while the newly bound device remains";
    }

} // namespace
