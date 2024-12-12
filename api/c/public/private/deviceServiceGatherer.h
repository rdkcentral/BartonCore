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
// Created by jelder380 on 1/23/19.
//


#ifndef ZILKER_DEVICESERVICEGATHERER_H
#define ZILKER_DEVICESERVICEGATHERER_H

#ifdef BARTON_CONFIG_ZIGBEE

#include <glib.h>

/**
 * Collects all information about each device
 * on the system, which is added to the
 * runtimeStats output per device.
 *
 * Each list added contains the following:
 *
 * The 5 most recent rejoin info,
 * The 5 most recent check-in info,
 * The 8 most recent attribute reports,
 * All of the devices endpoints and resources,
 * The device counters:
 *      Total rejoins for device,
 *      Total Secure rejoins for device,
 *      Total Un-secure rejoins for device,
 *      Total Duplicate Sequence Numbers for device,
 *      Total Aps Ack Failures for device
 *
 * NOTE: each device list has to be in the format:
 *
 * "uuid,manufacturer,model,firmwareVersion,
 * AttributeReportTime1,ClusterId1,AttributeId1,Data1,
 * AttributeReportTime2,ClusterId2,AttributeId2,Data2,
 * AttributeReportTime3,ClusterId3,AttributeId3,Data3,
 * AttributeReportTime4,ClusterId4,AttributeId4,Data4,
 * AttributeReportTime5,ClusterId5,AttributeId5,Data5,
 * AttributeReportTime6,ClusterId6,AttributeId6,Data6,
 * AttributeReportTime7,ClusterId7,AttributeId7,Data7,
 * AttributeReportTime8,ClusterId8,AttributeId8,Data8,
 * rejoinTime1,isSecure1,
 * rejoinTime2,isSecure2,
 * rejoinTime3,isSecure3,
 * rejoinTime4,isSecure4,
 * rejoinTime5,isSecure5,
 * checkInTime1,
 * checkInTime2,
 * checkInTime3,
 * checkInTime4,
 * checkInTime5,
 * type,hardwareVersion,lqi(ne/fe),
 * rssi(ne/fe),temperature,batteryVoltage,
 * lowBattery,commFailure,troubled,
 * bypassed,tampered,faulted,
 * totalRejoinCounter,
 * totalSecureRejoinCounter,
 * totalUnSecureRejoinCounter,
 * totalDuplicateSequenceNumberCounter,
 * totalApsAckFailureCounter,
 * cumulativeSensorDelayQS"
 *
 * @param output - the runtimeStatistics hashMap
 */
void collectAllDeviceStatistics(GHashTable *output);

/**
 * Collect all of the Zigbee counters from Zigbee core
 *
 * @param strings - the runtimeStatistics strings
 * @param ints - the runtimeStatistics ints
 */
void collectZigbeeNetworkCounters(GHashTable *strings, GHashTable *ints);

/**
 * Collect all of the Zigbee Core Network status
 * includes panID, channel, openForJoin, network up,
 * and eui64
 *
 * @param strings - the runtimeStatistics strings
 * @param longs - the runtimeStatistics longs
 */
void collectZigbeeCoreNetworkStatistics(GHashTable *strings, GHashTable *longs);

/**
 * Collect all of the Zigbee device firmware failures
 *
 * @param ints - the runtimeStatistics ints
 * @param longs - the runtimeStatistics longs
 */
void collectAllDeviceFirmwareEvents(GHashTable *ints, GHashTable *longs);

/**
 * Collect zigbee channel status and add them into
 * the runtime stats hash map
 *
 * @param ints
 */
void collectChannelScanStats(GHashTable *ints);

/**
 * Collects stats about Cameras and add them into
 * the runtime stats hash map
 *
 * @param output - the hash map container
 */
void collectCameraDeviceStats(GHashTable *output);

/**
 * Collect zigbee network map and add them into
 * the runtime stats hash map
 * The format is EUI64,Next Hop EUI64,LQI,
 * for every device
 *
 * @param output
 */
void collectZigbeeNetworkMap(GHashTable *output);

/**
 * Add device split markers by subystem, model and count
 *
 * @param output - the runtimeStatistics hashMap containing connected device split markers by subsytem, model and count
 * e.g.
 * {"TotalSensorCount_split" "subsystem,14"; }
 * {"TotalSubsytemSensor_split" "XHS2-SE,4;XCAM1,3;XCAM2,3;SMCWK01-Z,4;"}
 *
 */
void collectPairedDevicesInformation(GHashTable *output);

/**
 * Add camera split markers by model and count
 *
 * @param output - the runtimeStatistics hashMap containing connected camera device split markers by model and count
 * e.g.
 * {"TotalLocalCameraCount": "2"}
 * {"TotalLocalCamera": "XCam1,1;ICam2C,1;"}
 *
 */
void collectPairedCamerasInformation(GHashTable *output);

#endif // BARTON_CONFIG_ZIGBEE

#endif // ZILKER_DEVICESERVICEGATHERER_H
