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
// Created by tlea on 2/12/19.
//

#include "device-driver/device-driver-manager.h"
#include "deviceDrivers/zigbeeDriverCommon.h"
#include "zigbeeClusters/colorControlCluster.h"
#include "zigbeeClusters/levelControlCluster.h"
#include "zigbeeClusters/onOffCluster.h"
#include <commonDeviceDefs.h>
#include <device/deviceModelHelper.h>
#include <deviceServicePrivate.h>
#include <errno.h>
#include <icLog/logging.h>
#include <icUtil/stringUtils.h>
#include <math.h>
#include <memory.h>
#include <resourceTypes.h>
#include <stdio.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>
#include <zconf.h>
#include <zigbeeClusters/electricalMeasurementCluster.h>
#include <zigbeeClusters/meteringCluster.h>

#define LOG_TAG                         "zigbeeLightDD"
#define DRIVER_NAME                     "zigbeeLight"
#define DEVICE_CLASS_NAME               "light"
#define MY_DC_VERSION                   2
#define DIVISOR_METADATA                "divisor"
#define MULTIPLIER_METADATA             "multiplier"
#define POWER_MEASUREMENT_TYPE_METADATA "pmtype"
#define POWER_MEASUREMENT_TYPE_EM       "em"
#define POWER_MEASUREMENT_TYPE_SM       "sm"

// only compile if we support zigbee
#ifdef BARTON_CONFIG_ZIGBEE

#define isSwitchDevice(deviceId)                                                                                       \
    (deviceId == ON_OFF_LIGHT_SWITCH_DEVICE_ID || deviceId == DIMMABLE_LIGHT_SWITCH_DEVICE_ID ||                       \
     deviceId == COLOR_DIMMABLE_LIGHT_SWITCH_DEVICE_ID)

// The precision error we are allowing to consider updating the value of the bulb.
#define MAX_RESOUCE_COLOR_ERROR 0.001

static uint16_t myDeviceIds[] = {ON_OFF_LIGHT_DEVICE_ID,
                                 DIMMABLE_LIGHT_DEVICE_ID,
                                 COLOR_DIMMABLE_LIGHT_DEVICE_ID,
                                 COLOR_DIMMABLE2_LIGHT_DEVICE_ID,
                                 EXTENDED_COLOR_LIGHT_DEVICE_ID,
                                 EXTENDED_COLOR2_LIGHT_DEVICE_ID,
                                 COLOR_TEMPERATURE_LIGHT_DEVICE_ID,
                                 COLOR_TEMPERATURE2_LIGHT_DEVICE_ID,
                                 ON_OFF_PLUGIN_UNIT_DEVICE_ID,
                                 DIMMABLE_PLUGIN_UNIT_DEVICE_ID,
                                 SMART_PLUG_DEVICE_ID};

static bool fetchInitialResourceValues(ZigbeeDriverCommon *ctx,
                                       icDevice *device,
                                       IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                                       icInitialResourceValues *initialResourceValues);

static bool registerResources(ZigbeeDriverCommon *ctx,
                              icDevice *device,
                              IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                              icInitialResourceValues *initialResourceValues);

static char *getColorString(double x, double y);

static bool parseColorString(const char *colorString, double *x, double *y);

static void onOffStateChangedCallback(uint64_t eui64, uint8_t endpointId, bool isOn, const void *ctx);

static void levelChangedCallback(uint64_t eui64, uint8_t endpointId, uint8_t level, const void *ctx);

static void colorControlXChanged(uint64_t eui64, uint8_t endpointId, double x, const void *ctx);

static void colorControlYChanged(uint64_t eui64, uint8_t endpointId, double y, const void *ctx);

static void colorControlXYChanged(uint64_t eui64, uint8_t endpointId, double x, double y, const void *ctx);

static void activePowerChanged(uint64_t eui64, uint8_t endpointId, int16_t watts, const void *ctx);

static void instantaneousDemandChanged(uint64_t eui64, uint8_t endpointId, int32_t kilowatts, const void *ctx);

static const char *mapDeviceIdToProfile(ZigbeeDriverCommon *ctx, uint16_t deviceId);

static bool writeEndpointResource(ZigbeeDriverCommon *ctx,
                                  uint32_t endpointNumber,
                                  icDeviceResource *resource,
                                  const char *previousValue,
                                  const char *newValue,
                                  bool *baseDriverUpdatesResource);

static bool
preConfigureCluster(ZigbeeDriverCommon *ctx, ZigbeeCluster *cluster, DeviceConfigurationContext *deviceConfigContext);

static void synchronizeDevice(ZigbeeDriverCommon *ctx, icDevice *device, IcDiscoveredDeviceDetails *details);

static const ZigbeeDriverCommonCallbacks commonCallbacks = {
    .fetchInitialResourceValues = fetchInitialResourceValues,
    .registerResources = registerResources,
    .mapDeviceIdToProfile = mapDeviceIdToProfile,
    .writeEndpointResource = writeEndpointResource,
    .preConfigureCluster = preConfigureCluster,
    .synchronizeDevice = synchronizeDevice,
};

static const OnOffClusterCallbacks onOffClusterCallbacks = {
    .onOffStateChanged = onOffStateChangedCallback,
};

static const LevelControlClusterCallbacks levelControlClusterCallbacks = {
    .levelChanged = levelChangedCallback,
};

static const ColorControlClusterCallbacks colorControlClusterCallbacks = {
    .currentXChanged = colorControlXChanged,
    .currentYChanged = colorControlYChanged,
    .currentXYChanged = colorControlXYChanged,
};

static const ElectricalMeasurementClusterCallbacks electricalMeasurementClusterCallbacks = {
    .activePowerChanged = activePowerChanged,
};

static const MeteringClusterCallbacks meteringClusterCallbacks = {
    .instantaneousDemandChanged = instantaneousDemandChanged,
};

// zigbee device driver registration order matters, so we pick constructor priority carefully
__attribute__((constructor(250))) static void driverRegister(void)
{
    DeviceDriver *myDriver = zigbeeDriverCommonCreateDeviceDriver(DRIVER_NAME,
                                                                  DEVICE_CLASS_NAME,
                                                                  MY_DC_VERSION,
                                                                  myDeviceIds,
                                                                  sizeof(myDeviceIds) / sizeof(uint16_t),
                                                                  RX_MODE_NON_SLEEPY,
                                                                  &commonCallbacks,
                                                                  false);

    zigbeeDriverCommonAddCluster(myDriver, onOffClusterCreate(&onOffClusterCallbacks, myDriver));
    zigbeeDriverCommonAddCluster(myDriver, levelControlClusterCreate(&levelControlClusterCallbacks, myDriver));
    zigbeeDriverCommonAddCluster(myDriver, colorControlClusterCreate(&colorControlClusterCallbacks, myDriver));
    zigbeeDriverCommonAddCluster(myDriver,
                                 electricalMeasurementClusterCreate(&electricalMeasurementClusterCallbacks, myDriver));
    zigbeeDriverCommonAddCluster(myDriver, meteringClusterCreate(&meteringClusterCallbacks, myDriver));

    // enable periodic collection of common diagnostics info (rssi, lqi, etc).
    zigbeeDriverCommonSetDiagnosticsCollectionEnabled(myDriver, true);

    deviceDriverManagerRegisterDriver(myDriver);
}

static void onOffStateChangedCallback(uint64_t eui64, uint8_t endpointId, bool isOn, const void *ctx)
{
    icLogDebug(LOG_TAG, "%s: light is now %s", __FUNCTION__, isOn ? "on" : "off");

    char epName[4]; // max uint8_t + \0
    sprintf(epName, "%" PRIu8, endpointId);

    char *uuid = zigbeeSubsystemEui64ToId(eui64);

    updateResource(uuid, epName, LIGHT_PROFILE_RESOURCE_IS_ON, isOn ? "true" : "false", NULL);

    free(uuid);
}

static void levelChangedCallback(uint64_t eui64, uint8_t endpointId, uint8_t level, const void *ctx)
{
    icLogDebug(LOG_TAG, "%s: light is now  at level %" PRIu8, __FUNCTION__, level);

    char epName[4]; // max uint8_t + \0
    sprintf(epName, "%" PRIu8, endpointId);

    char *uuid = zigbeeSubsystemEui64ToId(eui64);

    char *levelStr = levelControlClusterGetLevelString(level);
    updateResource(uuid, epName, LIGHT_PROFILE_RESOURCE_CURRENT_LEVEL, levelStr, NULL);

    free(levelStr);
    free(uuid);
}

static void updateColorResource(uint64_t eui64, uint8_t endpointId, double *x, double *y)
{
    // Here we received an update to either x or y (one may be NULL).  Need to fetch from our saved resource
    // and update them with what we got

    char *uuid = zigbeeSubsystemEui64ToId(eui64);
    char epName[4]; // max uint8_t + \0
    sprintf(epName, "%" PRIu8, endpointId);

    AUTO_CLEAN(resourceDestroy__auto)
    icDeviceResource *resource = deviceServiceGetResourceById(uuid, epName, LIGHT_PROFILE_RESOURCE_COLOR);

    if (resource == NULL || resource->value == NULL)
    {
        icLogError(LOG_TAG, "%s: resource %s not found!", __FUNCTION__, LIGHT_PROFILE_RESOURCE_COLOR);
    }
    else
    {
        double tempX = 0;
        double tempY = 0;
        bool shouldUpdate = false;

        // Get the old values of the resource
        if (parseColorString(resource->value, &tempX, &tempY) == true)
        {
            // Now replace the old value(s) with the new value(s) if there's been considerable change
            if (x != NULL && (fabs(*x - tempX) >= MAX_RESOUCE_COLOR_ERROR))
            {
                tempX = *x;
                shouldUpdate = true;
            }
            if (y != NULL && (fabs(*y - tempY) >= MAX_RESOUCE_COLOR_ERROR))
            {
                tempY = *y;
                shouldUpdate = true;
            }

            if (shouldUpdate == true)
            {
                AUTO_CLEAN(free_generic__auto) char *xyStr = getColorString(tempX, tempY);
                updateResource(uuid, epName, LIGHT_PROFILE_RESOURCE_COLOR, xyStr, NULL);
            }
        }
        else
        {
            icLogError(LOG_TAG, "%s: could not parse previous color value %s", __FUNCTION__, resource->value);
        }
    }

    free(uuid);
}

static void colorControlXChanged(uint64_t eui64, uint8_t endpointId, double x, const void *ctx)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    updateColorResource(eui64, endpointId, &x, NULL);
}

static void colorControlYChanged(uint64_t eui64, uint8_t endpointId, double y, const void *ctx)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    updateColorResource(eui64, endpointId, NULL, &y);
}

static void colorControlXYChanged(uint64_t eui64, uint8_t endpointId, double x, double y, const void *ctx)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    updateColorResource(eui64, endpointId, &x, &y);
}

// attempt to retrieve from metadata.  If it isnt there, read from device and store in metadata
static bool getPowerComponent(uint64_t eui64,
                              uint8_t endpointId,
                              bool useElectricalMeasurementCluster,
                              bool getDivisor,
                              uint64_t *value)
{
    bool result = true;

    if (value == NULL)
    {
        return false;
    }

    char *uuid = zigbeeSubsystemEui64ToId(eui64);
    char epName[4]; // max uint8_t + \0
    sprintf(epName, "%" PRIu8, endpointId);

    char *metadata = getMetadata(uuid, epName, getDivisor ? DIVISOR_METADATA : MULTIPLIER_METADATA);

    if (metadata != NULL)
    {
        // we use what we have cached in metadata
        sscanf(metadata, "%" PRIu64, value);
    }
    else
    {
        // we have to get it from the cluster, then cache as metadata

        metadata = (char *) malloc(20); // 9223372036854775808 \0

        if (useElectricalMeasurementCluster)
        {
            uint16_t emval = 0;
            if (getDivisor)
            {
                if (electricalMeasurementClusterGetAcPowerDivisor(eui64, endpointId, &emval) == false)
                {
                    icLogError(LOG_TAG, "%s: failed to read power component", __FUNCTION__);
                    result = false;
                }
            }
            else
            {
                if (electricalMeasurementClusterGetAcPowerMultiplier(eui64, endpointId, &emval) == false)
                {
                    icLogError(LOG_TAG, "%s: failed to read power component", __FUNCTION__);
                    result = false;
                }
            }

            if (result)
            {
                *value = emval;
            }
        }
        else
        {
            uint32_t mval = 0;
            if (getDivisor)
            {
                if (meteringClusterGetDivisor(eui64, endpointId, &mval) == false)
                {
                    icLogError(LOG_TAG, "%s: failed to read power component", __FUNCTION__);
                    result = false;
                }
            }
            else
            {
                if (meteringClusterGetMultiplier(eui64, endpointId, &mval) == false)
                {
                    icLogError(LOG_TAG, "%s: failed to read power component", __FUNCTION__);
                    result = false;
                }
            }

            if (result)
            {
                *value = mval;
            }
        }

        snprintf(metadata, 20, "%" PRIu64, *value);
        setMetadata(uuid, epName, getDivisor ? DIVISOR_METADATA : MULTIPLIER_METADATA, metadata);
    }

    free(metadata);
    free(uuid);
    return result;
}

static bool getPowerDivisor(uint64_t eui64, uint8_t endpointId, bool useElectricalMeasurementCluster, uint64_t *divisor)
{
    return getPowerComponent(eui64, endpointId, useElectricalMeasurementCluster, true, divisor);
}

static bool
getPowerMultiplier(uint64_t eui64, uint8_t endpointId, bool useElectricalMeasurementCluster, uint64_t *multiplier)
{
    return getPowerComponent(eui64, endpointId, useElectricalMeasurementCluster, false, multiplier);
}

static void updatePowerResource(uint64_t eui64, uint8_t endpointId, int64_t val)
{
    // val is the raw power without having multiplier and divisor applied
    //   we need to determine which power method we are using
    char *uuid = zigbeeSubsystemEui64ToId(eui64);
    char epName[4]; // max uint8_t + \0
    sprintf(epName, "%" PRIu8, endpointId);
    char *emtype = getMetadata(uuid, epName, POWER_MEASUREMENT_TYPE_METADATA);
    bool useEm = false;
    uint64_t divisor = 1;
    uint64_t multiplier = 0;
    int64_t watts = 0;
    char powerStr[20]; // 9223372036854775808 \0

    if (emtype == NULL)
    {
        icLogError(LOG_TAG, "%s: energy measurement type not known", __FUNCTION__);
        goto exit;
    }

    useEm = strcmp(emtype, POWER_MEASUREMENT_TYPE_EM) == 0;
    free(emtype);

    if (!getPowerDivisor(eui64, endpointId, useEm, &divisor))
    {
        icLogError(LOG_TAG, "%s: failed to get divisor", __FUNCTION__);
        goto exit;
    }

    if (!getPowerMultiplier(eui64, endpointId, useEm, &multiplier))
    {
        icLogError(LOG_TAG, "%s: failed to get multiplier", __FUNCTION__);
        goto exit;
    }

    watts = val * multiplier / divisor;

    snprintf(powerStr, 20, "%" PRIi64, watts);

    updateResource(uuid, epName, LIGHT_PROFILE_RESOURCE_CURRENT_POWER, powerStr, NULL);

exit:
    free(uuid);
}

static void activePowerChanged(uint64_t eui64, uint8_t endpointId, int16_t watts, const void *ctx)
{
    icLogDebug(LOG_TAG, "%s: power %" PRIu16, __FUNCTION__, watts);

    updatePowerResource(eui64, endpointId, watts);
}

static void instantaneousDemandChanged(uint64_t eui64, uint8_t endpointId, int32_t kilowatts, const void *ctx)
{
    updatePowerResource(eui64, endpointId, (int64_t) kilowatts * 1000); // metering cluster reports in kilowatts
}

static bool getWattsFromDeviceWithType(uint64_t eui64, uint8_t endpointId, bool useEm, uint64_t *watts)
{
    bool result = false;

    char *uuid = zigbeeSubsystemEui64ToId(eui64);
    char epName[4]; // max uint8_t + \0
    sprintf(epName, "%" PRIu8, endpointId);

    if (watts != NULL)
    {
        uint64_t power = 0;
        uint64_t divisor = 1;
        uint64_t multiplier = 0;

        if (useEm)
        {
            uint16_t emPower = 0;
            if (electricalMeasurementClusterGetActivePower(eui64, endpointId, &emPower) == false)
            {
                goto exit;
            }

            if (emPower != 0xffff) // an invalid value, so just stick with 0 for now
            {
                power = emPower;
            }
        }
        else
        {
            uint32_t mPower = 0;

            if (meteringClusterGetInstantaneousDemand(eui64, endpointId, &mPower) == false)
            {
                goto exit;
            }

            power = mPower * 1000; // metering cluster reports in kilowatts
        }

        if (!getPowerDivisor(eui64, endpointId, useEm, &divisor))
        {
            icLogError(LOG_TAG, "%s: failed to get divisor", __FUNCTION__);
            goto exit;
        }

        if (!getPowerMultiplier(eui64, endpointId, useEm, &multiplier))
        {
            icLogError(LOG_TAG, "%s: failed to get multiplier", __FUNCTION__);
            goto exit;
        }

        *watts = power * multiplier / divisor;
    }

    result = true;

exit:
    free(uuid);
    return result;
}

// fetch initial resource values related to a light endpoint (not a switch)
static bool fetchInitialLightResourceValues(icDevice *device,
                                            IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                                            uint64_t eui64,
                                            uint8_t endpointId,
                                            uint16_t deviceId,
                                            const char *epName,
                                            icInitialResourceValues *initialResourceValues)
{
    // add the on/off stuff
    bool isOn;
    if (onOffClusterIsOn(eui64, endpointId, &isOn) == false)
    {
        icLogError(LOG_TAG, "%s: failed to read initial on off attribute value", __FUNCTION__);
        return false;
    }

    initialResourceValuesPutEndpointValue(
        initialResourceValues, epName, LIGHT_PROFILE_RESOURCE_IS_ON, isOn ? "true" : "false");


    // add the level stuff
    if (icDiscoveredDeviceDetailsEndpointHasCluster(
            discoveredDeviceDetails, endpointId, LEVEL_CONTROL_CLUSTER_ID, true))
    {
        uint8_t level;
        if (levelControlClusterGetLevel(eui64, endpointId, &level) == false)
        {
            icLogError(LOG_TAG, "%s: failed to read initial level attribute value", __FUNCTION__);
            return false;
        }

        char *levelStr = levelControlClusterGetLevelString(level);
        initialResourceValuesPutEndpointValue(
            initialResourceValues, epName, LIGHT_PROFILE_RESOURCE_CURRENT_LEVEL, levelStr);
        free(levelStr);

        // Whether dimming is enabled for this device
        initialResourceValuesPutEndpointValue(
            initialResourceValues, epName, LIGHT_PROFILE_RESOURCE_IS_DIMMABLE_MODE, "true");
    }

    // add the color stuff
    if (icDiscoveredDeviceDetailsEndpointHasCluster(
            discoveredDeviceDetails, endpointId, COLOR_CONTROL_CLUSTER_ID, true))
    {
        double x;
        double y;

        if (colorControlClusterGetX(eui64, endpointId, &x) == false)
        {
            icLogError(LOG_TAG, "%s: failed to read initial color x attribute value", __FUNCTION__);
            return false;
        }

        if (colorControlClusterGetY(eui64, endpointId, &y) == false)
        {
            icLogError(LOG_TAG, "%s: failed to read initial color y attribute value", __FUNCTION__);
            return false;
        }

        AUTO_CLEAN(free_generic__auto) char *xyStr = getColorString(x, y);

        initialResourceValuesPutEndpointValue(initialResourceValues, epName, LIGHT_PROFILE_RESOURCE_COLOR, xyStr);
    }

    // set up energy measurement if we have it (either ElectricalMeasurement cluster or SimpleMetering)
    bool hasEm = icDiscoveredDeviceDetailsEndpointHasCluster(
        discoveredDeviceDetails, endpointId, ELECTRICAL_MEASUREMENT_CLUSTER_ID, true);
    bool hasSm =
        icDiscoveredDeviceDetailsEndpointHasCluster(discoveredDeviceDetails, endpointId, METERING_CLUSTER_ID, true);

    if (hasEm || hasSm)
    {
        uint64_t watts = 0;
        if (!getWattsFromDeviceWithType(eui64, endpointId, hasEm, &watts))
        {
            icLogError(LOG_TAG, "%s: failed to read initial watts", __FUNCTION__);
            return false;
        }

        char powerStr[20]; // 9223372036854775808 \0
        snprintf(powerStr, 20, "%" PRIu64, watts);
        initialResourceValuesPutEndpointValue(
            initialResourceValues, epName, LIGHT_PROFILE_RESOURCE_CURRENT_POWER, powerStr);
    }
    return true;
}

// register resources related to a light endpoint (not a switch)
static bool registerLightResources(icDevice *device,
                                   IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                                   uint64_t eui64,
                                   uint8_t endpointId,
                                   const char *epName,
                                   icInitialResourceValues *initialResourceValues)
{
    icDeviceEndpoint *endpoint = createEndpoint(device, epName, LIGHT_PROFILE, true);


    bool result = createEndpointResourceIfAvailable(endpoint,
                                                    LIGHT_PROFILE_RESOURCE_IS_ON,
                                                    initialResourceValues,
                                                    RESOURCE_TYPE_BOOLEAN,
                                                    RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_DYNAMIC |
                                                        RESOURCE_MODE_EMIT_EVENTS | RESOURCE_MODE_LAZY_SAVE_NEXT,
                                                    CACHING_POLICY_ALWAYS) != NULL;

    // add the level stuff: optional
    createEndpointResourceIfAvailable(endpoint,
                                      LIGHT_PROFILE_RESOURCE_CURRENT_LEVEL,
                                      initialResourceValues,
                                      RESOURCE_TYPE_LIGHT_LEVEL,
                                      RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_EMIT_EVENTS |
                                          RESOURCE_MODE_LAZY_SAVE_NEXT,
                                      CACHING_POLICY_ALWAYS);

    // Whether dimming is enabled for this device: optional
    createEndpointResourceIfAvailable(endpoint,
                                      LIGHT_PROFILE_RESOURCE_IS_DIMMABLE_MODE,
                                      initialResourceValues,
                                      RESOURCE_TYPE_BOOLEAN,
                                      RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_EMIT_EVENTS,
                                      CACHING_POLICY_ALWAYS);

    // add the color stuff: optional
    createEndpointResourceIfAvailable(endpoint,
                                      LIGHT_PROFILE_RESOURCE_COLOR,
                                      initialResourceValues,
                                      RESOURCE_TYPE_CIE_1931_COLOR,
                                      RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_EMIT_EVENTS |
                                          RESOURCE_MODE_LAZY_SAVE_NEXT,
                                      CACHING_POLICY_ALWAYS);

    // set up energy measurement if we have it (either ElectricalMeasurement cluster or SimpleMetering)
    bool hasEm = icDiscoveredDeviceDetailsEndpointHasCluster(
        discoveredDeviceDetails, endpointId, ELECTRICAL_MEASUREMENT_CLUSTER_ID, true);
    bool hasSm =
        icDiscoveredDeviceDetailsEndpointHasCluster(discoveredDeviceDetails, endpointId, METERING_CLUSTER_ID, true);

    // Power: optional
    if (hasEm || hasSm)
    {
        createEndpointMetadata(
            endpoint, POWER_MEASUREMENT_TYPE_METADATA, hasEm ? POWER_MEASUREMENT_TYPE_EM : POWER_MEASUREMENT_TYPE_SM);
    }

    // power usage: optional
    createEndpointResourceIfAvailable(endpoint,
                                      LIGHT_PROFILE_RESOURCE_CURRENT_POWER,
                                      initialResourceValues,
                                      RESOURCE_TYPE_WATTS,
                                      RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_EMIT_EVENTS |
                                          RESOURCE_MODE_LAZY_SAVE_NEXT,
                                      CACHING_POLICY_ALWAYS);

    zigbeeDriverCommonSetEndpointNumber(endpoint, endpointId);

    return result;
}

// fetch intial resource values related to the switch endpoint
static bool fetchInitialSwitchResourceValues(icDevice *device,
                                             IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                                             uint64_t eui64,
                                             uint8_t endpointId,
                                             uint16_t deviceId,
                                             const char *epName,
                                             icInitialResourceValues *initialResourceValues)
{
    // Currently we have no resources on the switch endpoint
    return true;
}

// register resources related to the switch endpoint
static bool registerSwitchResources(icDevice *device,
                                    IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                                    uint64_t eui64,
                                    uint8_t endpointId,
                                    uint16_t deviceId,
                                    const char *epName,
                                    icInitialResourceValues *initialResourceValues)
{
    icDeviceEndpoint *endpoint = createEndpoint(device, epName, LIGHT_SWITCH_PROFILE, true);

    zigbeeDriverCommonSetEndpointNumber(endpoint, endpointId);

    return true;
}

static bool fetchInitialResourceValues(ZigbeeDriverCommon *ctx,
                                       icDevice *device,
                                       IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                                       icInitialResourceValues *initialResourceValues)
{
    bool result = true;

    icLogDebug(LOG_TAG, "%s: uuid=%s", __FUNCTION__, device->uuid);

    // get the eui64 for the device, which is the uuid
    uint64_t eui64 = zigbeeSubsystemIdToEui64(device->uuid);

    for (uint8_t i = 0; i < discoveredDeviceDetails->numEndpoints; i++)
    {
        uint8_t endpointId = discoveredDeviceDetails->endpointDetails[i].endpointId;
        uint16_t deviceId = discoveredDeviceDetails->endpointDetails[i].appDeviceId;

        char epName[4]; // max uint8_t + \0
        sprintf(epName, "%" PRIu8, endpointId);

        if (isSwitchDevice(deviceId))
        {
            if (!(result = fetchInitialSwitchResourceValues(
                      device, discoveredDeviceDetails, eui64, endpointId, deviceId, epName, initialResourceValues)))
            {
                icLogError(LOG_TAG, "%s: failed to fetch initial switch resource values", __FUNCTION__);
                result = false;
                break;
            }
        }
        else
        {
            if (!(result = fetchInitialLightResourceValues(
                      device, discoveredDeviceDetails, eui64, endpointId, deviceId, epName, initialResourceValues)))
            {
                icLogError(LOG_TAG, "%s: failed to fetch initial light resource values", __FUNCTION__);
                result = false;
                break;
            }
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

    icLogDebug(LOG_TAG, "%s: uuid=%s", __FUNCTION__, device->uuid);

    // get the eui64 for the device, which is the uuid
    uint64_t eui64 = zigbeeSubsystemIdToEui64(device->uuid);

    for (uint8_t i = 0; i < discoveredDeviceDetails->numEndpoints; i++)
    {
        uint8_t endpointId = discoveredDeviceDetails->endpointDetails[i].endpointId;
        uint16_t deviceId = discoveredDeviceDetails->endpointDetails[i].appDeviceId;

        char epName[4]; // max uint8_t + \0
        sprintf(epName, "%" PRIu8, endpointId);

        if (isSwitchDevice(deviceId))
        {
            if (!(result = registerSwitchResources(
                      device, discoveredDeviceDetails, eui64, endpointId, deviceId, epName, initialResourceValues)))
            {
                icLogError(LOG_TAG, "%s: failed to register switch resources", __FUNCTION__);
                result = false;
                break;
            }
        }
        else
        {
            if (!(result = registerLightResources(
                      device, discoveredDeviceDetails, eui64, endpointId, epName, initialResourceValues)))
            {
                icLogError(LOG_TAG, "%s: failed to register light resources", __FUNCTION__);
                result = false;
                break;
            }
        }
    }

    return result;
}

static char *getColorString(double x, double y)
{
    return stringBuilder("%0.6lf,%0.6lf", x, y);
}

/*
 * Parses the color string into allocated x and y. Returns true if successful, false otherwise.
 */
static bool parseColorString(const char *colorString, double *x, double *y)
{
    bool retVal = false;

    // Expected format is "0.123456,0.123456"
    char *delimiter = NULL;
    errno = 0;
    double tempX = strtod(colorString, &delimiter);
    if (*delimiter == ',' && errno == 0)
    {
        double tempY = strtod(delimiter + 1, &delimiter);
        if (*delimiter == '\0' && errno == 0)
        {
            *x = tempX;
            *y = tempY;
            retVal = true;
        }
    }

    return retVal;
}

static const char *mapDeviceIdToProfile(ZigbeeDriverCommon *ctx, uint16_t deviceId)
{
    uint8_t numDeviceIds = sizeof(myDeviceIds) / sizeof(uint16_t);

    for (int i = 0; i < numDeviceIds; ++i)
    {
        if (myDeviceIds[i] == deviceId)
        {
            return LIGHT_PROFILE;
        }
    }

    return NULL;
}

static bool writeEndpointResource(ZigbeeDriverCommon *ctx,
                                  uint32_t endpointNumber,
                                  icDeviceResource *resource,
                                  const char *previousValue,
                                  const char *newValue,
                                  bool *baseDriverUpdatesResource)
{
    bool result = false;

    if (resource == NULL || endpointNumber == 0 || newValue == NULL)
    {
        icLogDebug(LOG_TAG, "writeEndpointResource: invalid arguments");
        return false;
    }

    icLogDebug(LOG_TAG,
               "writeEndpointResource on endpoint %s: id=%s, previousValue=%s, newValue=%s",
               resource->endpointId,
               resource->id,
               previousValue,
               newValue);

    uint64_t eui64 = zigbeeSubsystemIdToEui64(resource->deviceUuid);

    // by default, resources are updated via attribute report.
    *baseDriverUpdatesResource = false;

    if (strcmp(LIGHT_PROFILE_RESOURCE_IS_ON, resource->id) == 0)
    {
        result = onOffClusterSetOn(eui64, (uint8_t) endpointNumber, strcmp(newValue, "true") == 0);
    }
    else if (strcmp(LIGHT_PROFILE_RESOURCE_CURRENT_LEVEL, resource->id) == 0)
    {
        result = levelControlClusterSetLevel(
            eui64, (uint8_t) endpointNumber, levelControlClusterGetLevelFromString(newValue));
    }
    else if (strcmp(LIGHT_PROFILE_RESOURCE_COLOR, resource->id) == 0)
    {
        double x = 0;
        double y = 0;
        if (parseColorString(newValue, &x, &y) == true)
        {
            result = colorControlClusterMoveToColor(eui64, (uint8_t) endpointNumber, x, y);
        }
        else
        {
            icLogError(LOG_TAG, "%s: invalid color coordinates %s", __FUNCTION__, newValue);
        }
    }
    else if (strcmp(LIGHT_PROFILE_RESOURCE_IS_DIMMABLE_MODE, resource->id) == 0)
    {
        // common driver will update the resource in db
        *baseDriverUpdatesResource = true;
        result = true;
    }

    return result;
}

static bool
preConfigureCluster(ZigbeeDriverCommon *ctx, ZigbeeCluster *cluster, DeviceConfigurationContext *deviceConfigContext)
{
    bool configureCluster = true;
    uint16_t deviceId = 0;

    // check if the current endpoint is a switch device, if it is a switch device, then do not
    // configure its clusters as this would bind the switch endpoint to us (device service),
    // breaking the default binding between the switch and light endpoint on the device.
    if (icDiscoveredDeviceDetailsEndpointGetDeviceId(
            deviceConfigContext->discoveredDeviceDetails, deviceConfigContext->endpointId, &deviceId) &&
        isSwitchDevice(deviceId) == true)
    {
        configureCluster = false;
    }
    else if (cluster->clusterId == METERING_CLUSTER_ID)
    {
        bool hasEm = icDiscoveredDeviceDetailsEndpointHasCluster(deviceConfigContext->discoveredDeviceDetails,
                                                                 deviceConfigContext->endpointId,
                                                                 ELECTRICAL_MEASUREMENT_CLUSTER_ID,
                                                                 true);
        // If we have electrical measurement, we don't want to configure/use metering
        if (hasEm)
        {
            configureCluster = false;
        }
    }

    return configureCluster;
}

static void synchronizeDevice(ZigbeeDriverCommon *ctx, icDevice *device, IcDiscoveredDeviceDetails *details)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    uint64_t eui64 = zigbeeSubsystemIdToEui64(device->uuid);

    icLinkedListIterator *it = linkedListIteratorCreate(device->endpoints);
    while (linkedListIteratorHasNext(it) && deviceServiceIsShuttingDown() == false)
    {
        icDeviceEndpoint *endpoint = (icDeviceEndpoint *) linkedListIteratorGetNext(it);
        uint8_t endpointNumber = zigbeeDriverCommonGetEndpointNumber(ctx, endpoint);

        if (icDiscoveredDeviceDetailsEndpointHasCluster(details, endpointNumber, ON_OFF_CLUSTER_ID, true))
        {
            bool isOn;
            if (onOffClusterIsOn(eui64, endpointNumber, &isOn))
            {
                updateResource(device->uuid, endpoint->id, LIGHT_PROFILE_RESOURCE_IS_ON, isOn ? "true" : "false", NULL);
            }
        }

        if (icDiscoveredDeviceDetailsEndpointHasCluster(details, endpointNumber, LEVEL_CONTROL_CLUSTER_ID, true) &&
            deviceServiceIsShuttingDown() == false)
        {
            uint8_t level;
            if (levelControlClusterGetLevel(eui64, endpointNumber, &level))
            {
                char *levelStr = levelControlClusterGetLevelString(level);
                updateResource(device->uuid, endpoint->id, LIGHT_PROFILE_RESOURCE_CURRENT_LEVEL, levelStr, NULL);
                free(levelStr);
            }
        }

        if (icDiscoveredDeviceDetailsEndpointHasCluster(details, endpointNumber, COLOR_CONTROL_CLUSTER_ID, true) &&
            deviceServiceIsShuttingDown() == false)
        {
            double x;
            double y;
            if (colorControlClusterGetX(eui64, endpointNumber, &x) &&
                colorControlClusterGetY(eui64, endpointNumber, &y))
            {
                char *xyStr = getColorString(x, y);
                updateResource(device->uuid, endpoint->id, LIGHT_PROFILE_RESOURCE_COLOR, xyStr, NULL);
                free(xyStr);
            }
        }
    }
    linkedListIteratorDestroy(it);
}

#endif // BARTON_CONFIG_ZIGBEE
