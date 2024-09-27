//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2019 Comcast
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
// Comcast retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------
//
// Created by mkoch201 on 5/16/19.
//

#include <subsystems/zigbee/zigbeeCommonIds.h>
#include <icUtil/array.h>
#include <commonDeviceDefs.h>
#include <icLog/logging.h>
#include <device/deviceModelHelper.h>
#include "deviceDrivers/zigbeeDriverCommon.h"
#include "device-driver/device-driver-manager.h"

#define LOG_TAG "ZigBeePresenceDD"
#define DEVICE_DRIVER_NAME "ZigBeePresenceDD"
#define MY_DC_VERSION      2

#define PRESENCE_DEVICE_ID 0x1080
#define COMM_FAIL_TIMEOUT_SECS "95"

/* ZigbeeDriverCommonCallbacks */
static const char *mapDeviceIdToProfile(ZigbeeDriverCommon *ctx, uint16_t deviceId);
static bool registerResources(ZigbeeDriverCommon *ctx,
                              icDevice *device,
                              IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                              icInitialResourceValues *initialResourceValues);

static const uint16_t myDeviceIds[] = { PRESENCE_DEVICE_ID };

//zigbee device driver registration order matters, so we pick constructor priority carefully
__attribute__ ((constructor (370)))
static void driverRegister(void)
{
    static const ZigbeeDriverCommonCallbacks myHooks = {.mapDeviceIdToProfile = mapDeviceIdToProfile,
                                                        .registerResources = registerResources};

    DeviceDriver *myDriver = zigbeeDriverCommonCreateDeviceDriver(DEVICE_DRIVER_NAME,
                                                                  PRESENCE_DC,
                                                                  MY_DC_VERSION,
                                                                  myDeviceIds,
                                                                  ARRAY_LENGTH(myDeviceIds),
                                                                  RX_MODE_SLEEPY,
                                                                  &myHooks,
                                                                  false);

    // Doesn't need to be in device descriptor, these are special experimental devices
    myDriver->neverReject = true;

    deviceDriverManagerRegisterDriver(myDriver);
}

static const char *mapDeviceIdToProfile(ZigbeeDriverCommon *ctx, uint16_t deviceId)
{
    const char *profile = NULL;

    switch (deviceId)
    {
        case PRESENCE_DEVICE_ID:
            profile = PRESENCE_PROFILE;
            break;
        default:
            break;
    }

    return profile;
}


static bool registerResources(ZigbeeDriverCommon *ctx,
                              icDevice *device,
                              IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                              icInitialResourceValues *initialResourceValues)
{

    bool result = true;

    icLogDebug(LOG_TAG, "%s: uuid=%s", __FUNCTION__, device->uuid);

    // Need an endpoint or the device gets removed
    icDeviceEndpoint *endpoint = createEndpoint(device, "1", PRESENCE_PROFILE, true);

    zigbeeDriverCommonSetEndpointNumber(endpoint, 1);

    const char *tmp = deviceGetMetadata(device, COMMON_DEVICE_METADATA_COMMFAIL_OVERRIDE_SECS);

    if (tmp == NULL)
    {
        createDeviceMetadata(device, COMMON_DEVICE_METADATA_COMMFAIL_OVERRIDE_SECS, COMM_FAIL_TIMEOUT_SECS);
    }

    return result;

}