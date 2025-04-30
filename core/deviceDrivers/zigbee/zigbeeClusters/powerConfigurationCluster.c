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
#include <icUtil/stringUtils.h>
#include <memory.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <subsystems/zigbee/zigbeeAttributeTypes.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>
#include <subsystems/zigbee/zigbeeIO.h>
#include <subsystems/zigbee/zigbeeSubsystem.h>
#include <zhal/zhal.h>

#ifdef BARTON_CONFIG_ZIGBEE

#include "zigbeeClusters/powerConfigurationCluster.h"

#define LOG_TAG                                           "powerConfigurationCluster"
// Alarm codes
#define AC_VOLTAGE_BELOW_MIN                              0x00
#define BATTERY_BELOW_MIN_THRESHOLD                       0x10
// These seem to be extensions to the Zigbee Spec
#define BATTERY_NOT_AVAILABLE                             0x3B
#define BATTERY_BAD                                       0x3C
#define BATTERY_HIGH_TEMPERATURE                          0x3F
#define CONFIGURE_BATTERY_ALARM_STATE_KEY                 "powerConfigurationConfigureBatteryAlarmState"
#define CONFIGURE_BATTERY_ALARM_MASK_KEY                  "powerConfigurationConfigureBatteryAlarmMask"
#define CONFIGURE_BATTERY_VOLTAGE_KEY                     "powerConfigurationConfigureBatteryVoltage"
#define CONFIGURE_BATTERY_PERCENTAGE_KEY                  "powerConfigurationConfigureBatteryPercentage"
#define CONFIGURE_BATTERY_RECHARGE_CYCLES_KEY             "powerConfigurationConfigureBatteryRechargeCycles"
#define CONFIGURE_BATTERY_VOLTAGE_MAX_INTERVAL            "powerConfigurationBatteryVoltageMaxInterval"
#define AC_POWER_LOSS_ALARM                               0x01
#define BATTERY_TOO_LOW_ALARM                             0x01

#define POWER_CONFIGURATION_CLUSTER_ENABLE_BIND_KEY       "powerConfigClusterEnableBind"
#define POWER_CONFIGURATION_CLUSTER_INVALID_VOLTAGE_VALUE 0xFF

typedef struct
{
    ZigbeeCluster cluster;

    const PowerConfigurationClusterCallbacks *callbacks;
    void *callbackContext;
    uint16_t mfgSpecificBatteryRechargeCycleAttributeId;
} PowerConfigurationCluster;

static bool configureCluster(ZigbeeCluster *ctx, const DeviceConfigurationContext *configContext);
static void handlePollControlCheckin(ZigbeeCluster *ctx, uint64_t eui64, uint8_t endpointId);
static bool
handleAlarm(ZigbeeCluster *ctx, uint64_t eui64, uint8_t endpointId, const ZigbeeAlarmTableEntry *alarmTableEntry);
static bool handleAlarmCleared(ZigbeeCluster *ctx,
                               uint64_t eui64,
                               uint8_t endpointId,
                               const ZigbeeAlarmTableEntry *alarmTableEntry);
static bool handleAttributeReport(ZigbeeCluster *ctx, ReceivedAttributeReport *report);

ZigbeeCluster *powerConfigurationClusterCreate(const PowerConfigurationClusterCallbacks *callbacks,
                                               void *callbackContext)
{
    PowerConfigurationCluster *result = (PowerConfigurationCluster *) calloc(1, sizeof(PowerConfigurationCluster));

    result->cluster.clusterId = POWER_CONFIGURATION_CLUSTER_ID;

    result->cluster.handlePollControlCheckin = handlePollControlCheckin;
    result->cluster.configureCluster = configureCluster;
    result->cluster.handleAlarm = handleAlarm;
    result->cluster.handleAlarmCleared = handleAlarmCleared;
    result->cluster.handleAttributeReport = handleAttributeReport;

    result->callbacks = callbacks;
    result->callbackContext = callbackContext;

    return (ZigbeeCluster *) result;
}

void powerConfigurationClusterSetBindingEnabled(const DeviceConfigurationContext *deviceConfigurationContext, bool bind)
{
    addBoolConfigurationMetadata(
        deviceConfigurationContext->configurationMetadata, POWER_CONFIGURATION_CLUSTER_ENABLE_BIND_KEY, bind);
}

bool powerConfigurationClusterGetBatteryVoltage(uint64_t eui64, uint8_t endpointId, uint8_t *decivolts)
{
    bool result = false;

    if (decivolts == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return false;
    }

    uint64_t val;
    if (zigbeeSubsystemReadNumber(
            eui64, endpointId, POWER_CONFIGURATION_CLUSTER_ID, true, BATTERY_VOLTAGE_ATTRIBUTE_ID, &val) == 0)
    {
        *decivolts = (uint8_t) (val & 0xFF);
        if (*decivolts != POWER_CONFIGURATION_CLUSTER_INVALID_VOLTAGE_VALUE)
        {
            result = true;
        }
    }
    else
    {
        icLogError(LOG_TAG, "%s: failed to read battery voltage", __FUNCTION__);
    }

    return result;
}

bool powerConfigurationClusterGetBatteryPercentageRemaining(uint64_t eui64, uint8_t endpointId, uint8_t *halfIntPercent)
{
    bool result = false;

    if (halfIntPercent == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return false;
    }

    uint64_t val;
    if (zigbeeSubsystemReadNumber(
            eui64, endpointId, POWER_CONFIGURATION_CLUSTER_ID, true, BATTERY_PERCENTAGE_REMAINING_ATTRIBUTE_ID, &val) ==
        0)
    {
        *halfIntPercent = (uint8_t) (val & 0xFF);
        result = true;
    }
    else
    {
        icLogError(LOG_TAG, "%s: failed to read battery percentage remaining", __FUNCTION__);
    }

    return result;
}

void powerConfigurationClusterBatteryThresholdsDestroy(PowerConfigurationClusterBatteryThresholds *thresholds)
{
    if (thresholds != NULL)
    {
        free(thresholds->minThresholdDecivolts);
        free(thresholds->threshold1Decivolts);
        free(thresholds->threshold2Decivolts);
        free(thresholds->threshold3Decivolts);
        free(thresholds->minThresholdPercent);
        free(thresholds->threshold1Percent);
        free(thresholds->threshold2Percent);
        free(thresholds->threshold3Percent);
        free(thresholds);
    }
}

static void setThresholdEntry(uint8_t **target, const bool *successes, const uint64_t *values, int idx)
{
    if (successes[idx] == true)
    {
        *target = malloc(1);
        **target = values[idx];
    }
}

PowerConfigurationClusterBatteryThresholds *powerConfigurationClusterGetBatteryThresholds(uint64_t eui64,
                                                                                          uint8_t endpointId)
{
    PowerConfigurationClusterBatteryThresholds *thresholds = NULL;

    uint16_t attributeIds[] = {
        BATTERY_VOLTAGE_MIN_THRESHOLD_ATTRIBUTE_ID,
        BATTERY_VOLTAGE_THRESHOLD1_ATTRIBUTE_ID,
        BATTERY_VOLTAGE_THRESHOLD2_ATTRIBUTE_ID,
        BATTERY_VOLTAGE_THRESHOLD3_ATTRIBUTE_ID,
        BATTERY_PERCENTAGE_MIN_THRESHOLD_ATTRIBUTE_ID,
        BATTERY_PERCENTAGE_THRESHOLD1_ATTRIBUTE_ID,
        BATTERY_PERCENTAGE_THRESHOLD2_ATTRIBUTE_ID,
        BATTERY_PERCENTAGE_THRESHOLD3_ATTRIBUTE_ID,
    };

    scoped_generic uint64_t *values = calloc(ARRAY_LENGTH(attributeIds), sizeof(uint64_t));
    scoped_generic bool *successes = calloc(ARRAY_LENGTH(attributeIds), sizeof(bool));

    if (zigbeeSubsystemReadNumbers(eui64,
                                   endpointId,
                                   POWER_CONFIGURATION_CLUSTER_ID,
                                   true,
                                   ARRAY_LENGTH(attributeIds),
                                   attributeIds,
                                   values,
                                   successes) == 0)
    {
        thresholds = calloc(1, sizeof(*thresholds));

        // the provided index matches the position in the attributeIds array if the read was successful
        setThresholdEntry(&thresholds->minThresholdDecivolts, successes, values, 0);
        setThresholdEntry(&thresholds->threshold1Decivolts, successes, values, 1);
        setThresholdEntry(&thresholds->threshold2Decivolts, successes, values, 2);
        setThresholdEntry(&thresholds->threshold3Decivolts, successes, values, 3);
        setThresholdEntry(&thresholds->minThresholdPercent, successes, values, 4);
        setThresholdEntry(&thresholds->threshold1Percent, successes, values, 5);
        setThresholdEntry(&thresholds->threshold2Percent, successes, values, 6);
        setThresholdEntry(&thresholds->threshold3Percent, successes, values, 7);
    }

    return thresholds;
}

bool powerConfigurationClusterGetMainsVoltage(uint64_t eui64, uint8_t endpointId, uint16_t *decivolts)
{
    bool result = false;

    if (decivolts == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return false;
    }

    uint64_t val;
    if (zigbeeSubsystemReadNumber(
            eui64, endpointId, POWER_CONFIGURATION_CLUSTER_ID, true, MAINS_VOLTAGE_ATTRIBUTE_ID, &val) == 0)
    {
        *decivolts = (uint16_t) (val & 0xFFFF);
        result = true;
    }
    else
    {
        icLogError(LOG_TAG, "%s: failed to read mains voltage", __FUNCTION__);
    }

    return result;
}

void powerConfigurationClusterSetConfigureBatteryAlarmState(
    const DeviceConfigurationContext *deviceConfigurationContext,
    bool configure)
{
    addBoolConfigurationMetadata(
        deviceConfigurationContext->configurationMetadata, CONFIGURE_BATTERY_ALARM_STATE_KEY, configure);
}

void powerConfigurationClusterSetConfigureBatteryAlarmMask(const DeviceConfigurationContext *deviceConfigurationContext,
                                                           bool configure)
{
    addBoolConfigurationMetadata(
        deviceConfigurationContext->configurationMetadata, CONFIGURE_BATTERY_ALARM_MASK_KEY, configure);
}

void powerConfigurationClusterSetConfigureBatteryVoltage(const DeviceConfigurationContext *deviceConfigurationContext,
                                                         bool configure)
{
    addBoolConfigurationMetadata(
        deviceConfigurationContext->configurationMetadata, CONFIGURE_BATTERY_VOLTAGE_KEY, configure);
}

void powerConfigurationClusterSetConfigureBatteryPercentage(
    const DeviceConfigurationContext *deviceConfigurationContext,
    bool configure)
{
    addBoolConfigurationMetadata(
        deviceConfigurationContext->configurationMetadata, CONFIGURE_BATTERY_PERCENTAGE_KEY, configure);
}

void powerConfigurationClusterSetConfigureBatteryRechargeCycles(
    const DeviceConfigurationContext *deviceConfigurationContext,
    uint16_t attributeId)
{
    addNumberConfigurationMetadata(
        deviceConfigurationContext->configurationMetadata, CONFIGURE_BATTERY_RECHARGE_CYCLES_KEY, attributeId);
}

void powerConfigurationClusterSetConfigureBatteryVoltageMaxInterval(
    const DeviceConfigurationContext *deviceConfigurationContext,
    uint64_t interval)
{
    addNumberConfigurationMetadata(
        deviceConfigurationContext->configurationMetadata, CONFIGURE_BATTERY_VOLTAGE_MAX_INTERVAL, interval);
}

static bool configureCluster(ZigbeeCluster *ctx, const DeviceConfigurationContext *configContext)
{
    bool result = true;

    icLogDebug(LOG_TAG, "%s", __FUNCTION__);
    bool configuredReporting = false;

    if (icDiscoveredDeviceDetailsClusterHasAttribute(configContext->discoveredDeviceDetails,
                                                     configContext->endpointId,
                                                     POWER_CONFIGURATION_CLUSTER_ID,
                                                     true,
                                                     BATTERY_ALARM_STATE_ATTRIBUTE_ID) == true)
    {
        // Check whether to configure battery alarm state, default to true
        if (getBoolConfigurationMetadata(configContext->configurationMetadata, CONFIGURE_BATTERY_ALARM_STATE_KEY, true))
        {
            // configure attribute reporting on battery state
            zhalAttributeReportingConfig batteryStateConfigs[1];
            uint8_t numConfigs = 1;
            memset(&batteryStateConfigs[0], 0, sizeof(zhalAttributeReportingConfig));
            batteryStateConfigs[0].attributeInfo.id = BATTERY_ALARM_STATE_ATTRIBUTE_ID;
            batteryStateConfigs[0].attributeInfo.type = ZCL_BITMAP32_ATTRIBUTE_TYPE;
            batteryStateConfigs[0].minInterval = 1;
            // We want to be told only when it changes, but there is a bug in Ember's stack (issue number 86930)
            // where a max interval of 0 can cause problems.  We need to use the max value of about 18 hours
            // for now.
            batteryStateConfigs[0].maxInterval = REPORTING_INTERVAL_MAX;
            batteryStateConfigs[0].reportableChange = 1;

            if (zigbeeSubsystemAttributesSetReporting(configContext->eui64,
                                                      configContext->endpointId,
                                                      POWER_CONFIGURATION_CLUSTER_ID,
                                                      batteryStateConfigs,
                                                      numConfigs) != 0)
            {
                icLogError(LOG_TAG, "%s: failed to set reporting for battery alarm state", __FUNCTION__);
                result = false;
            }

            // Record that we configured reporting
            configuredReporting = true;
        }
    }

    // Check whether to configure battery alarm mask, default to false
    if (getBoolConfigurationMetadata(configContext->configurationMetadata, CONFIGURE_BATTERY_ALARM_MASK_KEY, false))
    {
        if (zigbeeSubsystemWriteNumber(configContext->eui64,
                                       configContext->endpointId,
                                       POWER_CONFIGURATION_CLUSTER_ID,
                                       true,
                                       BATTERY_ALARM_MASK_ATTRIBUTE_ID,
                                       ZCL_BITMAP8_ATTRIBUTE_TYPE,
                                       BATTERY_TOO_LOW_ALARM,
                                       1) != 0)
        {
            icLogError(LOG_TAG, "%s: failed to set battery alarm mask", __FUNCTION__);
            result = false;
        }
    }

    if (icDiscoveredDeviceDetailsClusterHasAttribute(configContext->discoveredDeviceDetails,
                                                     configContext->endpointId,
                                                     POWER_CONFIGURATION_CLUSTER_ID,
                                                     true,
                                                     MAINS_ALARM_MASK_ATTRIBUTE_ID) == true)
    {
        if (zigbeeSubsystemWriteNumber(configContext->eui64,
                                       configContext->endpointId,
                                       POWER_CONFIGURATION_CLUSTER_ID,
                                       true,
                                       MAINS_ALARM_MASK_ATTRIBUTE_ID,
                                       ZCL_BITMAP8_ATTRIBUTE_TYPE,
                                       AC_POWER_LOSS_ALARM,
                                       1) != 0)
        {
            icLogError(LOG_TAG, "%s: failed to set mains alarm mask", __FUNCTION__);
            result = false;
        }
    }

    // Check whether to configure battery voltage reporting, default to false
    if (icDiscoveredDeviceDetailsClusterHasAttribute(configContext->discoveredDeviceDetails,
                                                     configContext->endpointId,
                                                     POWER_CONFIGURATION_CLUSTER_ID,
                                                     true,
                                                     BATTERY_VOLTAGE_ATTRIBUTE_ID) == true)
    {
        if (getBoolConfigurationMetadata(configContext->configurationMetadata, CONFIGURE_BATTERY_VOLTAGE_KEY, false))
        {
            // configure attribute reporting on battery voltage
            zhalAttributeReportingConfig batteryVoltageConfigs[1];
            uint8_t numConfigs = 1;
            memset(&batteryVoltageConfigs[0], 0, sizeof(zhalAttributeReportingConfig));
            batteryVoltageConfigs[0].attributeInfo.id = BATTERY_VOLTAGE_ATTRIBUTE_ID;
            batteryVoltageConfigs[0].attributeInfo.type = ZCL_INT8U_ATTRIBUTE_TYPE;
            batteryVoltageConfigs[0].minInterval = 1;
            batteryVoltageConfigs[0].maxInterval =
                (uint16_t) getNumberConfigurationMetadata(configContext->configurationMetadata,
                                                          CONFIGURE_BATTERY_VOLTAGE_MAX_INTERVAL,
                                                          REPORTING_INTERVAL_TWENTY_SEVEN_MINS);
            batteryVoltageConfigs[0].reportableChange = 1;

            if (zigbeeSubsystemAttributesSetReporting(configContext->eui64,
                                                      configContext->endpointId,
                                                      POWER_CONFIGURATION_CLUSTER_ID,
                                                      batteryVoltageConfigs,
                                                      numConfigs) != 0)
            {
                icLogError(LOG_TAG, "%s: failed to set reporting for battery voltage", __FUNCTION__);
                result = false;
            }

            // Record that we configured reporting
            configuredReporting = true;
        }
    }

    // Check whether to configure battery percent remaining reporting, default to false
    if (icDiscoveredDeviceDetailsClusterHasAttribute(configContext->discoveredDeviceDetails,
                                                     configContext->endpointId,
                                                     POWER_CONFIGURATION_CLUSTER_ID,
                                                     true,
                                                     BATTERY_PERCENTAGE_REMAINING_ATTRIBUTE_ID) == true)
    {
        if (getBoolConfigurationMetadata(configContext->configurationMetadata, CONFIGURE_BATTERY_PERCENTAGE_KEY, false))
        {
            zhalAttributeReportingConfig batteryPercentConfigs[1];
            uint8_t numConfigs = 1;

            memset(&batteryPercentConfigs[0], 0, sizeof(zhalAttributeReportingConfig));

            batteryPercentConfigs[0].attributeInfo.id = BATTERY_PERCENTAGE_REMAINING_ATTRIBUTE_ID;
            batteryPercentConfigs[0].attributeInfo.type = ZCL_INT8U_ATTRIBUTE_TYPE;
            batteryPercentConfigs[0].minInterval = 1;
            batteryPercentConfigs[0].maxInterval = REPORTING_INTERVAL_MAX;
            batteryPercentConfigs[0].reportableChange = 5;

            if (zigbeeSubsystemAttributesSetReporting(configContext->eui64,
                                                      configContext->endpointId,
                                                      POWER_CONFIGURATION_CLUSTER_ID,
                                                      batteryPercentConfigs,
                                                      numConfigs) != 0)
            {
                icLogError(LOG_TAG, "%s: failed to set reporting for battery percentage state", __FUNCTION__);
                result = false;
            }

            // Record that we configured reporting
            configuredReporting = true;
        }
    }

    uint64_t attributeId = getNumberConfigurationMetadata(
        configContext->configurationMetadata, CONFIGURE_BATTERY_RECHARGE_CYCLES_KEY, UINT64_MAX);

    // Check whether to configure battery recharge cycles reporting, default to false
    if (attributeId != UINT64_MAX)
    {
        // This attr id needs to be stored somewhere so that we can handle attribute reports later
        PowerConfigurationCluster *cluster = (PowerConfigurationCluster *) ctx;
        cluster->mfgSpecificBatteryRechargeCycleAttributeId = (uint16_t) attributeId;

        zhalAttributeReportingConfig batteryRechargeCycleConfigs[1];
        uint8_t numConfigs = 1;

        memset(&batteryRechargeCycleConfigs[0], 0, sizeof(zhalAttributeReportingConfig));

        batteryRechargeCycleConfigs[0].attributeInfo.id = (uint16_t) attributeId;
        batteryRechargeCycleConfigs[0].attributeInfo.type = ZCL_INT16U_ATTRIBUTE_TYPE;
        batteryRechargeCycleConfigs[0].minInterval = 1;
        batteryRechargeCycleConfigs[0].maxInterval = REPORTING_INTERVAL_MAX;
        batteryRechargeCycleConfigs[0].reportableChange = 1;

        if (zigbeeSubsystemAttributesSetReportingMfgSpecific(configContext->eui64,
                                                             configContext->endpointId,
                                                             POWER_CONFIGURATION_CLUSTER_ID,
                                                             COMCAST_MFG_ID,
                                                             batteryRechargeCycleConfigs,
                                                             numConfigs) != 0)
        {
            icLogError(LOG_TAG, "%s: failed to set reporting for battery recharge cycles", __FUNCTION__);
            result = false;
        }
        // Record that we configured reporting
        configuredReporting = true;
    }

    // Only worry about binding if we have configured some reporting
    if (configuredReporting == true)
    {
        // If the property is set to false we skip, otherwise accept its value or the default of true if nothing was set
        if (getBoolConfigurationMetadata(
                configContext->configurationMetadata, POWER_CONFIGURATION_CLUSTER_ENABLE_BIND_KEY, true))
        {
            if (zigbeeSubsystemBindingSet(
                    configContext->eui64, configContext->endpointId, POWER_CONFIGURATION_CLUSTER_ID) != 0)
            {
                icLogError(LOG_TAG, "%s: failed to bind power configuration cluster", __FUNCTION__);
                result = false;
            }
        }
    }
    return result;
}

static void handlePollControlCheckin(ZigbeeCluster *ctx, uint64_t eui64, uint8_t endpointId)
{
    PowerConfigurationCluster *cluster = (PowerConfigurationCluster *) ctx;

    if (cluster->callbacks->batteryVoltageUpdated != NULL)
    {
        uint8_t value;
        if (powerConfigurationClusterGetBatteryVoltage(eui64, endpointId, &value))
        {
            cluster->callbacks->batteryVoltageUpdated(cluster->callbackContext, eui64, endpointId, value);
        }
    }
}

static bool
handleAlarm(ZigbeeCluster *ctx, uint64_t eui64, uint8_t endpointId, const ZigbeeAlarmTableEntry *alarmTableEntry)
{
    bool result = true;

    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    PowerConfigurationCluster *cluster = (PowerConfigurationCluster *) ctx;

    switch (alarmTableEntry->alarmCode)
    {
        case BATTERY_BELOW_MIN_THRESHOLD:
            icLogWarn(LOG_TAG, "%s: Battery low", __FUNCTION__);
            if (cluster->callbacks->batteryChargeStatusUpdated != NULL)
            {
                cluster->callbacks->batteryChargeStatusUpdated(cluster->callbackContext, eui64, endpointId, true);
            }
            break;

        case BATTERY_NOT_AVAILABLE:
            icLogWarn(LOG_TAG, "%s: Battery missing", __FUNCTION__);
            if (cluster->callbacks->batteryMissingStatusUpdated != NULL)
            {
                cluster->callbacks->batteryMissingStatusUpdated(cluster->callbackContext, eui64, endpointId, true);
            }
            break;

        case BATTERY_BAD:
            icLogWarn(LOG_TAG, "%s: Battery bad", __FUNCTION__);
            if (cluster->callbacks->batteryBadStatusUpdated != NULL)
            {
                cluster->callbacks->batteryBadStatusUpdated(cluster->callbackContext, eui64, endpointId, true);
            }
            break;

        case AC_VOLTAGE_BELOW_MIN:
            icLogWarn(LOG_TAG, "%s: AC Voltage low", __FUNCTION__);
            if (cluster->callbacks->acMainsStatusUpdated != NULL)
            {
                cluster->callbacks->acMainsStatusUpdated(cluster->callbackContext, eui64, endpointId, false);
            }
            break;

        case BATTERY_HIGH_TEMPERATURE:
            icLogWarn(LOG_TAG, "%s: battery temperature high", __FUNCTION__);
            if (cluster->callbacks->batteryTemperatureStatusUpdated != NULL)
            {
                cluster->callbacks->batteryTemperatureStatusUpdated(cluster->callbackContext, eui64, endpointId, true);
            }
            break;

        default:
            icLogWarn(LOG_TAG,
                      "%s: Unsupported power configuration alarm code 0x%02x",
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
    bool result = true;

    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    PowerConfigurationCluster *cluster = (PowerConfigurationCluster *) ctx;

    switch (alarmTableEntry->alarmCode)
    {
        case BATTERY_BELOW_MIN_THRESHOLD:
            icLogWarn(LOG_TAG, "%s: Battery no longer low", __FUNCTION__);
            if (cluster->callbacks->batteryChargeStatusUpdated != NULL)
            {
                cluster->callbacks->batteryChargeStatusUpdated(cluster->callbackContext, eui64, endpointId, false);
            }
            break;

        case BATTERY_NOT_AVAILABLE:
            icLogWarn(LOG_TAG, "%s: Battery no longer missing", __FUNCTION__);
            if (cluster->callbacks->batteryMissingStatusUpdated != NULL)
            {
                cluster->callbacks->batteryMissingStatusUpdated(cluster->callbackContext, eui64, endpointId, false);
            }
            break;

        case BATTERY_BAD:
            icLogWarn(LOG_TAG, "%s: Battery no longer bad", __FUNCTION__);
            if (cluster->callbacks->batteryBadStatusUpdated != NULL)
            {
                cluster->callbacks->batteryBadStatusUpdated(cluster->callbackContext, eui64, endpointId, false);
            }
            break;

        case AC_VOLTAGE_BELOW_MIN:
            icLogWarn(LOG_TAG, "%s: AC Voltage low cleared", __FUNCTION__);
            if (cluster->callbacks->acMainsStatusUpdated != NULL)
            {
                cluster->callbacks->acMainsStatusUpdated(cluster->callbackContext, eui64, endpointId, true);
            }
            break;

        case BATTERY_HIGH_TEMPERATURE:
            icLogWarn(LOG_TAG, "%s: battery temperature normal", __FUNCTION__);
            if (cluster->callbacks->batteryTemperatureStatusUpdated != NULL)
            {
                cluster->callbacks->batteryTemperatureStatusUpdated(cluster->callbackContext, eui64, endpointId, false);
            }
            break;

        default:
            icLogWarn(LOG_TAG,
                      "%s: Unsupported power configuration alarm code 0x%02x",
                      __FUNCTION__,
                      alarmTableEntry->alarmCode);
            result = false;
            break;
    }

    return result;
}

static bool handleAttributeReport(ZigbeeCluster *ctx, ReceivedAttributeReport *report)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    PowerConfigurationCluster *cluster = (PowerConfigurationCluster *) ctx;

    sbZigbeeIOContext *zio = zigbeeIOInit(report->reportData, report->reportDataLen, ZIO_READ);
    uint16_t attributeId = zigbeeIOGetUint16(zio);
    uint8_t attributeType = zigbeeIOGetUint8(zio);

    icLogDebug(LOG_TAG,
               "%s: 0x%16" PRIx64 " attributeId=0x%.4" PRIx16 " attributeType=%" PRIu8,
               __FUNCTION__,
               report->eui64,
               attributeId,
               attributeType);

    if (attributeId == BATTERY_ALARM_STATE_ATTRIBUTE_ID)
    {
        if (cluster->callbacks->batteryChargeStatusUpdated != NULL)
        {
            uint32_t batteryAlarmState = zigbeeIOGetUint32(zio);
            icLogDebug(LOG_TAG, "%s: batteryAlarmState=0x%08" PRIx32, __FUNCTION__, batteryAlarmState);
            // CB-103: trigger low battery on any threshold for battery source 1 (the lower 4 bits)
            bool isLow = (batteryAlarmState & 0xf) > 0;
            cluster->callbacks->batteryChargeStatusUpdated(
                cluster->callbackContext, report->eui64, report->sourceEndpoint, isLow);
        }
    }
    else if (attributeId == BATTERY_VOLTAGE_ATTRIBUTE_ID)
    {
        if (cluster->callbacks->batteryVoltageUpdated != NULL)
        {
            uint8_t deciVolts = zigbeeIOGetUint8(zio);
            icLogDebug(LOG_TAG, "%s: batteryVoltage=%" PRIu8 " decivolts", __FUNCTION__, deciVolts);
            if (deciVolts != POWER_CONFIGURATION_CLUSTER_INVALID_VOLTAGE_VALUE)
            {
                cluster->callbacks->batteryVoltageUpdated(
                    cluster->callbackContext, report->eui64, report->sourceEndpoint, deciVolts);
            }
        }
    }
    else if (attributeId == BATTERY_PERCENTAGE_REMAINING_ATTRIBUTE_ID)
    {
        if (cluster->callbacks->batteryPercentageRemainingUpdated != NULL)
        {
            uint8_t halfIntPercent = zigbeeIOGetUint8(zio);

            cluster->callbacks->batteryPercentageRemainingUpdated(
                cluster->callbackContext, report->eui64, report->sourceEndpoint, halfIntPercent);
        }
    }
    else if (report->mfgId == COMCAST_MFG_ID && attributeId == cluster->mfgSpecificBatteryRechargeCycleAttributeId)
    {
        if (cluster->callbacks->batteryRechargeCyclesChanged != NULL)
        {
            uint16_t attrValue = zigbeeIOGetUint16(zio);
            icLogDebug(LOG_TAG, "%s: batteryRechargeCycles=%" PRIu16, __FUNCTION__, attrValue);
            cluster->callbacks->batteryRechargeCyclesChanged(cluster->callbackContext, report->eui64, attrValue);
        }
    }
    return true;
}

int powerConfigurationClusterReadBatteryRechargeCyclesInitialValue(uint64_t eui64,
                                                                   uint8_t endpointId,
                                                                   uint64_t *value,
                                                                   uint16_t attributeId)
{
    return zigbeeSubsystemReadNumberMfgSpecific(
        eui64, endpointId, POWER_CONFIGURATION_CLUSTER_ID, COMCAST_MFG_ID, true, attributeId, value);
}

#endif // BARTON_CONFIG_ZIGBEE
