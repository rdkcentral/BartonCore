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

#include <stdlib.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>
#include <icLog/logging.h>
#include <memory.h>
#include <subsystems/zigbee/zigbeeAttributeTypes.h>
#include <subsystems/zigbee/zigbeeSubsystem.h>
#include <stdio.h>
#include <commonDeviceDefs.h>
#include <subsystems/zigbee/zigbeeIO.h>

#ifdef BARTON_CONFIG_ZIGBEE

#include "zigbeeClusters/temperatureMeasurementCluster.h"

#define LOG_TAG "temperatureMeasurementCluster"

#define TEMPERATURE_REPORTING_KEY "temperatureMeasurementReporting"

typedef struct
{
    ZigbeeCluster cluster;

    const TemperatureMeasurementClusterCallbacks *callbacks;
    void *callbackContext;
} TemperatureMeasurementCluster;

static void handlePollControlCheckin(ZigbeeCluster *ctx, uint64_t eui64, uint8_t endpointId);
static bool configureCluster(ZigbeeCluster *ctx, const DeviceConfigurationContext *configContext);
static bool handleAttributeReport(ZigbeeCluster *ctx, ReceivedAttributeReport *report);

extern inline bool temperatureMeasurementClusterIsTemperatureValid(uint16_t value);

ZigbeeCluster *temperatureMeasurementClusterCreate(const TemperatureMeasurementClusterCallbacks *callbacks,
                                                   void *callbackContext)
{
    TemperatureMeasurementCluster *result = (TemperatureMeasurementCluster *) calloc(1,
                                                                                     sizeof(TemperatureMeasurementCluster));

    result->cluster.clusterId = TEMPERATURE_MEASUREMENT_CLUSTER_ID;

    result->cluster.handlePollControlCheckin = handlePollControlCheckin;
    result->cluster.configureCluster = configureCluster;
    result->cluster.handleAttributeReport = handleAttributeReport;

    result->callbacks = callbacks;
    result->callbackContext = callbackContext;

    return (ZigbeeCluster *) result;
}

bool temperatureMeasurementClusterGetMeasuredValue(uint64_t eui64, uint8_t endpointId, int16_t *value)
{
    bool result = false;

    if (value == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return false;
    }

    uint64_t val;
    if (zigbeeSubsystemReadNumber(eui64,
                                  endpointId,
                                  TEMPERATURE_MEASUREMENT_CLUSTER_ID,
                                  true,
                                  TEMP_MEASURED_VALUE_ATTRIBUTE_ID,
                                  &val) == 0)
    {
        *value = (int16_t) (val & 0xFFFF);
        result = true;
    }
    else
    {
        icLogError(LOG_TAG, "%s: failed to read measured value", __FUNCTION__);
    }

    return result;
}

void temperatureMeasurementSetTemperatureReporting(const DeviceConfigurationContext *deviceConfigurationContext,
                                                   bool configure)
{
    addBoolConfigurationMetadata(deviceConfigurationContext->configurationMetadata, TEMPERATURE_REPORTING_KEY, configure);
}

static void handlePollControlCheckin(ZigbeeCluster *ctx, uint64_t eui64, uint8_t endpointId)
{
    TemperatureMeasurementCluster *cluster = (TemperatureMeasurementCluster *) ctx;

    if (cluster->callbacks->measuredValueUpdated != NULL)
    {
        uint16_t value;
        if (temperatureMeasurementClusterGetMeasuredValue(eui64, endpointId, &value))
        {
            if (temperatureMeasurementClusterIsTemperatureValid(value) == true)
            {
                cluster->callbacks->measuredValueUpdated(cluster->callbackContext, eui64, endpointId, value);
            }
        }
    }
}

static bool configureCluster(ZigbeeCluster *ctx, const DeviceConfigurationContext *configContext)
{
    bool result = true;
    if (getBoolConfigurationMetadata(configContext->configurationMetadata, TEMPERATURE_REPORTING_KEY, false))
    {
        // configure attribute reporting on battery state
        zhalAttributeReportingConfig temperatureStateConfigs[1];
        uint8_t numConfigs = 1;
        memset(&temperatureStateConfigs[0], 0, sizeof(zhalAttributeReportingConfig));
        temperatureStateConfigs[0].attributeInfo.id = TEMP_MEASURED_VALUE_ATTRIBUTE_ID;
        temperatureStateConfigs[0].attributeInfo.type = ZCL_INT16S_ATTRIBUTE_TYPE;
        temperatureStateConfigs[0].minInterval = 1;
        temperatureStateConfigs[0].maxInterval = 1620; // 27 minutes
        temperatureStateConfigs[0].reportableChange = 50;

        if (zigbeeSubsystemBindingSet(configContext->eui64, configContext->endpointId, TEMPERATURE_MEASUREMENT_CLUSTER_ID) != 0)
        {
            icLogError(LOG_TAG, "%s: failed to bind temperature measurement", __FUNCTION__);
            result = false;
        }
        else if (zigbeeSubsystemAttributesSetReporting(configContext->eui64,
                                                       configContext->endpointId,
                                                       TEMPERATURE_MEASUREMENT_CLUSTER_ID,
                                                       temperatureStateConfigs,
                                                       numConfigs) != 0)
        {
            icLogError(LOG_TAG, "%s: failed to set reporting for measured temperature attribute", __FUNCTION__);
            result = false;
        }
    }

    return result;
}

static bool handleAttributeReport(ZigbeeCluster *ctx, ReceivedAttributeReport *report)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    TemperatureMeasurementCluster *cluster = (TemperatureMeasurementCluster *) ctx;

    sbZigbeeIOContext *zio = zigbeeIOInit(report->reportData, report->reportDataLen, ZIO_READ);
    uint16_t attributeId = zigbeeIOGetUint16(zio);
    // Pull out the type and throw it away
    zigbeeIOGetUint8(zio);

    if (attributeId == TEMP_MEASURED_VALUE_ATTRIBUTE_ID)
    {
        if (cluster->callbacks->measuredValueUpdated != NULL)
        {
            int16_t measuredTempValue = zigbeeIOGetInt16(zio);
            if (temperatureMeasurementClusterIsTemperatureValid(measuredTempValue) == true)
            {
                icLogDebug(LOG_TAG, "%s: measuredValueUpdated=%"PRId16, __FUNCTION__, measuredTempValue);
                cluster->callbacks->measuredValueUpdated(cluster->callbackContext,
                                                         report->eui64,
                                                         report->sourceEndpoint,
                                                         measuredTempValue);
            }
        }
    }

    return true;
}

#endif //BARTON_CONFIG_ZIGBEE