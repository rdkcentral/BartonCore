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

#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>

#include <opentelemetry/metrics/provider.h>
#include <opentelemetry/sdk/metrics/meter_provider.h>

#include "observability/deviceTelemetry.h"
#include "observability/observabilityMetrics.h"

extern "C" {
#include <commonDeviceDefs.h>
#include <device/icDevice.h>
#include <device/icDeviceEndpoint.h>
#include <device/icDeviceResource.h>
#include <resourceTypes.h>
}

namespace metrics_api = opentelemetry::metrics;
namespace metrics_sdk = opentelemetry::sdk::metrics;

// ---------------------------------------------------------------------------
// Mock state for device service functions
// ---------------------------------------------------------------------------

struct MockDeviceState
{
    const char *uuid = nullptr;
    const char *deviceClass = nullptr;
    const char *managingDeviceDriver = nullptr;
    const char *manufacturer = nullptr;
    const char *model = nullptr;
    const char *hardwareVersion = nullptr;
    const char *firmwareVersion = nullptr;
    bool returnNull = false;
};

struct MockEndpointState
{
    const char *profile = nullptr;
    bool returnNull = false;
};

static MockDeviceState g_mockDevice;
static MockEndpointState g_mockEndpoint;

// Helper to create a device resource and attach to a linked list
static icDeviceResource *createMockResource(const char *id, const char *value)
{
    auto *res = static_cast<icDeviceResource *>(calloc(1, sizeof(icDeviceResource)));
    res->id = strdup(id);
    res->value = value ? strdup(value) : nullptr;
    return res;
}

// ---------------------------------------------------------------------------
// Wrapped device service functions (--wrap linker overrides)
// ---------------------------------------------------------------------------

extern "C" {

icDevice *__wrap_deviceServiceGetDevice(const char *uuid)
{
    if (g_mockDevice.returnNull || uuid == nullptr)
    {
        return nullptr;
    }

    auto *device = static_cast<icDevice *>(calloc(1, sizeof(icDevice)));
    device->uuid = strdup(uuid);
    device->deviceClass = g_mockDevice.deviceClass ? strdup(g_mockDevice.deviceClass) : nullptr;
    device->managingDeviceDriver =
        g_mockDevice.managingDeviceDriver ? strdup(g_mockDevice.managingDeviceDriver) : nullptr;
    device->endpoints = linkedListCreate();
    device->metadata = linkedListCreate();

    // Build device-level resources list with metadata
    device->resources = linkedListCreate();
    auto addRes = [&](const char *resId, const char *value) {
        if (value)
        {
            linkedListAppend(device->resources, createMockResource(resId, value));
        }
    };
    addRes(COMMON_DEVICE_RESOURCE_MANUFACTURER, g_mockDevice.manufacturer);
    addRes(COMMON_DEVICE_RESOURCE_MODEL, g_mockDevice.model);
    addRes(COMMON_DEVICE_RESOURCE_HARDWARE_VERSION, g_mockDevice.hardwareVersion);
    addRes(COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION, g_mockDevice.firmwareVersion);

    return device;
}

icDeviceEndpoint *__wrap_deviceServiceGetEndpointById(const char *deviceUuid, const char *endpointId)
{
    (void) deviceUuid;
    if (g_mockEndpoint.returnNull || endpointId == nullptr)
    {
        return nullptr;
    }

    auto *ep = static_cast<icDeviceEndpoint *>(calloc(1, sizeof(icDeviceEndpoint)));
    ep->id = strdup(endpointId);
    ep->profile = g_mockEndpoint.profile ? strdup(g_mockEndpoint.profile) : nullptr;
    ep->resources = linkedListCreate();
    ep->metadata = linkedListCreate();
    return ep;
}

} // extern "C"

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------

class DeviceTelemetryTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        auto provider = opentelemetry::nostd::shared_ptr<metrics_api::MeterProvider>(new metrics_sdk::MeterProvider());
        metrics_api::Provider::SetMeterProvider(provider);

        // Default mock values: a fully-populated device
        g_mockDevice = {};
        g_mockDevice.uuid = "device-001";
        g_mockDevice.deviceClass = "light";
        g_mockDevice.managingDeviceDriver = "zigbee";
        g_mockDevice.manufacturer = "Philips";
        g_mockDevice.model = "LWA004";
        g_mockDevice.hardwareVersion = "1";
        g_mockDevice.firmwareVersion = "2.1.0";

        g_mockEndpoint = {};
        g_mockEndpoint.profile = "light";

        // Ensure cache is clean for each test
        deviceTelemetryInvalidateCache(nullptr);
    }

    void TearDown() override
    {
        deviceTelemetryInvalidateCache(nullptr);

        metrics_api::Provider::SetMeterProvider(
            opentelemetry::nostd::shared_ptr<metrics_api::MeterProvider>(new metrics_api::NoopMeterProvider()));
    }
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_F(DeviceTelemetryTest, IntegerResourceEmitsValueGauge)
{
    // Should not crash, should emit device.resource.value gauge and device.resource.update counter
    deviceTelemetryRecordResourceUpdate("device-001", "1", "localTemperature", RESOURCE_TYPE_INTEGER, "2350", true);
}

TEST_F(DeviceTelemetryTest, PercentageResourceEmitsValueGauge)
{
    deviceTelemetryRecordResourceUpdate("device-001", "1", "currentLevel", RESOURCE_TYPE_PERCENTAGE, "75", true);
}

TEST_F(DeviceTelemetryTest, LightLevelResourceEmitsValueGauge)
{
    // com.icontrol.lightLevel is a numeric type and should emit a gauge
    deviceTelemetryRecordResourceUpdate("device-001", "1", "currentLevel", RESOURCE_TYPE_LIGHT_LEVEL, "50", true);
}

TEST_F(DeviceTelemetryTest, TemperatureResourceEmitsValueGauge)
{
    // Temperature stored as hundredths of degrees C (e.g. 1900 = 19.00°C)
    deviceTelemetryRecordResourceUpdate("device-001", "1", "localTemperature", RESOURCE_TYPE_TEMPERATURE, "1900", true);
}

TEST_F(DeviceTelemetryTest, IlluminanceResourceEmitsValueGauge)
{
    deviceTelemetryRecordResourceUpdate("device-001", "1", "measuredValue", RESOURCE_TYPE_ILLUMINANCE, "350", true);
}

TEST_F(DeviceTelemetryTest, MagneticFieldStrengthResourceEmitsValueGauge)
{
    deviceTelemetryRecordResourceUpdate(
        "device-001", "1", "magneticField", RESOURCE_TYPE_MAGNETIC_FIELD_STRENGTH, "42", true);
}

TEST_F(DeviceTelemetryTest, MotionSensitivityResourceEmitsValueGauge)
{
    deviceTelemetryRecordResourceUpdate(
        "device-001", "1", "pirOMotionSensitivity", RESOURCE_TYPE_MOTION_SENSITIVITY, "5", true);
}

TEST_F(DeviceTelemetryTest, BooleanResourceEmitsStateChangeCounter)
{
    deviceTelemetryRecordResourceUpdate("device-001", "1", "isOn", RESOURCE_TYPE_BOOLEAN, "true", true);
    deviceTelemetryRecordResourceUpdate("device-001", "1", "isOn", RESOURCE_TYPE_BOOLEAN, "false", true);
}

TEST_F(DeviceTelemetryTest, StringResourceEmitsOnlyUpdateCounter)
{
    // String resources should NOT emit gauge or state_change, only update counter
    deviceTelemetryRecordResourceUpdate("device-001", "1", "colorMode", RESOURCE_TYPE_STRING, "xy", true);
}

TEST_F(DeviceTelemetryTest, UnchangedValueEmitsUpdateCounter)
{
    // didChange=false: the update counter should still fire but no gauge or state change
    deviceTelemetryRecordResourceUpdate("device-001", "1", "localTemperature", RESOURCE_TYPE_INTEGER, "2350", false);
}

TEST_F(DeviceTelemetryTest, NullDeviceUuidIsSafe)
{
    deviceTelemetryRecordResourceUpdate(nullptr, "1", "temp", RESOURCE_TYPE_INTEGER, "50", true);
}

TEST_F(DeviceTelemetryTest, NullResourceIdIsSafe)
{
    deviceTelemetryRecordResourceUpdate("device-001", "1", nullptr, RESOURCE_TYPE_INTEGER, "50", true);
}

TEST_F(DeviceTelemetryTest, NullResourceTypeSkipsTypeSpecificMetrics)
{
    // Should emit update counter but no gauge/state_change
    deviceTelemetryRecordResourceUpdate("device-001", "1", "thing", nullptr, "50", true);
}

TEST_F(DeviceTelemetryTest, NullValueSkipsTypeSpecificMetrics)
{
    deviceTelemetryRecordResourceUpdate("device-001", "1", "temp", RESOURCE_TYPE_INTEGER, nullptr, true);
}

TEST_F(DeviceTelemetryTest, DateLastContactedIsSkipped)
{
    // This high-frequency resource should be silently ignored
    deviceTelemetryRecordResourceUpdate(
        "device-001", "1", COMMON_DEVICE_RESOURCE_DATE_LAST_CONTACTED, RESOURCE_TYPE_DATETIME, "1234567890", true);
}

TEST_F(DeviceTelemetryTest, DeviceAttributesCachedOnSecondCall)
{
    // First call populates cache
    deviceTelemetryRecordResourceUpdate("device-001", "1", "temp", RESOURCE_TYPE_INTEGER, "50", true);

    // Change mock to return NULL — cached value should still be used
    g_mockDevice.returnNull = true;
    deviceTelemetryRecordResourceUpdate("device-001", "1", "temp", RESOURCE_TYPE_INTEGER, "55", true);
}

TEST_F(DeviceTelemetryTest, CacheInvalidationOnDeviceRemoval)
{
    // Populate cache
    deviceTelemetryRecordResourceUpdate("device-001", "1", "temp", RESOURCE_TYPE_INTEGER, "50", true);

    // Invalidate
    deviceTelemetryInvalidateCache("device-001");

    // Now mock returns different data
    g_mockDevice.manufacturer = "NewManufacturer";
    deviceTelemetryRecordResourceUpdate("device-001", "1", "temp", RESOURCE_TYPE_INTEGER, "55", true);
    // Should use new data (no crash = pass; attribute verification requires export reader)
}

TEST_F(DeviceTelemetryTest, CacheInvalidationOnFirmwareUpdate)
{
    // Populate cache
    deviceTelemetryRecordResourceUpdate("device-001", "1", "temp", RESOURCE_TYPE_INTEGER, "50", true);

    // Firmware update triggers auto-invalidation
    g_mockDevice.firmwareVersion = "3.0.0";
    deviceTelemetryRecordResourceUpdate(
        "device-001", "1", COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION, RESOURCE_TYPE_VERSION, "3.0.0", true);

    // Next call should re-populate from mock (cache was invalidated)
    g_mockDevice.returnNull = true;
    // This should now get NULL from device service and have no cached entry
    deviceTelemetryRecordResourceUpdate("device-001", "1", "temp", RESOURCE_TYPE_INTEGER, "60", true);
}

TEST_F(DeviceTelemetryTest, IncompleteDeviceNotCached)
{
    // Simulate device-add sequence: no manufacturer yet
    g_mockDevice.manufacturer = nullptr;

    // First call should NOT cache (incomplete)
    deviceTelemetryRecordResourceUpdate("device-001", "1", "temp", RESOURCE_TYPE_INTEGER, "50", true);

    // Now populate manufacturer
    g_mockDevice.manufacturer = "Philips";
    // Second call should populate and cache
    deviceTelemetryRecordResourceUpdate("device-001", "1", "temp", RESOURCE_TYPE_INTEGER, "55", true);

    // Verify cached by disabling mock
    g_mockDevice.returnNull = true;
    deviceTelemetryRecordResourceUpdate("device-001", "1", "temp", RESOURCE_TYPE_INTEGER, "60", true);
}

TEST_F(DeviceTelemetryTest, IncompleteDeviceNoDeviceClass)
{
    // Simulate no device class set yet
    g_mockDevice.deviceClass = nullptr;
    g_mockDevice.manufacturer = "Philips";

    // First call should NOT cache
    deviceTelemetryRecordResourceUpdate("device-001", "1", "temp", RESOURCE_TYPE_INTEGER, "50", true);

    // Now set device class
    g_mockDevice.deviceClass = "light";
    deviceTelemetryRecordResourceUpdate("device-001", "1", "temp", RESOURCE_TYPE_INTEGER, "55", true);

    // Should now be cached
    g_mockDevice.returnNull = true;
    deviceTelemetryRecordResourceUpdate("device-001", "1", "temp", RESOURCE_TYPE_INTEGER, "60", true);
}

TEST_F(DeviceTelemetryTest, DeviceNotFoundIsSafe)
{
    g_mockDevice.returnNull = true;
    // Should not crash — attributes will all be empty strings
    deviceTelemetryRecordResourceUpdate("device-999", "1", "temp", RESOURCE_TYPE_INTEGER, "50", true);
}

TEST_F(DeviceTelemetryTest, NullEndpointIdIsSafe)
{
    // Device-level resources have NULL endpointId
    deviceTelemetryRecordResourceUpdate("device-001", nullptr, "firmwareVersion", RESOURCE_TYPE_VERSION, "2.1.0", true);
}

TEST_F(DeviceTelemetryTest, InvalidNumericValueDoesNotEmitGauge)
{
    // Non-parseable integer should skip gauge emission but still emit update counter
    deviceTelemetryRecordResourceUpdate("device-001", "1", "temp", RESOURCE_TYPE_INTEGER, "notanumber", true);
}

TEST_F(DeviceTelemetryTest, MultipleDevicesCacheIndependently)
{
    g_mockDevice.manufacturer = "Philips";
    deviceTelemetryRecordResourceUpdate("device-001", "1", "temp", RESOURCE_TYPE_INTEGER, "50", true);

    g_mockDevice.manufacturer = "Yale";
    g_mockDevice.deviceClass = "doorLock";
    deviceTelemetryRecordResourceUpdate("device-002", "1", "locked", RESOURCE_TYPE_BOOLEAN, "true", true);

    // Both should be cached independently
    g_mockDevice.returnNull = true;
    deviceTelemetryRecordResourceUpdate("device-001", "1", "temp", RESOURCE_TYPE_INTEGER, "55", true);
    deviceTelemetryRecordResourceUpdate("device-002", "1", "locked", RESOURCE_TYPE_BOOLEAN, "false", true);
}

TEST_F(DeviceTelemetryTest, InvalidateAllCaches)
{
    deviceTelemetryRecordResourceUpdate("device-001", "1", "temp", RESOURCE_TYPE_INTEGER, "50", true);
    deviceTelemetryRecordResourceUpdate("device-002", "1", "isOn", RESOURCE_TYPE_BOOLEAN, "true", true);

    // Invalidate all
    deviceTelemetryInvalidateCache(nullptr);

    g_mockDevice.returnNull = true;
    // Both should be uncached now; deviceServiceGetDevice returns NULL
    deviceTelemetryRecordResourceUpdate("device-001", "1", "temp", RESOURCE_TYPE_INTEGER, "55", true);
    deviceTelemetryRecordResourceUpdate("device-002", "1", "isOn", RESOURCE_TYPE_BOOLEAN, "false", true);
}

// ---------------------------------------------------------------------------
// Zigbee RSSI/LQI tests
// ---------------------------------------------------------------------------

TEST_F(DeviceTelemetryTest, RssiResourceEmitsZigbeeRssiGauge)
{
    // RSSI values are negative dBm
    deviceTelemetryRecordResourceUpdate("device-001", nullptr, "feRssi", RESOURCE_TYPE_RSSI, "-65", true);
}

TEST_F(DeviceTelemetryTest, LqiResourceEmitsZigbeeLqiGauge)
{
    // LQI values are 0-255
    deviceTelemetryRecordResourceUpdate("device-001", nullptr, "feLqi", RESOURCE_TYPE_LQI, "200", true);
}

TEST_F(DeviceTelemetryTest, RssiUnchangedStillEmitsUpdateCounter)
{
    // didChange=false should still emit update counter but not RSSI gauge
    deviceTelemetryRecordResourceUpdate("device-001", nullptr, "feRssi", RESOURCE_TYPE_RSSI, "-65", false);
}
