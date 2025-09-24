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

/*-----------------------------------------------
 * zigbeeEventTracker.h
 *
 * Implements functions to track, and collect zigbee events comming in
 * from zigbeeSubsystem, which gets events from zigbeeEventHandler,
 * which gets events from zhal.
 *
 * Uses the property 'cpe.zigbee.reportDeviceInfo.enabled'
 * to turn on/off collecting the reporting events.
 *
 * Also does channel scans based on the various properties associated
 * with Zigbee Data Diagnostic.
 *
 * Uses the property 'cpe.diagnostics.zigBeeData.enabled'
 * to turn on/off the channel scans.
 *
 * Author: jelder380 - 2/21/19.
 *-----------------------------------------------*/

#pragma once

#include <icTypes/icLinkedList.h>
#include <time.h>
#include <zhal/zhal.h>

// max number of rejoin values, check-in values and attribute report values
#define MAX_NUMBER_OF_ATTRIBUTE_REPORTS 8
#define MAX_NUMBER_OF_REJOINS           5
#define MAX_NUMBER_OF_CHECK_INS         5

/*
 * The device Attribute item
 */
typedef struct
{
    char *reportTime;
    char *data;
    char *clusterId;
    char *attributeId;
} deviceAttributeItem;

/*
 * The device rejoin item
 */
typedef struct
{
    char *rejoinTime;
    char *isSecure;
} deviceRejoinItem;

/*
 * The upgrade failure item
 */
typedef struct
{
    time_t failureTime;
    char *deviceId;
} deviceUpgFailureItem;

/*
 * The event counters item
 */
typedef struct
{
    uint32_t totalRejoinEvents;
    uint32_t totalSecureRejoinEvents;
    uint32_t totalUnSecureRejoinEvents;
    uint32_t totalApsAckFailureEvents;
    uint32_t totalDuplicateSeqNumEvents;
    uint32_t cumulativeSensorDelayQS;
} deviceEventCounterItem;

/*
 * The Channel Energy Scan Data result item
 */
typedef struct
{
    uint8_t channel;
    int8_t max;
    int8_t min;
    int8_t average;
} channelEnergyScanDataItem;

/**
 * Adds attribute report for NON-SENSOR Devices into our collection
 * It adds the item based off of Device UUID or EUI64.
 *
 * @param report - the attribute report received
 */
void zigbeeEventTrackerAddAttributeReportEvent(ReceivedAttributeReport *report);

/**
 * Adds rejoin event per device,
 * should be added for every device
 *
 * @param eui64 - the device eui64
 */
void zigbeeEventTrackerAddRejoinEvent(uint64_t eui64, bool wasSecure);

/**
 * Adds check-in event and/or duplicate sequence number event
 * per device, should be added for every device
 *
 * @param command - the clusterCommand received
 */
void zigbeeEventTrackerAddClusterCommandEvent(ReceivedClusterCommand *command);

/**
 * Adds magnetic strength reports per device,
 * should be added for only for devices that support magnetic strength measurement
 *
 * @param deviceUuid - the device uuid
 * @param valueString - Magnetic Strength string. Format: <device model,magnetic strength>
 */
void zigbeeEventTrackerAddMagneticStrengthReport(const char *deviceUuid, const char *valueString);

/**
 * Adds apsAckFailure events per device,
 * should be added for every device
 *
 * @param eui64 - the device eui64
 */
void zigbeeEventTrackerAddApsAckFailureEvent(uint64_t eui64);

/**
 * Adds a count for how many devices have a successful
 * upgrade, should be for every device
 */
void zigbeeEventTrackerAddDeviceFirmwareUpgradeSuccessEvent();

/**
 * Adds a device upgrade failure per device,
 * should be for every device
 *
 * @param eui64 - the device eui64
 */
void zigbeeEventTrackerAddDeviceFirmwareUpgradeFailureEvent(uint64_t eui64);

/**
 * Collects the attribute reports for device,
 * only has attribute reports for non-sensor devices
 * The number of events is controlled by
 * MAX_NUMBER_OF_ATTRIBUTE_REPORTS
 *
 * NOTE - caller must free return object if not NULL
 * via destroyDeviceAttributeItem
 *
 * @param deviceId - the device ID to grab
 * @return - returns a copy of the deviceAttributeItems list
 */
icLinkedList *zigbeeEventTrackerCollectAttributeReportEventsForDevice(const char *deviceId);

/**
 * Collects the rejoins for device
 * The number of events is controlled by
 * MAX_NUMBER_OF_REJOINS
 *
 * NOTE - caller must free return object if not NULL
 * via destroyDeviceRejoinItem
 *
 * @param deviceId - the device ID to grab
 * @return - returns a copy of the deviceRejoinItems list
 */
icLinkedList *zigbeeEventTrackerCollectRejoinEventsForDevice(const char *deviceId);

/**
 * Collects the check-ins for device
 * The number of events is controlled by
 * MAX_NUMBER_OF_CHECK_INS
 *
 * NOTE - caller must free return object if not NULL
 * via standardFree function
 *
 * @param deviceId - the device ID to grab
 * @return - returns a copy of the string list
 */
icLinkedList *zigbeeEventTrackerCollectCheckInEventsForDevice(const char *deviceId);

/**
 * Collects the magnetic strength report for device
 *
 * NOTE - caller must free return string if not NULL
 *
 * @param deviceId - the device ID to grab
 * @return - returns a copy of the magnetic strength string
 */
char *zigbeeEventTrackerCollectMagneticStrengthReportsForDevice(const char *deviceId);

/**
 * Collects all the event counters for device.
 * Should be for every device, numbers will reset on reboot
 *
 * NOTE: caller must free return object if not null
 * via standardFree function
 *
 * @param deviceId - the device
 * @return - a copy of deviceEventCounterItem
 */
deviceEventCounterItem *zigbeeEventTrackerCollectEventCountersForDevice(const char *deviceId);

/**
 * Function to reset the sensor delay counter for device after a telemetry upload
 *
 * @param deviceId - the device ID to grab
 */
void zigbeeEventTrackerResetSensorDelayCountersForDevice(const char *deviceId);

/**
 * Collects all the successful device upgrade events,
 * will reset the number once this is called
 *
 * @return - the number of successful upgrades
 */
int zigbeeEventTrackerCollectFirmwareUpgradeSuccessEvents();

/**
 * Collects all the failures device upgrade events,
 * will reset the the list once this is called
 *
 * NOTE: caller must free return object if not NULL
 * via destroyDeviceUpgFailureItem
 *
 * @return - returns a copy of deviceUpgFailureItems list
 */
icLinkedList *zigbeeEventTrackerCollectFirmwareUpgradeFailureEvents();

/**
 * Collects all of the channel energy scan stats
 *
 * NOTE: caller must free return object if not NULL
 * via standardFree function
 *
 * @return - returns a copy of channelEnergyScanCollection
 */
icLinkedList *zigbeeEventTrackerCollectChannelEnergyScanStats();

void zigbeeEventTrackerPropertyCallback(const char *propertyName, const char *propertyValue);

/**
 * function for freeing memory for the device attribute item
 *
 * @param item - the item to destroy
 */
void destroyDeviceAttributeItem(void *item);

/**
 * function for freeing memory for the device rejoin item
 *
 * @param item - the item to destroy
 */
void destroyDeviceRejoinItem(void *item);

/**
 * function for freeing memory for the upgrade failure item
 *
 * @param item - the item to destroy
 */
void destroyDeviceUpgFailureItem(void *item);

/**
 * Used to set up the event tracker
 */
void initEventTracker();

/**
 * Used to clean up all of the collections
 */
void shutDownEventTracker();
