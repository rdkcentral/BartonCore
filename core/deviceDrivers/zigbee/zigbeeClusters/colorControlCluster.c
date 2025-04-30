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
// Created by tlea on 2/19/19.
//


#include <commonDeviceDefs.h>
#include <device/icDeviceResource.h>
#include <icLog/logging.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <subsystems/zigbee/zigbeeAttributeTypes.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>
#include <subsystems/zigbee/zigbeeIO.h>
#include <subsystems/zigbee/zigbeeSubsystem.h>

#ifdef BARTON_CONFIG_ZIGBEE

#include "zigbeeClusters/colorControlCluster.h"

#define LOG_TAG                       "colorControlCluster"

// ZCL wants integer values of normalized x and y by scaling by this value.
#define COLOR_CONTROL_XY_SCALE_FACTOR 65536.0

typedef struct
{
    ZigbeeCluster cluster;

    const ColorControlClusterCallbacks *callbacks;
    const void *callbackContext;
} ColorControlCluster;

static bool configureCluster(ZigbeeCluster *ctx, const DeviceConfigurationContext *configContext);

static bool handleAttributeReport(ZigbeeCluster *ctx, ReceivedAttributeReport *report);

ZigbeeCluster *colorControlClusterCreate(const ColorControlClusterCallbacks *callbacks, void *callbackContext)
{
    ColorControlCluster *result = (ColorControlCluster *) calloc(1, sizeof(ColorControlCluster));

    result->cluster.clusterId = COLOR_CONTROL_CLUSTER_ID;
    result->cluster.configureCluster = configureCluster;
    result->cluster.handleAttributeReport = handleAttributeReport;

    result->callbacks = callbacks;
    result->callbackContext = callbackContext;

    return (ZigbeeCluster *) result;
}

bool colorControlClusterGetX(uint64_t eui64, uint8_t endpointId, double *x)
{
    bool result = false;

    uint64_t val = 0;

    if (zigbeeSubsystemReadNumber(
            eui64, endpointId, COLOR_CONTROL_CLUSTER_ID, true, COLOR_CONTROL_CURRENTX_ATTRIBUTE_ID, &val) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to read current x attribute value", __FUNCTION__);
    }
    else
    {
        *x = val / COLOR_CONTROL_XY_SCALE_FACTOR;
        result = true;
    }

    return result;
}

bool colorControlClusterGetY(uint64_t eui64, uint8_t endpointId, double *y)
{
    bool result = false;

    uint64_t val = 0;

    if (zigbeeSubsystemReadNumber(
            eui64, endpointId, COLOR_CONTROL_CLUSTER_ID, true, COLOR_CONTROL_CURRENTY_ATTRIBUTE_ID, &val) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to read current y attribute value", __FUNCTION__);
    }
    else
    {
        *y = val / COLOR_CONTROL_XY_SCALE_FACTOR;
        result = true;
    }

    return result;
}

bool colorControlClusterMoveToColor(uint64_t eui64, uint8_t endpointId, double x, double y)
{
    if (x < 0 || x >= 1 || y < 0 || y >= 1)
    {
        icLogWarn(LOG_TAG, "%s: Invalid normalized x/y - %lf %lf", __func__, x, y);
        return false;
    }

    uint8_t msg[6];
    uint16_t scaledX = (uint16_t) (x * COLOR_CONTROL_XY_SCALE_FACTOR);
    uint16_t scaledY = (uint16_t) (y * COLOR_CONTROL_XY_SCALE_FACTOR);

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    msg[0] = (uint8_t) (scaledX & 0xff);
    msg[1] = (uint8_t) (scaledX >> 8);
    msg[2] = (uint8_t) (scaledY & 0xff);
    msg[3] = (uint8_t) (scaledY >> 8);
#else
    msg[1] = (uint8_t) (scaledX & 0xff);
    msg[0] = (uint8_t) (scaledX >> 8);
    msg[3] = (uint8_t) (scaledY & 0xff);
    msg[2] = (uint8_t) (scaledY >> 8);
#endif

    msg[4] = 0; // transition time byte 1
    msg[5] = 0; // transition time byte 2

    return (zigbeeSubsystemSendCommand(
                eui64, endpointId, COLOR_CONTROL_CLUSTER_ID, true, COLOR_CONTROL_MOVE_TO_COLOR_COMMAND_ID, msg, 6) ==
            0);
}

static bool configureCluster(ZigbeeCluster *ctx, const DeviceConfigurationContext *configContext)
{
    bool result = true;

    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    zhalAttributeReportingConfig colorConfigs[2];
    uint8_t numConfigs = 2;
    memset(&colorConfigs[0], 0, sizeof(zhalAttributeReportingConfig));
    colorConfigs[0].attributeInfo.id = COLOR_CONTROL_CURRENTX_ATTRIBUTE_ID;
    colorConfigs[0].attributeInfo.type = ZCL_INT16U_ATTRIBUTE_TYPE;
    colorConfigs[0].minInterval = 1;
    colorConfigs[0].maxInterval = REPORTING_INTERVAL_MAX;
    colorConfigs[0].reportableChange = 1;
    colorConfigs[1].attributeInfo.id = COLOR_CONTROL_CURRENTY_ATTRIBUTE_ID;
    colorConfigs[1].attributeInfo.type = ZCL_INT16U_ATTRIBUTE_TYPE;
    colorConfigs[1].minInterval = 1;
    colorConfigs[1].maxInterval = REPORTING_INTERVAL_MAX;
    colorConfigs[1].reportableChange = 1;

    if (zigbeeSubsystemBindingSet(configContext->eui64, configContext->endpointId, COLOR_CONTROL_CLUSTER_ID) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to bind color control", __FUNCTION__);
        result = false;
    }
    else if (zigbeeSubsystemAttributesSetReporting(
                 configContext->eui64, configContext->endpointId, COLOR_CONTROL_CLUSTER_ID, colorConfigs, numConfigs) !=
             0)
    {
        icLogError(LOG_TAG, "%s: failed to set reporting for color control", __FUNCTION__);
        result = false;
    }

    return result;
}

static bool handleAttributeReport(ZigbeeCluster *ctx, ReceivedAttributeReport *report)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    ColorControlCluster *colorControlCluster = (ColorControlCluster *) ctx;
    sbZigbeeIOContext *zio = zigbeeIOInit(report->reportData, report->reportDataLen, ZIO_READ);

    // Attributes we are expecting
    AUTO_CLEAN(free_generic__auto) uint16_t *x = NULL;
    AUTO_CLEAN(free_generic__auto) uint16_t *y = NULL;

    // Loop through the payload and populate the attribute values sent to us.
    // FIXME: Ideally, we need an attribute iterator since the data types of attributes
    // could change. Since the ones we care about now are the same data type, just mod 5.
    size_t remainingSize;
    while ((remainingSize = zigbeeIOGetRemainingSize(zio)) > 0 && remainingSize % 5 == 0)
    {
        uint16_t attributeId = zigbeeIOGetUint16(zio);
        zigbeeIOGetUint8(zio); // unneeded data type

        switch (attributeId)
        {
            case COLOR_CONTROL_CURRENTX_ATTRIBUTE_ID:
                if (zigbeeIOGetRemainingSize(zio) >= sizeof(uint16_t))
                {
                    x = malloc(sizeof(uint16_t));
                    *x = zigbeeIOGetUint16(zio);
                }
                break;
            case COLOR_CONTROL_CURRENTY_ATTRIBUTE_ID:
                if (zigbeeIOGetRemainingSize(zio) >= sizeof(uint16_t))
                {
                    y = malloc(sizeof(uint16_t));
                    *y = zigbeeIOGetUint16(zio);
                }
                break;
            default:
                icLogWarn(LOG_TAG, "Unrecognized attribute id %" PRIu16, attributeId);
                break;
        }
    }

    // Process collected attribute values
    if (x != NULL && y != NULL)
    {
        colorControlCluster->callbacks->currentXYChanged(report->eui64,
                                                         report->sourceEndpoint,
                                                         *x / COLOR_CONTROL_XY_SCALE_FACTOR,
                                                         *y / COLOR_CONTROL_XY_SCALE_FACTOR,
                                                         colorControlCluster->callbackContext);
    }
    else if (x != NULL)
    {
        colorControlCluster->callbacks->currentXChanged(report->eui64,
                                                        report->sourceEndpoint,
                                                        *x / COLOR_CONTROL_XY_SCALE_FACTOR,
                                                        colorControlCluster->callbackContext);
    }
    else if (y != NULL)
    {
        colorControlCluster->callbacks->currentYChanged(report->eui64,
                                                        report->sourceEndpoint,
                                                        *y / COLOR_CONTROL_XY_SCALE_FACTOR,
                                                        colorControlCluster->callbackContext);
    }

    return true;
}

#endif // BARTON_CONFIG_ZIGBEE
