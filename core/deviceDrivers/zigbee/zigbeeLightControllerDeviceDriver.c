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
// Created by tlea on 10/6/2019
//

/*
IcDiscoveredDeviceDetails:
    EUI64: ffff000fe7feea7a
    Device Type: end device
    Power Source: battery
    Manufacturer: Lutron
    Model: Z3-1BRL
    Hardware Version: 0x1
    Firmware Version: 0xbc2
    Number of endpoints: 1
        Endpoint ID: 1
        Profile ID: 0x0104
        Device ID: 0x0820
        Device Version: 1
        Server Cluster IDs:
            0x0000
            Attribute IDs:
            0x0001
            Attribute IDs:
            0x0003
            Attribute IDs:
            0x1000
            Attribute IDs:
            0xfc00
            Attribute IDs:
        Client Cluster IDs:
            0x0003
            Attribute IDs:
            0x0004
            Attribute IDs:
            0x0006
            Attribute IDs:
            0x0008
            Attribute IDs:
            0x0019
            Attribute IDs:
            0x1000
            Attribute IDs:
*/


#include "device-driver/device-driver-manager.h"
#include "deviceDrivers/zigbeeDriverCommon.h"
#include <commonDeviceDefs.h>
#include <device/deviceModelHelper.h>
#include <icConcurrent/delayedTask.h>
#include <icLog/logging.h>
#include <icUtil/stringUtils.h>
#include <memory.h>
#include <resourceTypes.h>
#include <stdio.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>
#include <zigbeeClusters/doorLockCluster.h>

#define LOG_TAG           "zigbeeLightControllerDD"
#define DRIVER_NAME       "zigbeeLightController"
#define DEVICE_CLASS_NAME "lightController"
#define MY_DC_VERSION     1

#ifdef BARTON_CONFIG_ZIGBEE

static uint16_t myDeviceIds[] = {LIGHT_CONTROLLER_DEVICE_ID};

static const char *mapDeviceIdToProfile(ZigbeeDriverCommon *ctx, uint16_t deviceId);

static bool registerResources(ZigbeeDriverCommon *ctx,
                              icDevice *device,
                              IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                              icInitialResourceValues *values);

static bool writeEndpointResource(ZigbeeDriverCommon *ctx,
                                  uint32_t endpointNumber,
                                  icDeviceResource *resource,
                                  const char *previousValue,
                                  const char *newValue,
                                  bool *baseDriverUpdatesResource);

static const ZigbeeDriverCommonCallbacks commonCallbacks = {
    .registerResources = registerResources,
    .mapDeviceIdToProfile = mapDeviceIdToProfile,
    .writeEndpointResource = writeEndpointResource,
};

// zigbee device driver registration order matters, so we pick constructor priority carefully
__attribute__((constructor(380))) static void driverRegister(void)
{
    DeviceDriver *myDriver = zigbeeDriverCommonCreateDeviceDriver(DRIVER_NAME,
                                                                  DEVICE_CLASS_NAME,
                                                                  MY_DC_VERSION,
                                                                  myDeviceIds,
                                                                  sizeof(myDeviceIds) / sizeof(uint16_t),
                                                                  RX_MODE_NON_SLEEPY,
                                                                  &commonCallbacks,
                                                                  true);

    zigbeeDriverCommonSkipConfiguration(myDriver); // dont discover or configure this thing

    // enable periodic collection of common diagnostics info (rssi, lqi, etc).
    zigbeeDriverCommonSetDiagnosticsCollectionEnabled(myDriver, true);

    deviceDriverManagerRegisterDriver(myDriver);
}

static bool registerResources(ZigbeeDriverCommon *ctx,
                              icDevice *device,
                              IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                              icInitialResourceValues *values)
{
    bool result = true;

    icLogDebug(LOG_TAG, "%s: uuid=%s", __FUNCTION__, device->uuid);

    for (uint8_t i = 0; i < discoveredDeviceDetails->numEndpoints; i++)
    {
        uint8_t endpointId = discoveredDeviceDetails->endpointDetails[i].endpointId;

        char epName[4]; // max uint8_t + \0
        sprintf(epName, "%" PRIu8, endpointId);

        icDeviceEndpoint *endpoint = createEndpoint(device, epName, LIGHTCONTROLLER_PROFILE, true);

        result &= createEndpointResource(endpoint,
                                         LIGHTCONTROLLER_PROFILE_RESOURCE_BOUND_ENDPOINT_URI,
                                         NULL,
                                         RESOURCE_TYPE_ENDPOINT_URI,
                                         RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_EMIT_EVENTS,
                                         CACHING_POLICY_ALWAYS) != NULL;

        zigbeeDriverCommonSetEndpointNumber(endpoint, endpointId);
    }

    return result;
}

static bool writeEndpointResource(ZigbeeDriverCommon *ctx,
                                  uint32_t endpointNumber,
                                  icDeviceResource *resource,
                                  const char *previousValue,
                                  const char *newValue,
                                  bool *baseDriverUpdatesResource)
{
    bool result = false;

    (void) baseDriverUpdatesResource; // unused

    if (resource == NULL || endpointNumber == 0 || newValue == NULL)
    {
        icLogDebug(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return false;
    }

    icLogDebug(LOG_TAG,
               "%s: endpoint %s: id=%s, previousValue=%s, newValue=%s",
               __FUNCTION__,
               resource->endpointId,
               resource->id,
               previousValue,
               newValue);

    if (strcmp(LIGHTCONTROLLER_PROFILE_RESOURCE_BOUND_ENDPOINT_URI, resource->id) == 0)
    {
        uint64_t controllerEui64 = zigbeeSubsystemIdToEui64(resource->deviceUuid);
        char *deviceId = (char *) calloc(1, strlen(newValue));   // allocate worst case
        char *endpointId = (char *) calloc(1, strlen(newValue)); // allocate worst case

        int numParsed = sscanf(newValue, "/%[^/]/ep/%[^/]", deviceId, endpointId);
        if (numParsed == 2)
        {
            uint64_t targetEui64 = zigbeeSubsystemIdToEui64(deviceId);
            uint8_t targetEndpoint;
            if (stringToUint8(endpointId, &targetEndpoint) == true)
            {
                result =
                    zhalBindingSetTarget(
                        controllerEui64, endpointNumber, targetEui64, targetEndpoint, LEVEL_CONTROL_CLUSTER_ID) == 0;
            }
        }

        free(deviceId);
        free(endpointId);
    }

    return result;
}

static const char *mapDeviceIdToProfile(ZigbeeDriverCommon *ctx, uint16_t deviceId)
{
    return LIGHTCONTROLLER_PROFILE;
}

#endif // BARTON_CONFIG_ZIGBEE
