//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2015 iControl Networks, Inc.
//
// All rights reserved.
//
// This software is protected by copyright laws of the United States
// and of foreign countries. This material may also be protected by
// patent laws of the United States and of foreign countries.
//
// This software is furnished under a license agreement and/or a
// nondisclosure agreement and may only be used or copied in accordance
// with the terms of those agreements.
//
// The mere transfer of this software does not imply any licenses of trade
// secrets, proprietary technology, copyrights, patents, trademarks, or
// any other form of intellectual property whatsoever.
//
// iControl Networks retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------

/*
 * This is a device driver that supports the 'window covering' device class
 *
 * Created by Thomas Lea on 10/19/2018
 */

#include <device-driver/device-driver.h>
#include <device-driver/device-driver-manager.h>
#include <icLog/logging.h>
#include <string.h>
#include <commonDeviceDefs.h>
#include <subsystems/zigbee/zigbeeSubsystem.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>
#include <resourceTypes.h>
#include <inttypes.h>
#include <stdio.h>
#include <limits.h>
#include <zhal/zhal.h>
#include <errno.h>
#include <device/icDeviceResource.h>
#include <device/icDevice.h>
#include "device/deviceModelHelper.h"
#include "deviceDrivers/zigbeeDriverCommon.h"

#define LOG_TAG "ZigBeeWindowCDD"
#define DEVICE_DRIVER_NAME "ZigBeeWindowCDD"
#define DEVICE_CLASS_NAME "windowCovering"
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

//zigbee device driver registration order matters, so we pick constructor priority carefully
__attribute__ ((constructor (390)))
static void driverRegister(void)
{
    static const ZigbeeDriverCommonCallbacks myHooks = {
            .mapDeviceIdToProfile = mapDeviceIdToProfile,
            .registerResources = registerResources,
            .executeEndpointResource = executeEndpointResource
    };

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

        char epName[4]; //max uint8_t + \0
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

    uint8_t commandId = 0xff; //invalid
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

    zigbeeSubsystemSendCommand(eui64,
                               endpointNumber,
                               WINDOW_COVERING_CLUSTER_ID,
                               true,
                               commandId,
                               NULL,
                               0);

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
