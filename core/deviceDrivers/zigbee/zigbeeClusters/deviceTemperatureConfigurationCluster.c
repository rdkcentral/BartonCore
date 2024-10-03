//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2020 Comcast Corporation
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
// Comcast Corporation retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------

#include <commonDeviceDefs.h>
#include <icLog/logging.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <subsystems/zigbee/zigbeeAttributeTypes.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>
#include <subsystems/zigbee/zigbeeIO.h>
#include <subsystems/zigbee/zigbeeSubsystem.h>

#ifdef BARTON_CONFIG_ZIGBEE

#include "zigbeeClusters/deviceTemperatureConfigurationCluster.h"

#define LOG_TAG                              "deviceTemperatureConfigCluster"

// Alarm codes
#define DEVICE_TEMPERATURE_TOO_LOW           0x00
#define DEVICE_TEMPERATURE_TOO_HIGH          0x01

#define CONFIGURE_TEMPERATURE_ALARM_MASK_KEY "deviceTemperatureConfigurationConfigureTemperatureAlarmMask"

typedef struct
{
    ZigbeeCluster cluster;
    const DeviceTemperatureConfigurationClusterCallbacks *callbacks;
    void *callbackContext;
} DeviceTemperatureConfigurationCluster;

static bool configureCluster(ZigbeeCluster *ctx, const DeviceConfigurationContext *configContext);

static bool
handleAlarm(ZigbeeCluster *ctx, uint64_t eui64, uint8_t endpointId, const ZigbeeAlarmTableEntry *alarmTableEntry);

static bool handleAlarmCleared(ZigbeeCluster *ctx,
                               uint64_t eui64,
                               uint8_t endpointId,
                               const ZigbeeAlarmTableEntry *alarmTableEntry);

ZigbeeCluster *
deviceTemperatureConfigurationClusterCreate(const DeviceTemperatureConfigurationClusterCallbacks *callbacks,
                                            void *callbackContext)
{
    DeviceTemperatureConfigurationCluster *result =
        (DeviceTemperatureConfigurationCluster *) calloc(1, sizeof(DeviceTemperatureConfigurationCluster));

    result->cluster.clusterId = DEVICE_TEMPERATURE_CONFIGURATION_CLUSTER_ID;

    result->cluster.configureCluster = configureCluster;
    result->cluster.handleAlarm = handleAlarm;
    result->cluster.handleAlarmCleared = handleAlarmCleared;

    result->callbacks = callbacks;
    result->callbackContext = callbackContext;

    return (ZigbeeCluster *) result;
}

void deviceTemperatureConfigurationClusterSetConfigureTemperatureAlarmMask(
    const DeviceConfigurationContext *deviceConfigurationContext,
    bool configure)
{
    addBoolConfigurationMetadata(
        deviceConfigurationContext->configurationMetadata, CONFIGURE_TEMPERATURE_ALARM_MASK_KEY, configure);
}

static bool configureCluster(ZigbeeCluster *ctx, const DeviceConfigurationContext *configContext)
{
    bool result = true;

    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    // Check whether to configure the temperature alarm mask, default to false
    if (icDiscoveredDeviceDetailsClusterHasAttribute(configContext->discoveredDeviceDetails,
                                                     configContext->endpointId,
                                                     DEVICE_TEMPERATURE_CONFIGURATION_CLUSTER_ID,
                                                     true,
                                                     DEVICE_TEMPERATURE_ALARM_MASK_ATTRIBUTE_ID) == true)
    {
        if (getBoolConfigurationMetadata(
                configContext->configurationMetadata, CONFIGURE_TEMPERATURE_ALARM_MASK_KEY, false))
        {
            if (zigbeeSubsystemWriteNumber(configContext->eui64,
                                           configContext->endpointId,
                                           DEVICE_TEMPERATURE_CONFIGURATION_CLUSTER_ID,
                                           true,
                                           DEVICE_TEMPERATURE_ALARM_MASK_ATTRIBUTE_ID,
                                           ZCL_BITMAP8_ATTRIBUTE_TYPE,
                                           0x01,
                                           1) != 0)
            {
                icLogError(LOG_TAG, "%s: failed to set temperature alarm mask", __FUNCTION__);
                result = false;
            }
        }
    }
    return result;
}

static bool
handleAlarm(ZigbeeCluster *ctx, uint64_t eui64, uint8_t endpointId, const ZigbeeAlarmTableEntry *alarmTableEntry)
{
    DeviceTemperatureConfigurationCluster *cluster = (DeviceTemperatureConfigurationCluster *) ctx;
    bool result = true;

    icLogDebug(LOG_TAG,
               "%s: %" PRIx64 " ep %" PRIu8 " alarmCode 0x%.2" PRIx8,
               __FUNCTION__,
               eui64,
               endpointId,
               alarmTableEntry->alarmCode);

    switch (alarmTableEntry->alarmCode)
    {
        case DEVICE_TEMPERATURE_TOO_LOW:
            icLogWarn(LOG_TAG, "%s: device temperature too low", __FUNCTION__);
            if (cluster->callbacks->deviceTemperatureStatusChanged != NULL)
            {
                cluster->callbacks->deviceTemperatureStatusChanged(
                    cluster->callbackContext, eui64, endpointId, deviceTemperatureStatusLow);
            }
            break;

        case DEVICE_TEMPERATURE_TOO_HIGH:
            icLogWarn(LOG_TAG, "%s: device temperature too high", __FUNCTION__);
            if (cluster->callbacks->deviceTemperatureStatusChanged != NULL)
            {
                cluster->callbacks->deviceTemperatureStatusChanged(
                    cluster->callbackContext, eui64, endpointId, deviceTemperatureStatusHigh);
            }
            break;

        default:
            icLogWarn(LOG_TAG,
                      "%s: Unsupported device temperature configuration alarm code 0x%02x",
                      __FUNCTION__,
                      alarmTableEntry->alarmCode);
            result = false;
            break;
    }

    return result;
}

static bool
handleAlarmCleared(ZigbeeCluster *ctx, uint64_t eui64, uint8_t endpointId, const ZigbeeAlarmTableEntry *alarmTableEntry)
{
    DeviceTemperatureConfigurationCluster *cluster = (DeviceTemperatureConfigurationCluster *) ctx;
    bool result = true;

    icLogDebug(LOG_TAG,
               "%s: %" PRIx64 " ep %" PRIu8 " alarmCode 0x%.2" PRIx8,
               __FUNCTION__,
               eui64,
               endpointId,
               alarmTableEntry->alarmCode);

    switch (alarmTableEntry->alarmCode)
    {
        case DEVICE_TEMPERATURE_TOO_LOW:
        case DEVICE_TEMPERATURE_TOO_HIGH:
            icLogWarn(LOG_TAG, "%s: device temperature is normal", __FUNCTION__);
            if (cluster->callbacks->deviceTemperatureStatusChanged != NULL)
            {
                cluster->callbacks->deviceTemperatureStatusChanged(
                    cluster->callbackContext, eui64, endpointId, deviceTemperatureStatusNormal);
            }
            break;

        default:
            icLogWarn(LOG_TAG,
                      "%s: Unsupported device temperature configuration alarm code 0x%02x",
                      __FUNCTION__,
                      alarmTableEntry->alarmCode);
            result = false;
            break;
    }

    return result;
}

#endif // BARTON_CONFIG_ZIGBEE
