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

#ifndef ZILKER_DEVICETEMPERATURECONFIGURATIONCLUSTER_H
#define ZILKER_DEVICETEMPERATURECONFIGURATIONCLUSTER_H

#include "zigbeeCluster.h"

typedef enum
{
    deviceTemperatureStatusNormal,
    deviceTemperatureStatusLow,
    deviceTemperatureStatusHigh,
} DeviceTemperatureStatus;

typedef struct
{
    void (*deviceTemperatureStatusChanged)(void *ctx,
                                           uint64_t eui64,
                                           uint8_t endpointId,
                                           DeviceTemperatureStatus status);
} DeviceTemperatureConfigurationClusterCallbacks;

ZigbeeCluster *
deviceTemperatureConfigurationClusterCreate(const DeviceTemperatureConfigurationClusterCallbacks *callbacks,
                                            void *callbackContext);

/**
 * Set whether or not to configure the temperature alarm mask.  By default the temperature alarm mask will not be
 * configured unless explicitly told to.
 * @param deviceConfigurationContext the configuration context
 * @param configure true to configure, false otherwise
 */
void deviceTemperatureConfigurationClusterSetConfigureTemperatureAlarmMask(
    const DeviceConfigurationContext *deviceConfigurationContext,
    bool configure);

#endif // ZILKER_DEVICETEMPERATURECONFIGURATIONCLUSTER_H
