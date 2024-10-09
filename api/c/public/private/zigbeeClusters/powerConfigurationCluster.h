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

#ifndef ZILKER_POWERCONFIGURATIONCLUSTER_H
#define ZILKER_POWERCONFIGURATIONCLUSTER_H

#include "zigbeeCluster.h"

typedef struct
{
    void (*batteryVoltageUpdated)(void *ctx, uint64_t eui64, uint8_t endpointId, uint8_t decivolts);
    void (*batteryPercentageRemainingUpdated)(void *ctx, uint64_t eui64, uint8_t endpointId, uint8_t halfIntPercent);
    void (*batteryChargeStatusUpdated)(void *ctx, uint64_t eui64, uint8_t endpointId, bool isLow);
    void (*batteryBadStatusUpdated)(void *ctx, uint64_t eui64, uint8_t endpointId, bool isBad);
    void (*batteryMissingStatusUpdated)(void *ctx, uint64_t eui64, uint8_t endpointId, bool isMissing);
    void (*batteryTemperatureStatusUpdated)(void *ctx, uint64_t eui64, uint8_t endpointId, bool isHigh);
    void (*acMainsStatusUpdated)(void *ctx, uint64_t eui64, uint8_t endpointId, bool isAcMainsConnected);
    void (*batteryRechargeCyclesChanged)(void *ctx, uint64_t eui64, uint16_t rechargeCycles);
} PowerConfigurationClusterCallbacks;

/**
 *  This structure holds the battery related thresholds that we care about.  If any of these are NULL then they were
 *  unable to be read most likely due to not being supported.  Use powerConfigurationClusterBatteryThresholdsDestroy
 *  to free.
 */
typedef struct
{
    uint8_t *minThresholdDecivolts;
    uint8_t *threshold1Decivolts;
    uint8_t *threshold2Decivolts;
    uint8_t *threshold3Decivolts;
    uint8_t *minThresholdPercent;
    uint8_t *threshold1Percent;
    uint8_t *threshold2Percent;
    uint8_t *threshold3Percent;
} PowerConfigurationClusterBatteryThresholds;

ZigbeeCluster *powerConfigurationClusterCreate(const PowerConfigurationClusterCallbacks *callbacks,
                                               void *callbackContext);

/**
 * Set whether or not to set a binding on this cluster.  By default we bind the cluster.
 * @param deviceConfigurationContext the configuration context
 * @param bind true to bind or false not to
 */
void powerConfigurationClusterSetBindingEnabled(const DeviceConfigurationContext *deviceConfigurationContext,
                                                bool bind);

/**
 * Retrieves the current battery voltage from the device.
 * @param eui64
 * @param endpointId
 * @param decivolts voltage value read
 * @return true if valid value for voltage is reported, otherwise false
 */
bool powerConfigurationClusterGetBatteryVoltage(uint64_t eui64, uint8_t endpointId, uint8_t *decivolts);

/**
 * Retrieves the remaining battery life as a percentage of the full battery capacity.
 * @param eui64
 * @param endpointId
 * @param halfIntPercent the percentage capacity remaining in half integer precision (e.g., 69 = 34.5%, 100 = 50%, 200 =
 * 100%)
 * @return
 */
bool powerConfigurationClusterGetBatteryPercentageRemaining(uint64_t eui64,
                                                            uint8_t endpointId,
                                                            uint8_t *halfIntPercent);

/**
 * Retrieves the current ac mains voltage from the device.
 * @param eui64
 * @param endpointId
 * @param decivolts
 * @return
 */
bool powerConfigurationClusterGetMainsVoltage(uint64_t eui64, uint8_t endpointId, uint16_t *decivolts);

/**
 * Set whether or not to configure battery alarm state reporting.  By default battery alarm state reporting will be
 * configured unless explicitly told not to
 * @param deviceConfigurationContext the configuration context
 * @param configure true to configure, false otherwise
 */
void powerConfigurationClusterSetConfigureBatteryAlarmState(
    const DeviceConfigurationContext *deviceConfigurationContext,
    bool configure);

/**
 * Set whether or not to configure battery alarm mask.  By default battery alarm mask will not be configured unless
 * explicitly told to
 * @param deviceConfigurationContext the configuration context
 * @param configure true to configure, false otherwise
 */
void powerConfigurationClusterSetConfigureBatteryAlarmMask(const DeviceConfigurationContext *deviceConfigurationContext,
                                                           bool configure);

/**
 * Set whether or not to configure battery voltage reporting.  By default battery voltage will not be configured unless
 * explicitly told to
 * @param deviceConfigurationContext the configuration context
 * @param configure true to configure, false otherwise
 */
void powerConfigurationClusterSetConfigureBatteryVoltage(const DeviceConfigurationContext *deviceConfigurationContext,
                                                         bool configure);

/**
 * Set whether or not to configure battery percent remaining reports.  By default, reports are disabled.
 * @param deviceConfigurationContext the configuration context
 * @param configure true to configure, false otherwise
 */
void powerConfigurationClusterSetConfigureBatteryPercentage(
    const DeviceConfigurationContext *deviceConfigurationContext,
    bool configure);

/**
 * Set whether or not to configure battery recharge cycle reports. By default, reports are disabled.
 * @param deviceConfigurationContext the configuration context
 * @param configure true to configure, false otherwise
 */
void powerConfigurationClusterSetConfigureBatteryRechargeCycles(
    const DeviceConfigurationContext *deviceConfigurationContext,
    bool configure);

/**
 * Set the max interval for the battery voltage configuration. By default, it's 27 minutes.
 * @param deviceConfigurationContext the configuration context
 * @param max interval
 */
void powerConfigurationClusterSetConfigureBatteryVoltageMaxInterval(
    const DeviceConfigurationContext *deviceConfigurationContext,
    uint64_t interval);

/**
 * Gives initial recharge cycle value
 *
 * @param eui64
 * @param endpointId
 * @param rechargeCycles
 *
 * @return 0 if read was successful, -1 otherwise
 */

int powerConfigurationClusterReadBatteryRechargeCyclesInitialValue(uint64_t eui64,
                                                                   uint8_t endpointId,
                                                                   uint64_t *rechargeCycles);

/**
 * Read battery alarm thresholds
 *
 * @param eui64
 * @param endpointId
 *
 * @return allocated PowerConfigurationClusterBatteryThresholds struct if any related attributes are available on
 * the device or NULL on failure.
 *
 * Note: returned struct needs to be destroyed by calling powerConfigurationClusterBatteryThresholdsDestroy.
 */
PowerConfigurationClusterBatteryThresholds *powerConfigurationClusterGetBatteryThresholds(uint64_t eui64,
                                                                                          uint8_t endpointId);

/**
 * destroy struct returned from powerConfigurationClusterGetBatteryThresholds function.
 */
void powerConfigurationClusterBatteryThresholdsDestroy(PowerConfigurationClusterBatteryThresholds *thresholds);

#endif // ZILKER_POWERCONFIGURATIONCLUSTER_H
