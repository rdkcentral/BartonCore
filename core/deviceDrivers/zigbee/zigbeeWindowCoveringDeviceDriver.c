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

/*
 * This is a device driver that supports the 'window covering' device class
 *
 * Created by Thomas Lea on 10/19/2018
 */

#include "device/deviceModelHelper.h"
#include "deviceDrivers/zigbeeDriverCommon.h"
#include <commonDeviceDefs.h>
#include <device-driver/device-driver-manager.h>
#include <device-driver/device-driver.h>
#include <device/icDevice.h>
#include <device/icDeviceResource.h>
#include <errno.h>
#include <icLog/logging.h>
#include <inttypes.h>
#include <limits.h>
#include <resourceTypes.h>
#include <stdio.h>
#include <string.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>
#include <subsystems/zigbee/zigbeeSubsystem.h>
#include <zhal/zhal.h>

#define LOG_TAG             "ZigBeeWindowCDD"
#define DEVICE_DRIVER_NAME  "ZigBeeWindowCDD"
#define DEVICE_CLASS_NAME   "windowCovering"
#define DEVICE_PROFILE_NAME "windowCovering"

// only compile if we support zigbee
#ifdef BARTON_CONFIG_ZIGBEE

#define MY_DC_VERSION 1

static bool registerResources(ZigbeeDriverCommon *ctx,
                              icDevice *device,
                              IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                              icInitialResourceValues *initialResourceValues);

static bool executeEndpointResource(ZigbeeDriverCommon *ctx,
                                    uint32_t endpointNumber,
                                    icDeviceResource *resource,
                                    const char *arg,
                                    char **response);

static const char *mapDeviceIdToProfile(ZigbeeDriverCommon *ctx, uint16_t deviceId);

static uint16_t myDeviceIds[] = {WINDOW_COVERING_DEVICE_ID};

// zigbee device driver registration order matters, so we pick constructor priority carefully
__attribute__((constructor(390))) static void driverRegister(void)
{
    static const ZigbeeDriverCommonCallbacks myHooks = {.mapDeviceIdToProfile = mapDeviceIdToProfile,
                                                        .registerResources = registerResources,
                                                        .executeEndpointResource = executeEndpointResource};

    DeviceDriver *myDriver = zigbeeDriverCommonCreateDeviceDriver(DEVICE_DRIVER_NAME,
                                                                  DEVICE_CLASS_NAME,
                                                                  MY_DC_VERSION,
                                                                  myDeviceIds,
                                                                  ARRAY_LENGTH(myDeviceIds),
                                                                  RX_MODE_NON_SLEEPY,
                                                                  &myHooks,
                                                                  false);

    // Doesn't need to be in device descriptor, these are special experimental devices
    myDriver->neverReject = true;

    deviceDriverManagerRegisterDriver(myDriver);
}

static bool registerResources(ZigbeeDriverCommon *ctx,
                              icDevice *device,
                              IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                              icInitialResourceValues *initialResourceValues)
{
    bool result = true;

    icLogDebug(LOG_TAG, "%s: uuid=%s", __FUNCTION__, device->uuid);

    for (uint8_t i = 0; i < discoveredDeviceDetails->numEndpoints; i++)
    {
        uint8_t endpointId = discoveredDeviceDetails->endpointDetails[i].endpointId;
        uint16_t deviceId = discoveredDeviceDetails->endpointDetails[i].appDeviceId;

        char epName[4]; // max uint8_t + \0
        sprintf(epName, "%" PRIu8, endpointId);

        if (deviceId == WINDOW_COVERING_DEVICE_ID)
        {
            icDeviceEndpoint *endpoint = createEndpoint(device, epName, WINDOW_COVERING_PROFILE, true);

            createEndpointResource(endpoint,
                                   WINDOW_COVERING_FUNCTION_UP,
                                   NULL,
                                   RESOURCE_TYPE_MOVE_UP_OPERATION,
                                   RESOURCE_MODE_EXECUTABLE,
                                   CACHING_POLICY_NEVER);

            createEndpointResource(endpoint,
                                   WINDOW_COVERING_FUNCTION_DOWN,
                                   NULL,
                                   RESOURCE_TYPE_MOVE_DOWN_OPERATION,
                                   RESOURCE_MODE_EXECUTABLE,
                                   CACHING_POLICY_NEVER);

            createEndpointResource(endpoint,
                                   WINDOW_COVERING_FUNCTION_STOP,
                                   NULL,
                                   RESOURCE_TYPE_STOP_OPERATION,
                                   RESOURCE_MODE_EXECUTABLE,
                                   CACHING_POLICY_NEVER);

            zigbeeDriverCommonSetEndpointNumber(endpoint, endpointId);
        }
    }

    return result;
}

static bool executeEndpointResource(ZigbeeDriverCommon *ctx,
                                    uint32_t endpointNumber,
                                    icDeviceResource *resource,
                                    const char *arg,
                                    char **response)
{
    bool result = true;

    if (resource == NULL || endpointNumber == 0)
    {
        icLogDebug(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return false;
    }

    uint64_t eui64 = zigbeeSubsystemIdToEui64(resource->deviceUuid);

    uint8_t commandId = 0xff; // invalid
    if (strcmp(resource->id, WINDOW_COVERING_FUNCTION_UP) == 0)
    {
        commandId = WINDOW_COVERING_UP_COMMAND_ID;
    }
    else if (strcmp(resource->id, WINDOW_COVERING_FUNCTION_DOWN) == 0)
    {
        commandId = WINDOW_COVERING_DOWN_COMMAND_ID;
    }
    else if (strcmp(resource->id, WINDOW_COVERING_FUNCTION_STOP) == 0)
    {
        commandId = WINDOW_COVERING_STOP_COMMAND_ID;
    }

    zigbeeSubsystemSendCommand(eui64, endpointNumber, WINDOW_COVERING_CLUSTER_ID, true, commandId, NULL, 0);

    return result;
}

static const char *mapDeviceIdToProfile(ZigbeeDriverCommon *ctx, uint16_t deviceId)
{
    const char *profile = NULL;

    switch (deviceId)
    {
        case WINDOW_COVERING_DEVICE_ID:
            profile = WINDOW_COVERING_PROFILE;
            break;
        default:
            break;
    }

    return profile;
}

#endif // BARTON_CONFIG_ZIGBEE
