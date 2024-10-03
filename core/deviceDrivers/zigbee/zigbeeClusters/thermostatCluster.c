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
// Created by tlea on 3/1/19
//

#include <commonDeviceDefs.h>
#include <icLog/logging.h>
#include <icUtil/stringUtils.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <subsystems/zigbee/zigbeeAttributeTypes.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>
#include <subsystems/zigbee/zigbeeSubsystem.h>

#ifdef BARTON_CONFIG_ZIGBEE

#include "zigbeeClusters/thermostatCluster.h"

#define LOG_TAG                             "thermostatCluster"

#define MAX_TEMP_VALUE                      9999
#define MIN_TEMP_VALUE                      -9999

// some defines for legacy thermostat support
#define RTCOA_MFG_ID                        0x109A
#define LEGACY_OPERATIONAL_INFO_COMMAND_ID  0x22
#define SET_ABSOLUTE_SET_POINT_MODE_RTCOA   0x05
#define WRITE_SLEEP_DURATION                0x02

#define THERMOSTAT_CLUSTER_DISABLE_BIND_KEY "tstatClusterDisableBind"

static bool configureCluster(ZigbeeCluster *ctx, const DeviceConfigurationContext *configContext);

static bool handleAttributeReport(ZigbeeCluster *ctx, ReceivedAttributeReport *report);

static bool handleClusterCommand(ZigbeeCluster *ctx, ReceivedClusterCommand *command);

typedef struct
{
    ZigbeeCluster cluster;

    const ThermostatClusterCallbacks *callbacks;
    const void *callbackContext;
} ThermostatCluster;

ZigbeeCluster *thermostatClusterCreate(const ThermostatClusterCallbacks *callbacks, void *callbackContext)
{
    ThermostatCluster *result = (ThermostatCluster *) calloc(1, sizeof(ThermostatCluster));

    result->cluster.clusterId = THERMOSTAT_CLUSTER_ID;
    result->cluster.configureCluster = configureCluster;
    result->cluster.handleAttributeReport = handleAttributeReport;
    result->cluster.handleClusterCommand = handleClusterCommand;

    result->callbacks = callbacks;
    result->callbackContext = callbackContext;

    return (ZigbeeCluster *) result;
}

void thermostatClusterSetBindingEnabled(const DeviceConfigurationContext *deviceConfigurationContext, bool bind)
{
    addBoolConfigurationMetadata(
        deviceConfigurationContext->configurationMetadata, THERMOSTAT_CLUSTER_DISABLE_BIND_KEY, bind);
}

static bool configureCluster(ZigbeeCluster *ctx, const DeviceConfigurationContext *configContext)
{
    bool result = true;

    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    zhalAttributeReportingConfig *tstatReportingConfigs =
        (zhalAttributeReportingConfig *) calloc(8, sizeof(zhalAttributeReportingConfig));

    tstatReportingConfigs[0].attributeInfo.id = THERMOSTAT_LOCAL_TEMPERATURE_ATTRIBUTE_ID;
    tstatReportingConfigs[0].attributeInfo.type = ZCL_INT16S_ATTRIBUTE_TYPE;
    tstatReportingConfigs[0].minInterval = 1;
    tstatReportingConfigs[0].maxInterval = 1620;    // 27 minutes
    tstatReportingConfigs[0].reportableChange = 20; //.2 degrees celsius

    tstatReportingConfigs[1].attributeInfo.id = THERMOSTAT_OCCUPIED_COOLING_SETPOINT_ATTRIBUTE_ID;
    tstatReportingConfigs[1].attributeInfo.type = ZCL_INT16S_ATTRIBUTE_TYPE;
    tstatReportingConfigs[1].minInterval = 1;
    tstatReportingConfigs[1].maxInterval = 1620;    // 27 minutes
    tstatReportingConfigs[1].reportableChange = 20; //.2 degrees celsius

    tstatReportingConfigs[2].attributeInfo.id = THERMOSTAT_OCCUPIED_HEATING_SETPOINT_ATTRIBUTE_ID;
    tstatReportingConfigs[2].attributeInfo.type = ZCL_INT16S_ATTRIBUTE_TYPE;
    tstatReportingConfigs[2].minInterval = 1;
    tstatReportingConfigs[2].maxInterval = 1620;    // 27 minutes
    tstatReportingConfigs[2].reportableChange = 20; //.2 degrees celsius

    tstatReportingConfigs[3].attributeInfo.id = THERMOSTAT_SYSTEM_MODE_ATTRIBUTE_ID;
    tstatReportingConfigs[3].attributeInfo.type = ZCL_ENUM8_ATTRIBUTE_TYPE;
    tstatReportingConfigs[3].minInterval = 1;
    tstatReportingConfigs[3].maxInterval = 1620; // 27 minutes
    tstatReportingConfigs[3].reportableChange = 1;

    tstatReportingConfigs[4].attributeInfo.id = THERMOSTAT_LOCAL_TEMPERATURE_CALIBRATION_ATTRIBUTE_ID;
    tstatReportingConfigs[4].attributeInfo.type = ZCL_INT8S_ATTRIBUTE_TYPE;
    tstatReportingConfigs[4].minInterval = 1;
    tstatReportingConfigs[4].maxInterval = 1620; // 27 minutes
    tstatReportingConfigs[4].reportableChange = 1;

    tstatReportingConfigs[5].attributeInfo.id = THERMOSTAT_SETPOINT_HOLD_ATTRIBUTE_ID;
    tstatReportingConfigs[5].attributeInfo.type = ZCL_ENUM8_ATTRIBUTE_TYPE;
    tstatReportingConfigs[5].minInterval = 1;
    tstatReportingConfigs[5].maxInterval = 1620; // 27 minutes
    tstatReportingConfigs[5].reportableChange = 1;

    tstatReportingConfigs[6].attributeInfo.id = THERMOSTAT_RUNNING_STATE_ATTRIBUTE_ID;
    tstatReportingConfigs[6].attributeInfo.type = ZCL_BITMAP16_ATTRIBUTE_TYPE;
    tstatReportingConfigs[6].minInterval = 1;
    tstatReportingConfigs[6].maxInterval = 1620; // 27 minutes
    tstatReportingConfigs[6].reportableChange = 1;

    tstatReportingConfigs[7].attributeInfo.id = THERMOSTAT_CTRL_SEQ_OP_ATTRIBUTE_ID;
    tstatReportingConfigs[7].attributeInfo.type = ZCL_ENUM8_ATTRIBUTE_TYPE;
    tstatReportingConfigs[7].minInterval = 1;
    tstatReportingConfigs[7].maxInterval = 1620; // 27 minutes
    tstatReportingConfigs[7].reportableChange = 1;

    // If the property is set to false we skip, otherwise accept its value or the default of true if nothing was set
    if (getBoolConfigurationMetadata(configContext->configurationMetadata, THERMOSTAT_CLUSTER_DISABLE_BIND_KEY, true))
    {
        if (zigbeeSubsystemBindingSet(configContext->eui64, configContext->endpointId, THERMOSTAT_CLUSTER_ID) != 0)
        {
            icLogError(LOG_TAG, "%s: failed to bind thermostat cluster", __FUNCTION__);
            goto exit;
        }
    }

    if (zigbeeSubsystemAttributesSetReporting(
            configContext->eui64, configContext->endpointId, THERMOSTAT_CLUSTER_ID, tstatReportingConfigs, 8) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to set reporting on thermostat cluster", __FUNCTION__);
        goto exit;
    }

    result = true;

exit:
    free(tstatReportingConfigs);

    return result;
}

static bool handleAttributeReport(ZigbeeCluster *ctx, ReceivedAttributeReport *report)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    if (report->reportDataLen <= 3) // there has to be more than the attribute id and type
    {
        icLogError(LOG_TAG, "%s: invalid report data", __FUNCTION__);
        return false;
    }

    ThermostatCluster *thermostatCluster = (ThermostatCluster *) ctx;

    uint16_t attributeId = report->reportData[0] + (report->reportData[1] << (uint8_t) 8);

    switch (attributeId)
    {
        case THERMOSTAT_LOCAL_TEMPERATURE_ATTRIBUTE_ID:
        {
            int16_t temp = report->reportData[3] + (report->reportData[4] << (uint8_t) 8);
            if (thermostatCluster->callbacks->localTemperatureChanged != NULL)
            {
                thermostatCluster->callbacks->localTemperatureChanged(
                    report->eui64, report->sourceEndpoint, temp, thermostatCluster->callbackContext);
            }
            break;
        }

        case THERMOSTAT_OCCUPIED_HEATING_SETPOINT_ATTRIBUTE_ID:
        {
            int16_t temp = report->reportData[3] + (report->reportData[4] << (uint8_t) 8);
            if (thermostatCluster->callbacks->occupiedHeatingSetpointChanged != NULL)
            {
                thermostatCluster->callbacks->occupiedHeatingSetpointChanged(
                    report->eui64, report->sourceEndpoint, temp, thermostatCluster->callbackContext);
            }
            break;
        }

        case THERMOSTAT_OCCUPIED_COOLING_SETPOINT_ATTRIBUTE_ID:
        {
            int16_t temp = report->reportData[3] + (report->reportData[4] << (uint8_t) 8);
            if (thermostatCluster->callbacks->occupiedCoolingSetpointChanged != NULL)
            {
                thermostatCluster->callbacks->occupiedCoolingSetpointChanged(
                    report->eui64, report->sourceEndpoint, temp, thermostatCluster->callbackContext);
            }
            break;
        }

        case THERMOSTAT_SYSTEM_MODE_ATTRIBUTE_ID:
        {
            if (thermostatCluster->callbacks->systemModeChanged != NULL)
            {
                thermostatCluster->callbacks->systemModeChanged(
                    report->eui64, report->sourceEndpoint, report->reportData[3], thermostatCluster->callbackContext);
            }
            break;
        }

        case THERMOSTAT_RUNNING_STATE_ATTRIBUTE_ID:
        {
            if (report->reportDataLen < 5)
            {
                icLogError(LOG_TAG, "Insufficient data in thermostat running state attribute report");
                break;
            }

            uint16_t state = report->reportData[3] + (report->reportData[4] << (uint8_t) 8);

            if (thermostatCluster->callbacks->runningStateChanged != NULL)
            {
                thermostatCluster->callbacks->runningStateChanged(
                    report->eui64, report->sourceEndpoint, state, thermostatCluster->callbackContext);
            }
            break;
        }

        case THERMOSTAT_SETPOINT_HOLD_ATTRIBUTE_ID:
        {
            if (thermostatCluster->callbacks->setpointHoldChanged != NULL)
            {
                thermostatCluster->callbacks->setpointHoldChanged(report->eui64,
                                                                  report->sourceEndpoint,
                                                                  report->reportData[3] > 0,
                                                                  thermostatCluster->callbackContext);
            }
            break;
        }

        case THERMOSTAT_CTRL_SEQ_OP_ATTRIBUTE_ID:
        {
            if (thermostatCluster->callbacks->ctrlSeqOpChanged != NULL)
            {
                thermostatCluster->callbacks->ctrlSeqOpChanged(
                    report->eui64, report->sourceEndpoint, report->reportData[3], thermostatCluster->callbackContext);

                break;
            }
        }

        case THERMOSTAT_LOCAL_TEMPERATURE_CALIBRATION_ATTRIBUTE_ID:
        {
            if (thermostatCluster->callbacks->localTemperatureCalibrationChanged != NULL)
            {
                thermostatCluster->callbacks->localTemperatureCalibrationChanged(
                    report->eui64, report->sourceEndpoint, report->reportData[3], thermostatCluster->callbackContext);
            }
            break;
        }

        default:
            icLogError(LOG_TAG, "Unhandled thermostat attribute report for attribute id 0x%04" PRIx16, attributeId);
            break;
    }

    return true;
}

static bool handleClusterCommand(ZigbeeCluster *ctx, ReceivedClusterCommand *command)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    ThermostatCluster *thermostatCluster = (ThermostatCluster *) ctx;

    if (command->mfgSpecific == true && command->mfgCode == RTCOA_MFG_ID &&
        command->commandId == LEGACY_OPERATIONAL_INFO_COMMAND_ID)
    {
        if (thermostatCluster->callbacks->legacyOperationInfoReceived != NULL)
        {
            uint8_t runningMode = command->commandData[0];
            bool holdOn = command->commandData[1] == 1;
            uint8_t runningState = command->commandData[2];
            uint8_t fanRunningState = command->commandData[3];

            thermostatCluster->callbacks->legacyOperationInfoReceived(
                command->eui64, command->sourceEndpoint, runningMode, holdOn, runningState, fanRunningState);
        }
    }

    return true;
}

bool thermostatClusterGetRunningState(const ZigbeeCluster *cluster, uint64_t eui64, uint8_t endpointId, uint16_t *state)
{
    if (state == NULL)
    {
        return false;
    }

    uint64_t tmp = 0;
    if (zigbeeSubsystemReadNumber(
            eui64, endpointId, THERMOSTAT_CLUSTER_ID, true, THERMOSTAT_RUNNING_STATE_ATTRIBUTE_ID, &tmp) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to read running state attribute", __FUNCTION__);
        return false;
    }

    *state = (uint16_t) tmp;
    return true;
}

bool thermostatClusterGetSystemMode(const ZigbeeCluster *cluster, uint64_t eui64, uint8_t endpointId, uint8_t *mode)
{
    if (mode == NULL)
    {
        return false;
    }

    uint64_t tmp = 0;
    if (zigbeeSubsystemReadNumber(
            eui64, endpointId, THERMOSTAT_CLUSTER_ID, true, THERMOSTAT_SYSTEM_MODE_ATTRIBUTE_ID, &tmp) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to read system mode attribute", __FUNCTION__);
        return false;
    }

    *mode = (uint8_t) tmp;
    return true;
}

bool thermostatClusterSetSystemMode(const ZigbeeCluster *cluster, uint64_t eui64, uint8_t endpointId, uint8_t mode)
{
    if (zigbeeSubsystemWriteNumber(eui64,
                                   endpointId,
                                   THERMOSTAT_CLUSTER_ID,
                                   true,
                                   THERMOSTAT_SYSTEM_MODE_ATTRIBUTE_ID,
                                   ZCL_ENUM8_ATTRIBUTE_TYPE,
                                   mode,
                                   1) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to write system mode attribute", __FUNCTION__);
        return false;
    }

    return true;
}

bool thermostatClusterIsHoldOn(const ZigbeeCluster *cluster, uint64_t eui64, uint8_t endpointId, bool *isHoldOn)
{
    if (isHoldOn == NULL)
    {
        return false;
    }

    uint64_t tmp = 0;
    if (zigbeeSubsystemReadNumber(
            eui64, endpointId, THERMOSTAT_CLUSTER_ID, true, THERMOSTAT_SETPOINT_HOLD_ATTRIBUTE_ID, &tmp) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to read setpoint hold attribute", __FUNCTION__);
        return false;
    }

    *isHoldOn = tmp > 0;
    return true;
}

bool thermostatClusterSetHold(const ZigbeeCluster *cluster, uint64_t eui64, uint8_t endpointId, bool holdOn)
{
    if (zigbeeSubsystemWriteNumber(eui64,
                                   endpointId,
                                   THERMOSTAT_CLUSTER_ID,
                                   true,
                                   THERMOSTAT_SETPOINT_HOLD_ATTRIBUTE_ID,
                                   ZCL_ENUM8_ATTRIBUTE_TYPE,
                                   holdOn ? 1 : 0,
                                   1) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to write setpoint hold attribute", __FUNCTION__);
        return false;
    }

    return true;
}

bool thermostatClusterGetLocalTemperature(const ZigbeeCluster *cluster,
                                          uint64_t eui64,
                                          uint8_t endpointId,
                                          int16_t *temp)
{
    if (temp == NULL)
    {
        return false;
    }

    uint64_t tmp = 0;
    if (zigbeeSubsystemReadNumber(
            eui64, endpointId, THERMOSTAT_CLUSTER_ID, true, THERMOSTAT_LOCAL_TEMPERATURE_ATTRIBUTE_ID, &tmp) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to read local temperature attribute", __FUNCTION__);
        return false;
    }

    *temp = (int16_t) tmp;
    return true;
}

bool thermostatClusterGetLocalTemperatureCalibration(const ZigbeeCluster *cluster,
                                                     uint64_t eui64,
                                                     uint8_t endpointId,
                                                     int8_t *calibration)
{
    if (calibration == NULL)
    {
        return false;
    }

    uint64_t tmp = 0;
    if (zigbeeSubsystemReadNumber(eui64,
                                  endpointId,
                                  THERMOSTAT_CLUSTER_ID,
                                  true,
                                  THERMOSTAT_LOCAL_TEMPERATURE_CALIBRATION_ATTRIBUTE_ID,
                                  &tmp) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to read local temp calibration attribute", __FUNCTION__);
        return false;
    }

    *calibration = (int8_t) tmp;
    return true;
}

bool thermostatClusterSetLocalTemperatureCalibration(const ZigbeeCluster *cluster,
                                                     uint64_t eui64,
                                                     uint8_t endpointId,
                                                     int8_t calibration)
{
    if (zigbeeSubsystemWriteNumber(eui64,
                                   endpointId,
                                   THERMOSTAT_CLUSTER_ID,
                                   true,
                                   THERMOSTAT_LOCAL_TEMPERATURE_CALIBRATION_ATTRIBUTE_ID,
                                   ZCL_INT8S_ATTRIBUTE_TYPE,
                                   (uint64_t) calibration,
                                   1) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to write local temperature calibration attribute", __FUNCTION__);
        return false;
    }

    return true;
}

bool thermostatClusterGetAbsMinHeatSetpoint(const ZigbeeCluster *cluster,
                                            uint64_t eui64,
                                            uint8_t endpointId,
                                            int16_t *temp)
{
    if (temp == NULL)
    {
        return false;
    }

    uint64_t tmp = 0;
    if (zigbeeSubsystemReadNumber(
            eui64, endpointId, THERMOSTAT_CLUSTER_ID, true, THERMOSTAT_ABS_MIN_HEAT_SETPOINT_ATTRIBUTE_ID, &tmp) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to read abs min heat attribute", __FUNCTION__);
        return false;
    }

    *temp = (int16_t) tmp;
    return true;
}

bool thermostatClusterGetAbsMaxHeatSetpoint(const ZigbeeCluster *cluster,
                                            uint64_t eui64,
                                            uint8_t endpointId,
                                            int16_t *temp)
{
    if (temp == NULL)
    {
        return false;
    }

    uint64_t tmp = 0;
    if (zigbeeSubsystemReadNumber(
            eui64, endpointId, THERMOSTAT_CLUSTER_ID, true, THERMOSTAT_ABS_MAX_HEAT_SETPOINT_ATTRIBUTE_ID, &tmp) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to read abs max heat attribute", __FUNCTION__);
        return false;
    }

    *temp = (int16_t) tmp;
    return true;
}

bool thermostatClusterGetAbsMinCoolSetpoint(const ZigbeeCluster *cluster,
                                            uint64_t eui64,
                                            uint8_t endpointId,
                                            int16_t *temp)
{
    if (temp == NULL)
    {
        return false;
    }

    uint64_t tmp = 0;
    if (zigbeeSubsystemReadNumber(
            eui64, endpointId, THERMOSTAT_CLUSTER_ID, true, THERMOSTAT_ABS_MIN_COOL_SETPOINT_ATTRIBUTE_ID, &tmp) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to read abs min cool attribute", __FUNCTION__);
        return false;
    }

    *temp = (int16_t) tmp;
    return true;
}

bool thermostatClusterGetAbsMaxCoolSetpoint(const ZigbeeCluster *cluster,
                                            uint64_t eui64,
                                            uint8_t endpointId,
                                            int16_t *temp)
{
    if (temp == NULL)
    {
        return false;
    }

    uint64_t tmp = 0;
    if (zigbeeSubsystemReadNumber(
            eui64, endpointId, THERMOSTAT_CLUSTER_ID, true, THERMOSTAT_ABS_MAX_COOL_SETPOINT_ATTRIBUTE_ID, &tmp) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to read abs max cool attribute", __FUNCTION__);
        return false;
    }

    *temp = (int16_t) tmp;
    return true;
}

bool thermostatClusterGetOccupiedHeatingSetpoint(const ZigbeeCluster *cluster,
                                                 uint64_t eui64,
                                                 uint8_t endpointId,
                                                 int16_t *temp)
{
    if (temp == NULL)
    {
        return false;
    }

    uint64_t tmp = 0;
    if (zigbeeSubsystemReadNumber(
            eui64, endpointId, THERMOSTAT_CLUSTER_ID, true, THERMOSTAT_OCCUPIED_HEATING_SETPOINT_ATTRIBUTE_ID, &tmp) !=
        0)
    {
        icLogError(LOG_TAG, "%s: failed to read occupied heating setpoint attribute", __FUNCTION__);
        return false;
    }

    *temp = (int16_t) tmp;
    return true;
}

bool thermostatClusterSetOccupiedHeatingSetpoint(const ZigbeeCluster *cluster,
                                                 uint64_t eui64,
                                                 uint8_t endpointId,
                                                 int16_t temp)
{
    if (zigbeeSubsystemWriteNumber(eui64,
                                   endpointId,
                                   THERMOSTAT_CLUSTER_ID,
                                   true,
                                   THERMOSTAT_OCCUPIED_HEATING_SETPOINT_ATTRIBUTE_ID,
                                   ZCL_INT16S_ATTRIBUTE_TYPE,
                                   (uint64_t) temp,
                                   2) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to write occupied heating setpoint attribute", __FUNCTION__);
        return false;
    }

    return true;
}

bool thermostatClusterGetOccupiedCoolingSetpoint(const ZigbeeCluster *cluster,
                                                 uint64_t eui64,
                                                 uint8_t endpointId,
                                                 int16_t *temp)
{
    if (temp == NULL)
    {
        return false;
    }

    uint64_t tmp = 0;
    if (zigbeeSubsystemReadNumber(
            eui64, endpointId, THERMOSTAT_CLUSTER_ID, true, THERMOSTAT_OCCUPIED_COOLING_SETPOINT_ATTRIBUTE_ID, &tmp) !=
        0)
    {
        icLogError(LOG_TAG, "%s: failed to read occupied cooling setpoint attribute", __FUNCTION__);
        return false;
    }

    *temp = (int16_t) tmp;
    return true;
}

bool thermostatClusterSetOccupiedCoolingSetpoint(const ZigbeeCluster *cluster,
                                                 uint64_t eui64,
                                                 uint8_t endpointId,
                                                 int16_t temp)
{
    if (zigbeeSubsystemWriteNumber(eui64,
                                   endpointId,
                                   THERMOSTAT_CLUSTER_ID,
                                   true,
                                   THERMOSTAT_OCCUPIED_COOLING_SETPOINT_ATTRIBUTE_ID,
                                   ZCL_INT16S_ATTRIBUTE_TYPE,
                                   (uint64_t) temp,
                                   2) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to write occupied cooling setpoint attribute", __FUNCTION__);
        return false;
    }

    return true;
}

bool thermostatClusterGetCtrlSeqOp(const ZigbeeCluster *cluster, uint64_t eui64, uint8_t endpointId, uint8_t *ctrlSeqOp)
{
    if (ctrlSeqOp == NULL)
    {
        return false;
    }

    uint64_t tmp = 0;
    if (zigbeeSubsystemReadNumber(
            eui64, endpointId, THERMOSTAT_CLUSTER_ID, true, THERMOSTAT_CTRL_SEQ_OP_ATTRIBUTE_ID, &tmp) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to read control sequence of operation attribute", __FUNCTION__);
        return false;
    }

    *ctrlSeqOp = (uint8_t) tmp;
    return true;
}

bool thermostatClusterSetCtrlSeqOp(const ZigbeeCluster *cluster, uint64_t eui64, uint8_t endpointId, uint8_t ctrlSeqOp)
{
    if (zigbeeSubsystemWriteNumber(eui64,
                                   endpointId,
                                   THERMOSTAT_CLUSTER_ID,
                                   true,
                                   THERMOSTAT_CTRL_SEQ_OP_ATTRIBUTE_ID,
                                   ZCL_ENUM8_ATTRIBUTE_TYPE,
                                   ctrlSeqOp,
                                   1) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to write control sequence of operation attribute", __FUNCTION__);
        return false;
    }

    return true;
}

bool thermostatClusterSetAbsoluteSetpointModeLegacy(const ZigbeeCluster *cluster, uint64_t eui64, uint8_t endpointId)
{
    uint8_t payload[1] = {0x1};

    if (zigbeeSubsystemSendMfgCommand(eui64,
                                      endpointId,
                                      THERMOSTAT_CLUSTER_ID,
                                      true,
                                      SET_ABSOLUTE_SET_POINT_MODE_RTCOA,
                                      RTCOA_MFG_ID,
                                      payload,
                                      1) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to send set absolute set point mode command", __FUNCTION__);
        return false;
    }

    return true;
}

bool thermostatClusterSetPollRateLegacy(const ZigbeeCluster *cluster,
                                        uint64_t eui64,
                                        uint8_t endpointId,
                                        uint16_t quarterSeconds)
{
    icLogDebug(LOG_TAG, "%s: qs=%" PRIu16, __FUNCTION__, quarterSeconds);

    uint8_t payload[2] = {quarterSeconds & 0xffu, (quarterSeconds >> 8u)};

    if (zigbeeSubsystemSendMfgCommand(
            eui64, endpointId, THERMOSTAT_CLUSTER_ID, true, WRITE_SLEEP_DURATION, RTCOA_MFG_ID, payload, 2) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to send write sleep duration command", __FUNCTION__);
        return false;
    }

    return true;
}

bool thermostatClusterRequestOperationalInfoLegacy(const ZigbeeCluster *cluster, uint64_t eui64, uint8_t endpointId)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    if (zigbeeSubsystemSendMfgCommand(eui64,
                                      endpointId,
                                      THERMOSTAT_CLUSTER_ID,
                                      true,
                                      LEGACY_OPERATIONAL_INFO_COMMAND_ID,
                                      RTCOA_MFG_ID,
                                      NULL,
                                      0) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to send operational info request command", __FUNCTION__);
        return false;
    }

    return true;
}

const char *thermostatClusterGetSystemState(uint16_t runningState)
{ /*
  * this is a 2 byte bitmask field
  *
 HeatStateOn = 0x0001;
 CoolStateOn = 0x0002;
 FanStateOn = 0x0004;
 HeatSecondStageStateOn = 0x0008;
 CoolSecondStageStateOn = 0x0010;
 FanSecondStageStateOn = 0x0020;
 FanThirdStageStateOn = 0x0040;
  */
    if (runningState & (uint64_t) 0x0001 || runningState & (uint64_t) 0x0008)
    {
        return THERMOSTAT_PROFILE_RESOURCE_SYSTEM_STATE_HEATING;
    }
    else if (runningState & (uint64_t) 0x0002 || runningState & (uint64_t) 0x0010)
    {
        return THERMOSTAT_PROFILE_RESOURCE_SYSTEM_STATE_COOLING;
    }
    else
    {
        return THERMOSTAT_PROFILE_RESOURCE_SYSTEM_STATE_OFF;
    }
}

bool thermostatClusterIsFanOn(uint16_t runningState)
{
    /*
    * this is a 2 byte bitmask field
    *
   HeatStateOn = 0x0001;
   CoolStateOn = 0x0002;
   FanStateOn = 0x0004;
   HeatSecondStageStateOn = 0x0008;
   CoolSecondStageStateOn = 0x0010;
   FanSecondStageStateOn = 0x0020;
   FanThirdStageStateOn = 0x0040;
    */

    return (runningState & (uint64_t) 0x0004 || runningState & (uint64_t) 0x0020 || runningState & (uint64_t) 0x0040);
}

// return a constant string representing the provided system mode value
const char *thermostatClusterGetSystemModeString(uint8_t systemMode)
{
    /*
     * 	off(0x0),
     *  auto(0x1),
     *  cool(0x3),
     *  heat(0x4),
     *  emergencyHeating(0x5),
     *  precooling(0x6),
     *  fanOnly(0x7);
     */
    switch (systemMode)
    {
        case 0x0:
            return THERMOSTAT_PROFILE_RESOURCE_SYSTEM_MODE_OFF;
        case 0x1:
            return THERMOSTAT_PROFILE_RESOURCE_SYSTEM_MODE_AUTO;
        case 0x3:
            return THERMOSTAT_PROFILE_RESOURCE_SYSTEM_MODE_COOL;
        case 0x4:
        case 0x5:
            return THERMOSTAT_PROFILE_RESOURCE_SYSTEM_MODE_HEAT;
        case 0x6:
            return THERMOSTAT_PROFILE_RESOURCE_SYSTEM_MODE_PRECOOLING;
        case 0x7:
            return THERMOSTAT_PROFILE_RESOURCE_SYSTEM_MODE_FAN_ONLY;

        default:
            return "unknown";
    }
}

// this only returns the values we support setting, not the entire enum
uint8_t thermostatClusterGetSystemModeFromString(const char *systemMode)
{
    if (systemMode == NULL)
    {
        return 0xff; // invalid
    }

    if (stringCompare(systemMode, THERMOSTAT_PROFILE_RESOURCE_SYSTEM_MODE_HEAT, false) == 0)
    {
        return 0x4;
    }
    else if (stringCompare(systemMode, THERMOSTAT_PROFILE_RESOURCE_SYSTEM_MODE_COOL, false) == 0)
    {
        return 0x3;
    }
    else if (stringCompare(systemMode, THERMOSTAT_PROFILE_RESOURCE_SYSTEM_MODE_OFF, false) == 0)
    {
        return 0x0;
    }

    return 0xff; // invalid
}

// return a string representing the temperature in celcius * 100
//  Caller MUST free returned memory
char *thermostatClusterGetTemperatureString(int16_t temperature)
{
    if (temperature > MAX_TEMP_VALUE)
    {
        icLogError(LOG_TAG, "%s: out of range %" PRIu16, __FUNCTION__, temperature);
        return NULL;
    }

    char *result = stringBuilder("%04" PRIi16, temperature);

    return result;
}

bool thermostatClusterGetTemperatureValue(const char *temperatureString, int16_t *temp)
{
    if (temperatureString == NULL || temp == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid args", __FUNCTION__);
        return false;
    }

    int16_t val = 0;

    if (stringToInt16(temperatureString, &val) == false)
    {
        icLogError(LOG_TAG, "%s: string out of int16_t bounds %s", __FUNCTION__, temperatureString);
        return false;
    }
    else if (val > MAX_TEMP_VALUE || val < MIN_TEMP_VALUE)
    {
        icLogError(LOG_TAG, "%s: out of range %s", __FUNCTION__, temperatureString);
        return false;
    }

    *temp = val;
    return true;
}

const char *thermostatClusterGetCtrlSeqOpString(uint8_t ctrlSeqOp)
{
    switch (ctrlSeqOp)
    {
        case 0x01:
            return "coolingWithReheat";

        case 0x02:
            return "heatingOnly";

        case 0x03:
            return "heatingWithReheat";

        case 0x04:
            return "coolingAndHeatingFourPipes";

        case 0x05:
            return "coolingAndHeatingFourPipesWithReheat";

        case 0x00:
        default:
            return "coolingOnly";
    }
}

uint8_t thermostatClusterGetCtrlSeqOpFromString(const char *ctrlSeqOp)
{
    if (ctrlSeqOp == NULL)
    {
        return 0xff; // invalid
    }

    if (stringCompare(ctrlSeqOp, "coolingOnly", false) == 0)
    {
        return 0x00;
    }
    else if (stringCompare(ctrlSeqOp, "coolingWithReheat", false) == 0)
    {
        return 0x01;
    }
    else if (stringCompare(ctrlSeqOp, "heatingOnly", false) == 0)
    {
        return 0x02;
    }
    else if (stringCompare(ctrlSeqOp, "heatingWithReheat", false) == 0)
    {
        return 0x03;
    }
    else if (stringCompare(ctrlSeqOp, "coolingAndHeatingFourPipes", false) == 0)
    {
        return 0x04;
    }
    else if (stringCompare(ctrlSeqOp, "coolingAndHeatingFourPipesWithReheat", false) == 0)
    {
        return 0x05;
    }

    return 0xff; // invalid
}

uint8_t thermostatClusterGetFanModeFromString(const char *fanMode)
{
    if (fanMode == NULL)
    {
        return 0xff; // invalid
    }

    if (stringCompare(fanMode, "off", false) == 0)
    {
        return 0x0;
    }
    else if (stringCompare(fanMode, "on", false) == 0)
    {
        return 0x4;
    }
    else if (stringCompare(fanMode, "auto", false) == 0)
    {
        return 0x5;
    }

    return 0xff; // invalid
}

#endif // BARTON_CONFIG_ZIGBEE