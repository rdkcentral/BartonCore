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
// Created by tlea on 2/18/19.
//

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

#include "zigbeeClusters/temperatureMeasurementCluster.h"

#define LOG_TAG                   "temperatureMeasurementCluster"

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
    TemperatureMeasurementCluster *result =
        (TemperatureMeasurementCluster *) calloc(1, sizeof(TemperatureMeasurementCluster));

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
    if (zigbeeSubsystemReadNumber(
            eui64, endpointId, TEMPERATURE_MEASUREMENT_CLUSTER_ID, true, TEMP_MEASURED_VALUE_ATTRIBUTE_ID, &val) == 0)
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
    addBoolConfigurationMetadata(
        deviceConfigurationContext->configurationMetadata, TEMPERATURE_REPORTING_KEY, configure);
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

        if (zigbeeSubsystemBindingSet(
                configContext->eui64, configContext->endpointId, TEMPERATURE_MEASUREMENT_CLUSTER_ID) != 0)
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
                icLogDebug(LOG_TAG, "%s: measuredValueUpdated=%" PRId16, __FUNCTION__, measuredTempValue);
                cluster->callbacks->measuredValueUpdated(
                    cluster->callbackContext, report->eui64, report->sourceEndpoint, measuredTempValue);
            }
        }
    }

    return true;
}

#endif // BARTON_CONFIG_ZIGBEE
