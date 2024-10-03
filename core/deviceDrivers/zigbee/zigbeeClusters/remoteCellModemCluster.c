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

#include <icLog/logging.h>
#include <icUtil/stringUtils.h>
#include <memory.h>
#include <stdlib.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>
#include <subsystems/zigbee/zigbeeIO.h>
#include <subsystems/zigbee/zigbeeSubsystem.h>

#ifdef BARTON_CONFIG_ZIGBEE

#include "zigbeeClusters/remoteCellModemCluster.h"

#define LOG_TAG "remoteCellModemCluster"

static bool configureCluster(ZigbeeCluster *ctx, const DeviceConfigurationContext *configContext);

static bool handleAttributeReport(ZigbeeCluster *ctx, ReceivedAttributeReport *report);

typedef struct
{
    ZigbeeCluster cluster;
    const RemoteCellModemClusterCallbacks *callbacks;
    void *callbackContext;
    uint16_t manufacturerId;
} RemoteCellModemCluster;

ZigbeeCluster *remoteCellModemClusterCreate(const RemoteCellModemClusterCallbacks *callbacks,
                                            void *callbackContext,
                                            uint16_t manufacturerId)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    RemoteCellModemCluster *result = (RemoteCellModemCluster *) calloc(1, sizeof(RemoteCellModemCluster));

    result->cluster.clusterId = REMOTE_CELL_MODEM_CLUSTER_ID;
    result->cluster.configureCluster = configureCluster;
    result->cluster.priority = CLUSTER_PRIORITY_DEFAULT;
    result->cluster.handleAttributeReport = handleAttributeReport;

    result->callbacks = callbacks;
    result->callbackContext = callbackContext;
    result->manufacturerId = manufacturerId;

    return (ZigbeeCluster *) result;
}

static bool configureCluster(ZigbeeCluster *ctx, const DeviceConfigurationContext *configContext)
{
    RemoteCellModemCluster *cellModemCluster = (RemoteCellModemCluster *) ctx;

    icLogDebug(LOG_TAG, "%s: endpoint %" PRIu8, __FUNCTION__, configContext->endpointId);

    uint8_t numConfigs = 1;
    zhalAttributeReportingConfig attrReportConfigs[numConfigs];
    memset(&attrReportConfigs[0], 0, sizeof(zhalAttributeReportingConfig));

    attrReportConfigs[0].attributeInfo.id = REMOTE_CELL_MODEM_POWER_STATUS_ATTRIBUTE_ID;
    attrReportConfigs[0].attributeInfo.type = ZCL_BOOLEAN_ATTRIBUTE_TYPE;
    attrReportConfigs[0].minInterval = 0;
    attrReportConfigs[0].maxInterval = 1620; // every 27 minutes at least.  we need this for comm fail, but only 1 attr.
    attrReportConfigs[0].reportableChange = 1;

    if (zigbeeSubsystemBindingSet(configContext->eui64, configContext->endpointId, REMOTE_CELL_MODEM_CLUSTER_ID) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to bind", __FUNCTION__);
        return false;
    }
    if (zigbeeSubsystemAttributesSetReportingMfgSpecific(configContext->eui64,
                                                         configContext->endpointId,
                                                         REMOTE_CELL_MODEM_CLUSTER_ID,
                                                         cellModemCluster->manufacturerId,
                                                         attrReportConfigs,
                                                         numConfigs) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to set attribute reporting", __FUNCTION__);
        return false;
    }
    return true;
}

bool remoteCellModemClusterIsPoweredOn(ZigbeeCluster *ctx, uint64_t eui64, uint8_t endpointId, bool *result)
{
    RemoteCellModemCluster *cellModemCluster = (RemoteCellModemCluster *) ctx;

    icLogDebug(LOG_TAG, "%s: %" PRIx64 " endpoint %d", __FUNCTION__, eui64, endpointId);

    if (result == NULL)
    {
        icLogWarn(LOG_TAG, "invalid parameter: result");
        return false;
    }
    uint64_t value = 0;
    if (zigbeeSubsystemReadNumberMfgSpecific(eui64,
                                             endpointId,
                                             REMOTE_CELL_MODEM_CLUSTER_ID,
                                             cellModemCluster->manufacturerId,
                                             true,
                                             REMOTE_CELL_MODEM_POWER_STATUS_ATTRIBUTE_ID,
                                             &value) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to read power status attribute", __FUNCTION__);
        return false;
    }
    *result = (value > 0);
    return true;
}

bool remoteCellModemClusterPowerOn(ZigbeeCluster *ctx, uint64_t eui64, uint8_t endpointId)
{
    RemoteCellModemCluster *cellModemCluster = (RemoteCellModemCluster *) ctx;

    icLogDebug(LOG_TAG, "%s: %" PRIx64 " endpoint %d", __FUNCTION__, eui64, endpointId);

    if (zigbeeSubsystemSendMfgCommand(eui64,
                                      endpointId,
                                      REMOTE_CELL_MODEM_CLUSTER_ID,
                                      true,
                                      REMOTE_CELL_MODEM_POWER_ON_COMMAND_ID,
                                      cellModemCluster->manufacturerId,
                                      NULL,
                                      0) != 0)
    {
        icLogError(LOG_TAG, "%s: power on command failed", __FUNCTION__);
        return false;
    }
    return true;
}

bool remoteCellModemClusterPowerOff(ZigbeeCluster *ctx, uint64_t eui64, uint8_t endpointId)
{
    RemoteCellModemCluster *cellModemCluster = (RemoteCellModemCluster *) ctx;

    icLogDebug(LOG_TAG, "%s: %" PRIx64 " endpoint %d", __FUNCTION__, eui64, endpointId);

    if (zigbeeSubsystemSendMfgCommand(eui64,
                                      endpointId,
                                      REMOTE_CELL_MODEM_CLUSTER_ID,
                                      true,
                                      REMOTE_CELL_MODEM_OFF_COMMAND_ID,
                                      cellModemCluster->manufacturerId,
                                      NULL,
                                      0) != 0)
    {
        icLogError(LOG_TAG, "%s: power off command failed", __FUNCTION__);
        return false;
    }
    return true;
}

bool remoteCellModemClusterEmergencyReset(ZigbeeCluster *ctx, uint64_t eui64, uint8_t endpointId)
{
    RemoteCellModemCluster *cellModemCluster = (RemoteCellModemCluster *) ctx;

    icLogDebug(LOG_TAG, "%s: %" PRIx64 " endpoint %d", __FUNCTION__, eui64, endpointId);

    if (zigbeeSubsystemSendMfgCommand(eui64,
                                      endpointId,
                                      REMOTE_CELL_MODEM_CLUSTER_ID,
                                      true,
                                      REMOTE_CELL_MODEM_RESET_COMMAND_ID,
                                      cellModemCluster->manufacturerId,
                                      NULL,
                                      0) != 0)
    {
        icLogError(LOG_TAG, "%s: emergency reset command failed", __FUNCTION__);
        return false;
    }
    return true;
}

static bool handleAttributeReport(ZigbeeCluster *ctx, ReceivedAttributeReport *report)
{
    RemoteCellModemCluster *cellModemCluster = (RemoteCellModemCluster *) ctx;
    sbZigbeeIOContext *zio = zigbeeIOInit(report->reportData, report->reportDataLen, ZIO_READ);

    uint16_t attrId = zigbeeIOGetUint16(zio);
    uint8_t attrType = zigbeeIOGetUint8(zio);
    uint8_t attrValue = zigbeeIOGetUint8(zio);

    icLogDebug(LOG_TAG, "%s: 0x%15" PRIx64 " attr %" PRIu16, __FUNCTION__, report->eui64, attrId);
    bool result = true;

    switch (attrId)
    {
        case REMOTE_CELL_MODEM_POWER_STATUS_ATTRIBUTE_ID:
            if (cellModemCluster->callbacks->onOffStateChanged != NULL)
            {
                cellModemCluster->callbacks->onOffStateChanged(report->eui64, report->sourceEndpoint, (attrValue == 1));
            }
            break;
        default:
            icLogWarn(
                LOG_TAG, "%s: unsupported attribute %" PRIu16 " type 0x02%" PRIx8, __FUNCTION__, attrId, attrType);
            result = false;
            break;
    }

    return result;
}

#endif // BARTON_CONFIG_ZIGBEE