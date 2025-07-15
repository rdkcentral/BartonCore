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
// Created by Thomas Lea on 4/17/25.
//

#include <math.h>
#define LOG_TAG "illuminanceMeasurementCluster"
#define logFmt(fmt) "(%s): " fmt, __func__

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

#include "zigbeeClusters/illuminanceMeasurementCluster.h"


typedef struct
{
    ZigbeeCluster cluster;

    const IlluminanceMeasurementClusterCallbacks *callbacks;
    void *callbackContext;
} IlluminanceMeasurementCluster;

static bool configureCluster(ZigbeeCluster *ctx, const DeviceConfigurationContext *configContext);
static bool handleAttributeReport(ZigbeeCluster *ctx, ReceivedAttributeReport *report);

ZigbeeCluster *illuminanceMeasurementClusterCreate(const IlluminanceMeasurementClusterCallbacks *callbacks,
                                                   void *callbackContext)
{
    IlluminanceMeasurementCluster *result =
        (IlluminanceMeasurementCluster *) calloc(1, sizeof(IlluminanceMeasurementCluster));

    result->cluster.clusterId = ILLUMINANCE_MEASUREMENT_CLUSTER_ID;

    result->cluster.configureCluster = configureCluster;
    result->cluster.handleAttributeReport = handleAttributeReport;

    result->callbacks = callbacks;
    result->callbackContext = callbackContext;

    return (ZigbeeCluster *) result;
}

bool illuminanceMeasurementClusterGetMeasuredValue(uint64_t eui64, uint8_t endpointId, uint16_t *value)
{
    bool result = false;

    if (value == NULL)
    {
        icError("invalid arguments");
        return false;
    }

    uint64_t val;
    if (zigbeeSubsystemReadNumber(
            eui64, endpointId, ILLUMINANCE_MEASUREMENT_CLUSTER_ID, true, ILLUMINANCE_MEASURED_VALUE_ATTRIBUTE_ID, &val) == 0)
    {
        *value = (uint16_t) (val & 0xFFFF);
        result = true;
    }
    else
    {
        icError("failed to read measured value");
    }

    return result;
}

uint32_t illuminanceMeasurementClusterConvertToLux(uint16_t measuredValue)
{
    // 0 is a special case, it means "too low to measure"
    if (measuredValue == 0)
    {
        return 0;
    }
    // From Zigbee Cluster Library Specification:
    // MeasuredValue = 10,000 x log10 Illuminance + 1
    // Therefore: Illuminance = 10 ^ ( (MeasuredValue - 1) / 10,000 )
    return pow(10.0, ((double)measuredValue-1.0)/10000.0);
}

static bool configureCluster(ZigbeeCluster *ctx, const DeviceConfigurationContext *configContext)
{
    bool result = true;

    // configure attribute reporting on measured value
    zhalAttributeReportingConfig valueConfigs[1];
    uint8_t numConfigs = 1;
    memset(&valueConfigs[0], 0, sizeof(zhalAttributeReportingConfig));
    valueConfigs[0].attributeInfo.id = ILLUMINANCE_MEASURED_VALUE_ATTRIBUTE_ID;
    valueConfigs[0].attributeInfo.type = ZCL_INT16U_ATTRIBUTE_TYPE;
    valueConfigs[0].minInterval = 1;
    valueConfigs[0].maxInterval = TWENTY_SEVEN_MINUTES_IN_SECS;
    valueConfigs[0].reportableChange = 50; //TODO this should be configurable

    if (zigbeeSubsystemBindingSet(
            configContext->eui64, configContext->endpointId, ILLUMINANCE_MEASUREMENT_CLUSTER_ID) != 0)
    {
        icError("failed to bind illuminance measurement");
        result = false;
    }
    else if (zigbeeSubsystemAttributesSetReporting(configContext->eui64,
                                                   configContext->endpointId,
                                                   ILLUMINANCE_MEASUREMENT_CLUSTER_ID,
                                                   valueConfigs,
                                                   numConfigs) != 0)
    {
        icError("failed to set reporting for measured illuminance attribute");
        result = false;
    }

    return result;
}

static bool handleAttributeReport(ZigbeeCluster *ctx, ReceivedAttributeReport *report)
{
    icDebug();

    IlluminanceMeasurementCluster *cluster = (IlluminanceMeasurementCluster *) ctx;

    sbZigbeeIOContext *zio = zigbeeIOInit(report->reportData, report->reportDataLen, ZIO_READ);
    uint16_t attributeId = zigbeeIOGetUint16(zio);
    // Pull out the type and throw it away
    zigbeeIOGetUint8(zio);

    if (attributeId == ILLUMINANCE_MEASURED_VALUE_ATTRIBUTE_ID)
    {
        if (cluster->callbacks->measuredValueUpdated != NULL)
        {
            uint16_t measuredValue = zigbeeIOGetUint16(zio);
            if (illuminanceMeasurementClusterIsValueValid(measuredValue) == true)
            {
                icDebug("measuredValueUpdated=%" PRIu16, measuredValue);
                cluster->callbacks->measuredValueUpdated(
                    cluster->callbackContext, report->eui64, report->sourceEndpoint, measuredValue);
            }
        }
    }

    return true;
}

#endif // BARTON_CONFIG_ZIGBEE
