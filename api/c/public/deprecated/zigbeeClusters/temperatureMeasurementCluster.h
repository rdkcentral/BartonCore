//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2019 Comcast Corporation
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

//
// Created by tlea on 2/18/19.
//

#ifndef ZILKER_TEMPERATUREMEASUREMENTCLUSTER_H
#define ZILKER_TEMPERATUREMEASUREMENTCLUSTER_H

#include "zigbeeCluster.h"

#define TEMPERATURE_MEASUREMENT_CLUSTER_INVALID_TEMPERATURE_VALUE  0x8000

typedef struct
{
    void (*measuredValueUpdated)(void *ctx, uint64_t eui64, uint8_t endpointId, int16_t value);
} TemperatureMeasurementClusterCallbacks;

ZigbeeCluster *temperatureMeasurementClusterCreate(const TemperatureMeasurementClusterCallbacks *callbacks, void *callbackContext);

bool temperatureMeasurementClusterGetMeasuredValue(uint64_t eui64, uint8_t endpointId, int16_t *value);

/**
 * Set whether or not to configure temperature reporting.  By default temperature reporting is not configured
 * @param deviceConfigurationContext the configuration context
 * @param configure true to configure, false otherwise
 */
void temperatureMeasurementSetTemperatureReporting(const DeviceConfigurationContext *deviceConfigurationContext,
                                                   bool configure);

/**
 * Helper to check for valid temperature value.
 *
 * @param value temperature value to check
 * @return true if value is valid
 */

inline bool temperatureMeasurementClusterIsTemperatureValid(uint16_t value)
{
    return (value != TEMPERATURE_MEASUREMENT_CLUSTER_INVALID_TEMPERATURE_VALUE);
}

#endif //ZILKER_TEMPERATUREMEASUREMENTCLUSTER_H
