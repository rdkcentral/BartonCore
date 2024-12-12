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
// Created by tlea on 3/1/19
//

#ifndef ZILKER_THERMOSTATCLUSTER_H
#define ZILKER_THERMOSTATCLUSTER_H


#ifdef BARTON_CONFIG_ZIGBEE

#include "zigbeeCluster.h"

typedef struct
{
    void (*localTemperatureChanged)(uint64_t eui64, uint8_t endpointId, int16_t temp, const void *ctx);

    void (*occupiedHeatingSetpointChanged)(uint64_t eui64, uint8_t endpointId, int16_t temp, const void *ctx);

    void (*occupiedCoolingSetpointChanged)(uint64_t eui64, uint8_t endpointId, int16_t temp, const void *ctx);

    void (*systemModeChanged)(uint64_t eui64, uint8_t endpointId, uint8_t mode, const void *ctx);

    void (*runningStateChanged)(uint64_t eui64, uint8_t endpointId, uint16_t state, const void *ctx);

    void (*setpointHoldChanged)(uint64_t eui64, uint8_t endpointId, bool holdOn, const void *ctx);

    void (*ctrlSeqOpChanged)(uint64_t eui64, uint8_t endpointId, uint8_t ctrlSeqOp, const void *ctx);

    void (*legacyOperationInfoReceived)(uint64_t eui64,
                                        uint8_t endpointId,
                                        uint8_t runningMode, // 0=off, 1=heat, 2=cool
                                        bool holdOn,
                                        uint8_t runningState,     // 0=off, 1=heat, 2=cool, 0xff=not used
                                        uint8_t fanRunningState); // 0=off, 1=running, 0xff=not used

    void (*localTemperatureCalibrationChanged)(uint64_t eui64,
                                               uint8_t endpointId,
                                               int8_t calibrationTemp,
                                               const void *ctx);
} ThermostatClusterCallbacks;


ZigbeeCluster *thermostatClusterCreate(const ThermostatClusterCallbacks *callbacks, void *callbackContext);

/**
 * Set whether or not to set a binding on this cluster.  By default we bind the cluster.
 * @param deviceConfigurationContext the configuration context
 * @param bind true to bind or false not to
 */
void thermostatClusterSetBindingEnabled(const DeviceConfigurationContext *deviceConfigurationContext, bool bind);

bool thermostatClusterGetRunningState(const ZigbeeCluster *cluster,
                                      uint64_t eui64,
                                      uint8_t endpointId,
                                      uint16_t *state);

bool thermostatClusterGetSystemMode(const ZigbeeCluster *cluster, uint64_t eui64, uint8_t endpointId, uint8_t *mode);

bool thermostatClusterSetSystemMode(const ZigbeeCluster *cluster, uint64_t eui64, uint8_t endpointId, uint8_t mode);

bool thermostatClusterIsHoldOn(const ZigbeeCluster *cluster, uint64_t eui64, uint8_t endpointId, bool *isHoldOn);

bool thermostatClusterSetHold(const ZigbeeCluster *cluster, uint64_t eui64, uint8_t endpointId, bool holdOn);

bool thermostatClusterGetLocalTemperature(const ZigbeeCluster *cluster,
                                          uint64_t eui64,
                                          uint8_t endpointId,
                                          int16_t *temp);

bool thermostatClusterGetLocalTemperatureCalibration(const ZigbeeCluster *cluster,
                                                     uint64_t eui64,
                                                     uint8_t endpointId,
                                                     int8_t *calibration);

bool thermostatClusterSetLocalTemperatureCalibration(const ZigbeeCluster *cluster,
                                                     uint64_t eui64,
                                                     uint8_t endpointId,
                                                     int8_t calibration);

bool thermostatClusterGetAbsMinHeatSetpoint(const ZigbeeCluster *cluster,
                                            uint64_t eui64,
                                            uint8_t endpointId,
                                            int16_t *temp);

bool thermostatClusterGetAbsMaxHeatSetpoint(const ZigbeeCluster *cluster,
                                            uint64_t eui64,
                                            uint8_t endpointId,
                                            int16_t *temp);

bool thermostatClusterGetAbsMinCoolSetpoint(const ZigbeeCluster *cluster,
                                            uint64_t eui64,
                                            uint8_t endpointId,
                                            int16_t *temp);

bool thermostatClusterGetAbsMaxCoolSetpoint(const ZigbeeCluster *cluster,
                                            uint64_t eui64,
                                            uint8_t endpointId,
                                            int16_t *temp);

bool thermostatClusterGetOccupiedHeatingSetpoint(const ZigbeeCluster *cluster,
                                                 uint64_t eui64,
                                                 uint8_t endpointId,
                                                 int16_t *temp);

bool thermostatClusterGetOccupiedCoolingSetpoint(const ZigbeeCluster *cluster,
                                                 uint64_t eui64,
                                                 uint8_t endpointId,
                                                 int16_t *temp);

bool thermostatClusterSetOccupiedHeatingSetpoint(const ZigbeeCluster *cluster,
                                                 uint64_t eui64,
                                                 uint8_t endpointId,
                                                 int16_t temp);

bool thermostatClusterSetOccupiedCoolingSetpoint(const ZigbeeCluster *cluster,
                                                 uint64_t eui64,
                                                 uint8_t endpointId,
                                                 int16_t temp);

bool thermostatClusterGetCtrlSeqOp(const ZigbeeCluster *cluster,
                                   uint64_t eui64,
                                   uint8_t endpointId,
                                   uint8_t *ctrlSeqOp);

bool thermostatClusterSetCtrlSeqOp(const ZigbeeCluster *cluster, uint64_t eui64, uint8_t endpointId, uint8_t ctrlSeqOp);

// this is a manufacturer specific command for RTCoA thermostats
bool thermostatClusterSetAbsoluteSetpointModeLegacy(const ZigbeeCluster *cluster, uint64_t eui64, uint8_t endpointId);

// this is a manufacturer specific command for RTCoA and CentraLite thermostats
bool thermostatClusterSetPollRateLegacy(const ZigbeeCluster *cluster,
                                        uint64_t eui64,
                                        uint8_t endpointId,
                                        uint16_t quarterSeconds);

// this is a manufacturer specific command for RTCoA and CentraLite thermostats. It triggers an operational info
// command to be sent to us.
bool thermostatClusterRequestOperationalInfoLegacy(const ZigbeeCluster *cluster, uint64_t eui64, uint8_t endpointId);

const char *thermostatClusterGetSystemState(uint16_t runningState);

bool thermostatClusterIsFanOn(uint16_t runningState);

const char *thermostatClusterGetSystemModeString(uint8_t systemMode);

// this only returns the values we support setting, not the entire enum
uint8_t thermostatClusterGetSystemModeFromString(const char *systemMode);

// return a string representing the temperature in celcius * 100
char *thermostatClusterGetTemperatureString(int16_t temperature);

// parse a string representing the temperature in celcius * 100
bool thermostatClusterGetTemperatureValue(const char *temperatureString, int16_t *temp);

const char *thermostatClusterGetCtrlSeqOpString(uint8_t ctrlSeqOp);

uint8_t thermostatClusterGetCtrlSeqOpFromString(const char *ctrlSeqOp);

// this only returns the values we support setting, not the entire enum
uint8_t thermostatClusterGetFanModeFromString(const char *fanMode);

#endif // BARTON_CONFIG_ZIGBEE

#endif // ZILKER_THERMOSTATCLUSTER_H
