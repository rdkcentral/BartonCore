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
//
// Created by Thomas Lea on 4/17/25.
//

// NOTE: this Zigbee Occupancy Sensor driver maps to a sensor device class and profile
//       but with some additional and optional support for humidity, temperature, and
//       illuminance measurement.

#include "device-driver/device-driver-manager.h"
#include "deviceServicePrivate.h"
#include "device/icDeviceResource.h"
#include "deviceDrivers/zigbeeDriverCommon.h"
#include "glib.h"
#include "icUtil/stringUtils.h"
#include "subsystems/zigbee/zigbeeSubsystem.h"
#include "zigbeeClusters/illuminanceMeasurementCluster.h"
#include "zigbeeClusters/occupancySensingCluster.h"
#include "zigbeeClusters/relativeHumidityMeasurementCluster.h"
#include <commonDeviceDefs.h>
#include <device/deviceModelHelper.h>
#include <icLog/logging.h>
#include <icUtil/array.h>
#include <resourceTypes.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>

#define LOG_TAG                    "ZigBeeOccupancyDD"
#define DEVICE_DRIVER_NAME         "ZigBeeOccupancyDD"
#define MY_DC_VERSION              1

#define OCCUPANCY_SENSOR_DEVICE_ID 0x0107

#define HUMIDITY_FORMAT_STRING     "%.2f"

/* ZigbeeDriverCommonCallbacks */
static bool fetchInitialResourceValues(ZigbeeDriverCommon *ctx,
                                       icDevice *device,
                                       IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                                       icInitialResourceValues *initialResourceValues);
static const char *mapDeviceIdToProfile(ZigbeeDriverCommon *ctx, uint16_t deviceId);
static bool registerResources(ZigbeeDriverCommon *ctx,
                              icDevice *device,
                              IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                              icInitialResourceValues *initialResourceValues);
static void synchronizeDevice(ZigbeeDriverCommon *ctx, icDevice *device, IcDiscoveredDeviceDetails *details);

static const uint16_t myDeviceIds[] = {OCCUPANCY_SENSOR_DEVICE_ID};

static void occupancyUpdated(void *ctx, uint64_t eui64, uint8_t endpointId, bool isOccupied);
static const OccupancySensingClusterCallbacks occupancySensingClusterCallbacks = {
    .occupancyUpdated = occupancyUpdated,
};

static void illuminanceMeasuredValueUpdated(void *ctx, uint64_t eui64, uint8_t endpointId, uint16_t value);
static const IlluminanceMeasurementClusterCallbacks illuminanceMeasurementClusterCallbacks = {
    .measuredValueUpdated = illuminanceMeasuredValueUpdated,
};

static void relativeHumidityMeasuredValueUpdated(void *ctx, uint64_t eui64, uint8_t endpointId, uint16_t value);
static const RelativeHumidityMeasurementClusterCallbacks relativeHumidityMeasurementClusterCallbacks = {
    .measuredValueUpdated = relativeHumidityMeasuredValueUpdated,
};

// zigbee device driver registration order matters, so we pick constructor priority carefully
__attribute__((constructor(400))) static void driverRegister(void)
{
    static const ZigbeeDriverCommonCallbacks myHooks = {.fetchInitialResourceValues = fetchInitialResourceValues,
                                                        .mapDeviceIdToProfile = mapDeviceIdToProfile,
                                                        .registerResources = registerResources,
                                                        .synchronizeDevice = synchronizeDevice};

    DeviceDriver *myDriver = zigbeeDriverCommonCreateDeviceDriver(DEVICE_DRIVER_NAME,
                                                                  SENSOR_DC,
                                                                  MY_DC_VERSION,
                                                                  myDeviceIds,
                                                                  ARRAY_LENGTH(myDeviceIds),
                                                                  RX_MODE_SLEEPY,
                                                                  &myHooks,
                                                                  false);

    // Doesn't need to be in device descriptor, these are special experimental devices
    myDriver->neverReject = true;

    zigbeeDriverCommonAddCluster(myDriver, occupancySensingClusterCreate(&occupancySensingClusterCallbacks, myDriver));
    zigbeeDriverCommonAddCluster(
        myDriver, illuminanceMeasurementClusterCreate(&illuminanceMeasurementClusterCallbacks, myDriver));
    zigbeeDriverCommonAddCluster(
        myDriver, relativeHumidityMeasurementClusterCreate(&relativeHumidityMeasurementClusterCallbacks, myDriver));

    deviceDriverManagerRegisterDriver(myDriver);
}

static const char *mapDeviceIdToProfile(ZigbeeDriverCommon *ctx, uint16_t deviceId)
{
    const char *profile = NULL;

    switch (deviceId)
    {
        case OCCUPANCY_SENSOR_DEVICE_ID:
            profile = SENSOR_PROFILE;
            break;
        default:
            break;
    }

    return profile;
}

static bool fetchInitialResourceValues(ZigbeeDriverCommon *ctx,
                                       icDevice *device,
                                       IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                                       icInitialResourceValues *initialResourceValues)
{
    bool result = true;
    bool foundOne = false;
    uint64_t eui64 = zigbeeSubsystemIdToEui64(device->uuid);

    for (int i = 0; i < discoveredDeviceDetails->numEndpoints; i++)
    {
        IcDiscoveredEndpointDetails endpointDetails = discoveredDeviceDetails->endpointDetails[i];
        g_autofree char *epName = zigbeeSubsystemEndpointIdAsString(endpointDetails.endpointId);
        if (icDiscoveredDeviceDetailsEndpointHasCluster(
                discoveredDeviceDetails, endpointDetails.endpointId, OCCUPANCY_SENSING_CLUSTER_ID, true))
        {
            bool isOccupied = false;
            result &= occupancySensingClusterGetIsOccupied(eui64, endpointDetails.endpointId, &isOccupied);
            initialResourceValuesPutEndpointValue(initialResourceValues,
                                                  epName,
                                                  SENSOR_PROFILE_RESOURCE_FAULTED,
                                                  stringValueOfBool(isOccupied));

            foundOne |= result;
        }

        if (icDiscoveredDeviceDetailsEndpointHasCluster(
                discoveredDeviceDetails, endpointDetails.endpointId, ILLUMINANCE_MEASUREMENT_CLUSTER_ID, true))
        {
            uint16_t illuminance = 0;
            result &= illuminanceMeasurementClusterGetMeasuredValue(eui64, endpointDetails.endpointId, &illuminance);
            g_autofree char *illuminanceStr = NULL;
            if (illuminanceMeasurementClusterIsValueValid(illuminance))
            {
                illuminanceStr = g_strdup_printf("%" PRIu32, illuminanceMeasurementClusterConvertToLux(illuminance));
            }
            initialResourceValuesPutEndpointValue(initialResourceValues,
                                                  epName,
                                                  SENSOR_PROFILE_RESOURCE_ILLUMINANCE,
                                                  illuminanceStr);
        }

        if (icDiscoveredDeviceDetailsEndpointHasCluster(
                discoveredDeviceDetails, endpointDetails.endpointId, RELATIVE_HUMIDITY_MEASUREMENT_CLUSTER_ID, true))
        {
            uint16_t humidity = 0;
            result &= relativeHumidityMeasurementClusterGetMeasuredValue(eui64, endpointDetails.endpointId, &humidity);
            g_autofree char *humidityStr = NULL;
            if (relativeHumidityMeasurementClusterIsValueValid(humidity))
            {
                humidityStr = g_strdup_printf(HUMIDITY_FORMAT_STRING,
                                              relativeHumidityMeasurementClusterConvertToPercentage(humidity));
            }
            initialResourceValuesPutEndpointValue(initialResourceValues,
                                                  epName,
                                                  SENSOR_PROFILE_RESOURCE_HUMIDITY,
                                                  humidityStr);
        }
    }

    return result;
}

static bool registerResources(ZigbeeDriverCommon *ctx,
                              icDevice *device,
                              IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                              icInitialResourceValues *initialResourceValues)
{

    bool result = true;

    icLogDebug(LOG_TAG, "%s: uuid=%s", __func__, device->uuid);

    // get the eui64 for the device, which is the uuid
    uint64_t eui64 = zigbeeSubsystemIdToEui64(device->uuid);

    for (uint8_t i = 0; i < discoveredDeviceDetails->numEndpoints; i++)
    {
        uint8_t endpointId = discoveredDeviceDetails->endpointDetails[i].endpointId;
        g_autofree char *epName = zigbeeSubsystemEndpointIdAsString(endpointId);

        icDeviceEndpoint *endpoint = createEndpoint(device, epName, SENSOR_PROFILE, true);

        result = createEndpointResourceIfAvailable(endpoint,
                                                   SENSOR_PROFILE_RESOURCE_FAULTED,
                                                   initialResourceValues,
                                                   RESOURCE_TYPE_BOOLEAN,
                                                   RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC |
                                                       RESOURCE_MODE_EMIT_EVENTS,
                                                   CACHING_POLICY_ALWAYS);

        result &= createEndpointResourceIfAvailable(endpoint,
                                                    SENSOR_PROFILE_RESOURCE_ILLUMINANCE,
                                                    initialResourceValues,
                                                    RESOURCE_TYPE_ILLUMINANCE,
                                                    RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC |
                                                        RESOURCE_MODE_EMIT_EVENTS,
                                                    CACHING_POLICY_ALWAYS) != NULL;

        result &= createEndpointResourceIfAvailable(endpoint,
                                                    SENSOR_PROFILE_RESOURCE_HUMIDITY,
                                                    initialResourceValues,
                                                    RESOURCE_TYPE_PERCENTAGE,
                                                    RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC |
                                                        RESOURCE_MODE_EMIT_EVENTS,
                                                    CACHING_POLICY_ALWAYS) != NULL;

        createEndpointResource(endpoint,
                               SENSOR_PROFILE_RESOURCE_QUALIFIED,
                               "false",
                               RESOURCE_TYPE_BOOLEAN,
                               RESOURCE_MODE_READABLE,
                               CACHING_POLICY_ALWAYS);

        createEndpointResource(endpoint,
                               SENSOR_PROFILE_RESOURCE_BYPASSED,
                               "false",
                               RESOURCE_TYPE_BOOLEAN,
                               RESOURCE_MODE_READABLE,
                               CACHING_POLICY_ALWAYS | RESOURCE_MODE_EMIT_EVENTS);

        zigbeeDriverCommonSetEndpointNumber(endpoint, endpointId);
    }

    return result;
}

static void synchronizeDevice(ZigbeeDriverCommon *ctx, icDevice *device, IcDiscoveredDeviceDetails *details)
{
    icLogDebug(LOG_TAG, "%s: uuid=%s", __func__, device->uuid);

    if (ctx == NULL || device == NULL || details == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __func__);
        return;
    }

    // get the eui64 for the device, which is the uuid
    uint64_t eui64 = zigbeeSubsystemIdToEui64(device->uuid);

    for (uint8_t i = 0; i < details->numEndpoints; i++)
    {
        uint8_t endpointId = details->endpointDetails[i].endpointId;

        bool isOccupied = false;
        if (occupancySensingClusterGetIsOccupied(eui64, endpointId, &isOccupied))
        {
            occupancyUpdated(ctx, eui64, endpointId, isOccupied);
        }

        if (icDiscoveredDeviceDetailsEndpointHasCluster(
                details, endpointId, ILLUMINANCE_MEASUREMENT_CLUSTER_ID, true))
        {
            uint16_t illuminance = 0;
            if (illuminanceMeasurementClusterGetMeasuredValue(eui64, endpointId, &illuminance))
            {
                illuminanceMeasuredValueUpdated(ctx, eui64, endpointId, illuminance);
            }
        }

        if (icDiscoveredDeviceDetailsEndpointHasCluster(
                details, endpointId, RELATIVE_HUMIDITY_MEASUREMENT_CLUSTER_ID, true))
        {
            uint16_t humidity = 0;
            if (relativeHumidityMeasurementClusterGetMeasuredValue(eui64, endpointId, &humidity))
            {
                relativeHumidityMeasuredValueUpdated(ctx, eui64, endpointId, humidity);
            }
        }
    }
}

static void occupancyUpdated(void *ctx, uint64_t eui64, uint8_t endpointId, bool isOccupied)
{
    icLogDebug(LOG_TAG, "%s: occupancy=%s", __func__, stringValueOfBool(isOccupied));
    g_autofree char *deviceId = zigbeeSubsystemEui64ToId(eui64);
    g_autofree char *epName = zigbeeSubsystemEndpointIdAsString(endpointId);
    updateResource(deviceId, epName, SENSOR_PROFILE_RESOURCE_FAULTED, stringValueOfBool(isOccupied), NULL);
}

static void illuminanceMeasuredValueUpdated(void *ctx, uint64_t eui64, uint8_t endpointId, uint16_t value)
{
    icLogDebug(LOG_TAG, "%s: value=%" PRIu16, __func__, value);
    g_autofree char *deviceId = zigbeeSubsystemEui64ToId(eui64);
    g_autofree char *epName = zigbeeSubsystemEndpointIdAsString(endpointId);
    g_autofree char *illuminanceStr = NULL;
    if (illuminanceMeasurementClusterIsValueValid(value))
    {

        illuminanceStr = g_strdup_printf("%" PRIu32, illuminanceMeasurementClusterConvertToLux(value));
    }
    updateResource(deviceId, epName, SENSOR_PROFILE_RESOURCE_ILLUMINANCE, illuminanceStr, NULL);
}

static void relativeHumidityMeasuredValueUpdated(void *ctx, uint64_t eui64, uint8_t endpointId, uint16_t value)
{
    icLogDebug(LOG_TAG, "%s: value=%" PRIu16, __func__, value);
    g_autofree char *deviceId = zigbeeSubsystemEui64ToId(eui64);
    g_autofree char *epName = zigbeeSubsystemEndpointIdAsString(endpointId);
    g_autofree char *humidityStr = NULL;
    if (relativeHumidityMeasurementClusterIsValueValid(value))
    {
        humidityStr =
            g_strdup_printf(HUMIDITY_FORMAT_STRING, relativeHumidityMeasurementClusterConvertToPercentage(value));
    }
    updateResource(deviceId, epName, SENSOR_PROFILE_RESOURCE_HUMIDITY, humidityStr, NULL);
}
