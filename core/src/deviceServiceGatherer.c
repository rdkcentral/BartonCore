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
// Created by jelder380 on 1/17/19.
//

#ifdef BARTON_CONFIG_ZIGBEE

#include <ctype.h>
#include <glib.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include "deviceService.h"
#include "deviceServiceGatherer.h"
#include "deviceServicePrivate.h"
#include "jsonHelper/jsonHelper.h"
#include "subsystems/zigbee/zigbeeEventTracker.h"
#include "subsystems/zigbee/zigbeeSubsystem.h"
#include <cjson/cJSON.h>
#include <commonDeviceDefs.h>
#include <deviceDriverManager.h>
#include <icConcurrent/delayedTask.h>
#include <icLog/logging.h>
#include <icTypes/icHashMap.h>
#include <icTypes/icLinkedList.h>
#include <icTypes/icStringBuffer.h>
#include <icUtil/numberUtils.h>
#include <icUtil/stringUtils.h>
#include <zhal/zhal.h>

#define LOG_TAG                           "deviceServiceGather"

// the device service / zigbee core keys
#define DEVICE_TITLE_KEY                  "Device"
#define PAN_ID_KEY                        "panid"
#define OPEN_FOR_JOIN_KEY                 "openForJoin"
#define IS_ZIGBEE_NET_CONFIGURED_KEY      "isConfigured"
#define IS_ZIGBEE_NET_AVAILABLE_KEY       "isAvailable"
#define IS_ZIGBEE_NETWORK_UP_KEY          "networkUp"
#define EUI_64_KEY                        "eui64"
#define CHANNEL_KEY                       "channel"
#define DEVICE_FW_UPGRADE_FAIL_CNT_KEY    "zigbeeDevFwUpgFails"
#define DEVICE_FW_UPGRADE_SUCCESS_CNT_KEY "zigbeeDevFwUpgSuccesses"
#define DEVICE_FW_UPGRADE_FAILURE_KEY     "zigbeeDevFwUpgFail_"
#define CHANNEL_SCAN_MAX_KEY              "emaxc"
#define CHANNEL_SCAN_MIN_KEY              "eminc"
#define CHANNEL_SCAN_AVG_KEY              "eavgc"
#define CAMERA_TOTAL_DEVICE_LIST_KEY      "cameraDeviceList"
#define CAMERA_CONNECTED_LIST_KEY         "cameraConnectedList"
#define CAMERA_DISCONNECTED_LIST_KEY      "cameraDisconnectedList"
#define ZIGBEE_NETWORK_MAP_KEY            "zigbeeNetworkMap"
#define ZIGBEE_DEVICE_NETWORK_HEALTH_KEY  "zigbeeDeviceHealthStats"
#define ZIGBEE_NETWORK_COUNTERS_KEY       "zigbeeCounters"
#define MAGNETIC_STRENGTH_KEY             "halleffect"

// Zigbee Counter Keys in a list
// order is important
//
static const char *const zigbeeNetworkCountersKeys[] = {"sampleIntervalMillis",
                                                        "macRxBcast",
                                                        "macTxBcast",
                                                        "macRxUcast",
                                                        "macTxUcastSuccesses",
                                                        "macTxUcastRetries",
                                                        "macTxUcastFailures",
                                                        "apsRxBcast",
                                                        "apsTxBcast",
                                                        "apsRxUcast",
                                                        "apsTxUcastSuccesses",
                                                        "apsTxUcastRetries",
                                                        "apsTxUcastFailures",
                                                        "nwkFrameCounterFailures",
                                                        "apsFrameCounterFailures",
                                                        "nwkLinkKeyNotAuthorized",
                                                        "nwkDecryptionFailures",
                                                        "apsDecryptionFailures",
                                                        "bufferAllocationFailures",
                                                        "queueOverruns",
                                                        "packetValidationFailures",
                                                        "nwkRetryQueueOverflows",
                                                        "phyCcaFailures",
                                                        "badChanCnt"};


// device types to ignore
#define IGNORE_INTEGRATED_PIEZO_DEVICE      "intPiezoDD"

// null value in device resource
#define DEVICE_NULL_VALUE                   "(null)"
#define VALUE_IS_EMPTY_STRING               ""

// constants
#define INITIAL_CAMERA_BUFFER_SIZE          18 // size of MAC Address with collons and null char

// battery state strings
#define BATTERY_STATE_GOOD_STRING           "GOOD"
#define BATTERY_STATE_LOW_STRING            "LOW"
#define BATTERY_STATE_BAD_STRING            "BAD"

#define MAX_ZIGBEE_NETWORK_MAP_LENGTH       38 // size of two eui64, max lqi and commas

#define MIN_ZIGBEE_NETWORK_MAP_COLLECT_HOUR 1
#define MAX_ZIGBEE_NETWORK_MAP_COLLECT_HOUR 24

static pthread_mutex_t ZIGBEE_NETWORK_MAP_MTX = PTHREAD_MUTEX_INITIALIZER;
static uint32_t zigbeeMapGathererTask = 0;
static char *zigbeeNetworkMapStr = NULL;

/*
 * structs for all of the device information
 *
 * NOTE: only points to the strings, no memory is needed to be created
 */

typedef struct
{
    const char *manufacturer;
    const char *model;
    const char *firmVer;
    const char *hardVer;
    const char *nearLqi;
    const char *farLqi;
    const char *nearRssi;
    const char *farRssi;
    const char *temp;
    const char *batteryVolts;
    const char *lowBattery;
    const char *commFail;
    const char *troubled;
    const char *bypassed;
    const char *tampered;
    const char *faulted;
    const char *upgradeStatus;
    const char *batteryState;
} basicDeviceInfo;

typedef struct
{
    const char *macAddress;
    const char *commFail;
} cameraDeviceInfo;

typedef struct
{
    const char *reportTime;
    const char *clusterId;
    const char *attributeId;
    const char *data;
} attributeDeviceInfo;

typedef struct
{
    const char *time;
    const char *isSecure;
} rejoinDeviceInfo;

typedef struct
{
    const char *time;
} checkInDeviceInfo;

typedef struct
{
    char *model;
    uint16_t count;
} deviceModelListItem;

// private functions
static const char *customStringToString(const char *src);
static void initDeviceInfo(basicDeviceInfo *deviceInfo);
static void initCameraDeviceInfo(cameraDeviceInfo *deviceInfo);
static void initAttDeviceInfo(attributeDeviceInfo *attDeviceInfo, int size);
static void initRejoinDeviceInfo(rejoinDeviceInfo *rejoinInfo, int size);
static void initCheckInDeviceInfo(checkInDeviceInfo *checkInInfo, int size);
static void collectResources(basicDeviceInfo *deviceInfo, const icDevice *device);
static void collectEndpointResources(basicDeviceInfo *deviceInfo, const icDevice *device);
static void collectCameraResources(cameraDeviceInfo *deviceInfo, const icDevice *device);
static void convertAttributeReports(icLinkedList *attList, attributeDeviceInfo *deviceInfo);
static void convertRejoins(icLinkedList *rejoinList, rejoinDeviceInfo *deviceInfo);
static void convertCheckIns(icLinkedList *checkInList, checkInDeviceInfo *deviceInfo);
static char *createDeviceStringList(const char *uuid,
                                    basicDeviceInfo *deviceInfo,
                                    attributeDeviceInfo *attList,
                                    rejoinDeviceInfo *rejoinList,
                                    checkInDeviceInfo *checkInList,
                                    deviceEventCounterItem *deviceCounters,
                                    const char *deviceClass);
static void gatherZigbeeNetworkMapCallback(void *arg);
static void gatherZigbeeNetworkMap(void);

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
 * "uuid,manufacturer,model,firmwareVersion,upgradeStatus,
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
 * rssi(ne/fe),temperature,batteryVoltage,batteryState,
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
void collectAllDeviceStatistics(GHashTable *output)
{
    g_return_if_fail(output != NULL);

    scoped_icStringBuffer *magStrengthReports = stringBufferCreate(0);
    scoped_icStringBuffer *zigbeeDeviceNetworkHealth = stringBufferCreate(0);

    scoped_icDeviceList *deviceList = deviceServiceGetAllDevices();
    scoped_icLinkedListIterator *deviceIter = linkedListIteratorCreate(deviceList);
    int deviceCount = 0;

    while (linkedListIteratorHasNext(deviceIter))
    {
        const icDevice *device = linkedListIteratorGetNext(deviceIter);

        if (device->uuid == NULL || device->deviceClass == NULL)
        {
            continue;
        }

        DeviceDriver *driver = deviceDriverManagerGetDeviceDriver(device->managingDeviceDriver);

        if (driver != NULL && stringCompare(driver->subsystemName, ZIGBEE_SUBSYSTEM_NAME, false) != 0)
        {
            continue;
        }

        deviceCount++;

        basicDeviceInfo statList;
        initDeviceInfo(&statList);

        attributeDeviceInfo attDeviceInfo[MAX_NUMBER_OF_ATTRIBUTE_REPORTS];
        initAttDeviceInfo(attDeviceInfo, MAX_NUMBER_OF_ATTRIBUTE_REPORTS);

        rejoinDeviceInfo rejoinInfo[MAX_NUMBER_OF_REJOINS];
        initRejoinDeviceInfo(rejoinInfo, MAX_NUMBER_OF_REJOINS);

        checkInDeviceInfo checkInInfo[MAX_NUMBER_OF_CHECK_INS];
        initCheckInDeviceInfo(checkInInfo, MAX_NUMBER_OF_CHECK_INS);


        collectResources(&statList, device);
        collectEndpointResources(&statList, device);

        scoped_generic deviceEventCounterItem *deviceEventCounters =
            zigbeeEventTrackerCollectEventCountersForDevice(device->uuid);

        icLinkedList *attList = zigbeeEventTrackerCollectAttributeReportEventsForDevice(device->uuid);
        convertAttributeReports(attList, attDeviceInfo);

        icLinkedList *rejoinList = zigbeeEventTrackerCollectRejoinEventsForDevice(device->uuid);
        convertRejoins(rejoinList, rejoinInfo);

        scoped_icLinkedListGeneric *checkInList = zigbeeEventTrackerCollectCheckInEventsForDevice(device->uuid);
        convertCheckIns(checkInList, checkInInfo);

        scoped_generic char *deviceInfoList = createDeviceStringList(
            device->uuid, &statList, attDeviceInfo, rejoinInfo, checkInInfo, deviceEventCounters, device->deviceClass);

        if (deviceInfoList != NULL)
        {
            scoped_generic char *listTag = stringBuilder("%s %d", DEVICE_TITLE_KEY, deviceCount);
            g_hash_table_insert(output, g_steal_pointer(&listTag), g_steal_pointer(&deviceInfoList));
        }
        else
        {
            icLogWarn(LOG_TAG,
                      "%s: was unable to create device information string list for device %s",
                      __FUNCTION__,
                      device->uuid);
        }

        scoped_generic char *magneticStrength = zigbeeEventTrackerCollectMagneticStrengthReportsForDevice(device->uuid);

        if (magneticStrength != NULL)
        {
            if (deviceCount > 1 && stringBufferLength(magStrengthReports) > 0)
            {
                stringBufferAppend(magStrengthReports, "|");
            }

            stringBufferAppend(magStrengthReports, magneticStrength);
        }

        /*
         * Zigbee Network Health Telemetry
         * This will likely be used to replace the above stuff, as
         * this format is vetted to work with telemetry, where as the
         * information above does not.
         * String format:  model,NE/FE RSSI, rejoins, duplicate sequence counter, aps ack failure counter, IAS Zone
         * status sensor delay value Each device is separated by a |
         */
        if (deviceCount > 1)
        {
            stringBufferAppend(zigbeeDeviceNetworkHealth, "|");
        }
        stringBufferAppend(zigbeeDeviceNetworkHealth, statList.model); // model

        stringBufferAppend(zigbeeDeviceNetworkHealth, ",");

        stringBufferAppend(zigbeeDeviceNetworkHealth, statList.nearRssi); // NE/FE RSSI
        stringBufferAppend(zigbeeDeviceNetworkHealth, "/");
        stringBufferAppend(zigbeeDeviceNetworkHealth, statList.farRssi);

        stringBufferAppend(zigbeeDeviceNetworkHealth, ",");

        scoped_generic char *rejoins = stringBuilder("%" PRIu32, deviceEventCounters->totalRejoinEvents);
        stringBufferAppend(zigbeeDeviceNetworkHealth, rejoins); // rejoin

        stringBufferAppend(zigbeeDeviceNetworkHealth, ",");

        scoped_generic char *dupSeq = stringBuilder("%" PRIu32, deviceEventCounters->totalDuplicateSeqNumEvents);
        stringBufferAppend(zigbeeDeviceNetworkHealth, dupSeq); // Duplicate Sequence Counter

        stringBufferAppend(zigbeeDeviceNetworkHealth, ",");

        scoped_generic char *ackFailure = stringBuilder("%" PRIu32, deviceEventCounters->totalApsAckFailureEvents);
        stringBufferAppend(zigbeeDeviceNetworkHealth, ackFailure); // Aps Ack failure

        stringBufferAppend(zigbeeDeviceNetworkHealth, ",");

        scoped_generic char *sensorDelayValue = stringBuilder("%" PRIu32, deviceEventCounters->cumulativeSensorDelayQS);
        stringBufferAppend(zigbeeDeviceNetworkHealth, sensorDelayValue); // Sensor Fault/Restore Message Delay Value

        zigbeeEventTrackerResetSensorDelayCountersForDevice(device->uuid);

        linkedListDestroy(attList, destroyDeviceAttributeItem);
        attList = NULL;

        linkedListDestroy(rejoinList, destroyDeviceRejoinItem);
        rejoinList = NULL;
    }

    scoped_generic char *magStrengthReportString = stringBufferToString(magStrengthReports);
    g_hash_table_insert(output, g_strdup(MAGNETIC_STRENGTH_KEY), g_steal_pointer(&magStrengthReportString));

    scoped_generic char *zigbeeDeviceNetworkHealthString = stringBufferToString(zigbeeDeviceNetworkHealth);
    g_hash_table_insert(
        output, g_strdup(ZIGBEE_DEVICE_NETWORK_HEALTH_KEY), g_steal_pointer(&zigbeeDeviceNetworkHealthString));

    scoped_icLinkedListNofree *deviceDrivers = deviceDriverManagerGetDeviceDrivers();
    scoped_icLinkedListIterator *driverIter = linkedListIteratorCreate(deviceDrivers);

    while (linkedListIteratorHasNext(driverIter))
    {
        const DeviceDriver *driver = linkedListIteratorGetNext(driverIter);

        if (driver->fetchRuntimeStats != NULL)
        {
            scoped_icStringHashMap *stats = stringHashMapCreate();
            driver->fetchRuntimeStats(driver->callbackContext, stats);

            scoped_icStringHashMapIterator *statsIter = stringHashMapIteratorCreate(stats);
            while (stringHashMapIteratorHasNext(statsIter))
            {
                /* Do not free - owned by collection */
                char *key;
                char *value;

                stringHashMapIteratorGetNext(statsIter, &key, &value);
                g_hash_table_insert(output, g_strdup(key), g_strdup(value));
            }
        }
    }
}

static deviceModelListItem *createDeviceModelListItem(const char *model, uint16_t count)
{
    deviceModelListItem *item = calloc(1, sizeof(deviceModelListItem));
    item->model = strdup(model);
    item->count = count;

    return item;
}

static void destroyDeviceModelListItem(void *item)
{
    deviceModelListItem *listItem = item;
    if (listItem != NULL)
    {
        free(listItem->model);
        free(listItem);
    }
}

static bool searchDeviceModelInList(void *searchVal, void *item)
{
    char *model = (char *) searchVal;
    deviceModelListItem *node = (deviceModelListItem *) item;

    if (stringCompare(model, node->model, false) == 0)
    {
        return true;
    }

    return false;
}

static void destroySubsystemMap(void *key, void *value)
{
    free(key);
    linkedListDestroy(value, destroyDeviceModelListItem);
}

static void destroyDeviceClassMap(void *key, void *value)
{
    free(key);
    hashMapDestroy(value, destroySubsystemMap);
}

static bool addDeviceClassMapEntry(icHashMap *deviceClassMap, const char *deviceClass)
{
    if (deviceClassMap == NULL || deviceClass == NULL)
    {
        return false;
    }

    bool success = false;

    icHashMap *subsystemMap = hashMapGet(deviceClassMap, (char *) deviceClass, strlen(deviceClass));

    if (subsystemMap)
    {
        success = true;
    }
    else
    {
        subsystemMap = hashMapCreate();
        char *key = strdup(deviceClass);
        success = hashMapPut(deviceClassMap, key, strlen(key), subsystemMap);

        if (success == false)
        {
            free(g_steal_pointer(&key));
            icLogWarn(LOG_TAG, "%s: unable to add subsystem entry for device class %s", __FUNCTION__, deviceClass);
            hashMapDestroy(subsystemMap, NULL);
        }
    }

    return success;
}

static bool addSubsystemMapEntry(icHashMap *subsystemMap, const char *subsystemName)
{
    if (subsystemMap == NULL || subsystemName == NULL)
    {
        return false;
    }

    bool success = false;
    icLinkedList *deviceModelsList = hashMapGet(subsystemMap, (char *) subsystemName, strlen(subsystemName));

    if (deviceModelsList != NULL)
    {
        success = true;
    }
    else
    {
        deviceModelsList = linkedListCreate();
        char *key = strdup(subsystemName);
        success = hashMapPut(subsystemMap, key, strlen(key), deviceModelsList);

        if (success == false)
        {
            free(g_steal_pointer(&key));
            icLogWarn(LOG_TAG, "%s: unable to add deviceModel list for subsystem %s", __FUNCTION__, subsystemName);
            linkedListDestroy(deviceModelsList, NULL);
        }
    }

    return success;
}

static bool addDeviceModelsListEntry(icLinkedList *deviceModelsList, const char *model)
{
    if (deviceModelsList == NULL || model == NULL)
    {
        return false;
    }

    bool success = false;
    deviceModelListItem *item = linkedListFind(deviceModelsList, (char *) model, searchDeviceModelInList);

    if (item != NULL)
    {
        item->count++;
        success = true;
    }
    else
    {
        item = createDeviceModelListItem(model, 1);
        success = linkedListAppend(deviceModelsList, item);

        if (success == false)
        {
            icLogWarn(LOG_TAG, "%s: unable to add model %s to the list", __FUNCTION__, model);
            destroyDeviceModelListItem(item);
        }
    }

    return success;
}

/*
 * Create a hash map with device class(e.g. sensor, etc) as key and value is another hash map with subsystem (e.g.
 * zigbee, etc) as key and linked list as value containing all models(e.g. XHS2-SE, etc) and their corresponding count.
 */
static icHashMap *getOrganizedPairedDeviceInfo(void)
{
    icHashMap *deviceClassMap = hashMapCreate();

    scoped_icLinkedListNofree *deviceDrivers = deviceDriverManagerGetDeviceDrivers();
    scoped_icLinkedListIterator *driverIter = linkedListIteratorCreate(deviceDrivers);

    while (linkedListIteratorHasNext(driverIter))
    {
        const DeviceDriver *driver = linkedListIteratorGetNext(driverIter);

        // ignore camera devices as we have separate function for it.
        //
        if (stringCompare(driver->driverName, "openHomeCameraDeviceDriver", false) == 0)
        {
            continue;
        }

        scoped_icDeviceList *devices = deviceServiceGetDevicesByDeviceDriver(driver->driverName);
        scoped_icLinkedListIterator *devicesIter = linkedListIteratorCreate(devices);

        while (linkedListIteratorHasNext(devicesIter))
        {
            const icDevice *device = linkedListIteratorGetNext(devicesIter);

            const icDeviceResource *modelResource =
                deviceServiceFindDeviceResourceById((icDevice *) device, COMMON_DEVICE_RESOURCE_MODEL);
            if (modelResource == NULL)
            {
                icLogWarn(LOG_TAG,
                          "%s: unable to get model resource for a device of deviceClass %s",
                          __FUNCTION__,
                          device->deviceClass);
                continue;
            }

            if (addDeviceClassMapEntry(deviceClassMap, device->deviceClass) == false)
            {
                continue;
            }

            icHashMap *subsystemMap = hashMapGet(deviceClassMap, device->deviceClass, strlen(device->deviceClass));

            if (addSubsystemMapEntry(subsystemMap, driver->subsystemName) == false)
            {
                continue;
            }

            icLinkedList *deviceModelsList =
                hashMapGet(subsystemMap, driver->subsystemName, strlen(driver->subsystemName));
            addDeviceModelsListEntry(deviceModelsList, modelResource->value);
        }
    }

    return deviceClassMap;
}

static inline char *getFormattedDeviceClassString(const char *deviceClass)
{
    char *formattedDeviceClass = strdup(deviceClass);
    formattedDeviceClass[0] = (char) toupper(formattedDeviceClass[0]);

    return formattedDeviceClass;
}

static inline char *getFormattedSubsystemString(const char *subsystem)
{
    char *formattedSubsystem = strdup(subsystem);
    formattedSubsystem[0] = (char) toupper(formattedSubsystem[0]);

    return formattedSubsystem;
}

static char *getDeviceCountKey(const char *deviceClass)
{
    if (deviceClass == NULL)
    {
        return NULL;
    }

    scoped_generic char *formattedDeviceClass = getFormattedDeviceClassString(deviceClass);

    return stringBuilder("Total%sCount", formattedDeviceClass);
}

static char *getDeviceTypeKey(const char *deviceClass, const char *subsystem)
{
    if (deviceClass == NULL || subsystem == NULL)
    {
        return NULL;
    }

    scoped_generic char *formattedDeviceClass = getFormattedDeviceClassString(deviceClass);
    scoped_generic char *formattedSubsystem = getFormattedSubsystemString(subsystem);

    return stringBuilder("Total%s%s", formattedSubsystem, formattedDeviceClass);
}

static uint16_t getDevicesCount(void)
{
    icLinkedList *allDevicesList = deviceServiceGetAllDevices();
    uint16_t count = linkedListCount(allDevicesList);
    linkedListDestroy(allDevicesList, (linkedListItemFreeFunc) deviceDestroy);

    return count;
}

static uint16_t getCameraCount(void)
{
    icLinkedList *allCameraList = deviceServiceGetDevicesByDeviceClass(CAMERA_DC);
    uint16_t count = linkedListCount(allCameraList);
    linkedListDestroy(allCameraList, (linkedListItemFreeFunc) deviceDestroy);

    return count;
}

/**
 * Add device split markers by subsystem, model and count
 *
 * @param output - the runtimeStatistics hashMap containing connected device split markers by subsytem, model and count
 * e.g.
 * {"TotalSensorCount_split" "subsystem,14"; }
 * {"TotalSubsytemSensor_split" "XHS2-SE,4;XCAM1,3;XCAM2,3;SMCWK01-Z,4;"}
 *
 */
void collectPairedDevicesInformation(GHashTable *output)
{
    g_return_if_fail(output != NULL);

    static icHashMap *deviceClassMap = NULL;
    static uint16_t devicesCount = 0;

    uint16_t count = getDevicesCount();
    // if number of devices are still the same in this iteration, assume nothing has changed and use cached information
    // in deviceClassMap.
    //
    if (count != devicesCount)
    {
        // There is a change in number of devices, flush out cached information.
        // Cleanup iteratively everything in deviceClassMap i.e. including hash map of subsystem for each class and
        // lists of device models for each subsystem
        //
        hashMapDestroy(deviceClassMap, destroyDeviceClassMap);
        deviceClassMap = getOrganizedPairedDeviceInfo();
        devicesCount = count;
    }

    scoped_icHashMapIterator *deviceClassMapIter = hashMapIteratorCreate(deviceClassMap);

    while (hashMapIteratorHasNext(deviceClassMapIter))
    {
        char *deviceClass = NULL;
        uint16_t deviceClassLen = 0;
        icHashMap *subsystemMap = NULL;

        hashMapIteratorGetNext(deviceClassMapIter, (void **) &deviceClass, &deviceClassLen, (void **) &subsystemMap);

        scoped_generic char *deviceCountKey = getDeviceCountKey(deviceClass);
        GString *deviceCountValue = g_string_new(NULL);

        scoped_icHashMapIterator *subsystemMapIter = hashMapIteratorCreate(subsystemMap);

        while (hashMapIteratorHasNext(subsystemMapIter))
        {
            char *subsystem = NULL;
            uint16_t subsystemLen = 0;
            icLinkedList *modelList = NULL;

            hashMapIteratorGetNext(subsystemMapIter, (void **) &subsystem, &subsystemLen, (void **) &modelList);

            scoped_generic char *deviceTypeKey = getDeviceTypeKey(deviceClass, subsystem);
            GString *deviceTypeValue = g_string_new(NULL);

            uint16_t deviceCount = 0;
            scoped_icLinkedListIterator *modelListIter = linkedListIteratorCreate(modelList);

            while (linkedListIteratorHasNext(modelListIter))
            {
                const deviceModelListItem *item = linkedListIteratorGetNext(modelListIter);
                g_string_append_printf(deviceTypeValue, "%s,%d;", item->model, item->count);
                deviceCount += item->count;
            }

            scoped_generic char *value = g_string_free(g_steal_pointer(&deviceTypeValue), FALSE);
            g_hash_table_insert(output, g_steal_pointer(&deviceTypeKey), g_steal_pointer(&value));

            g_string_append_printf(deviceCountValue, "%s,%d;", subsystem, deviceCount);
        }

        scoped_generic char *value = g_string_free(g_steal_pointer(&deviceCountValue), FALSE);
        g_hash_table_insert(output, g_steal_pointer(&deviceCountKey), g_steal_pointer(&value));
    }
}

/*
 * Create a linked list containing all camera models(e.g. iCamera, etc) and their corresponding count.
 * NOTE: caller must free return LinkedList
 */
static icLinkedList *getPairedCamerasInfo(void)
{
    icLinkedList *cameraModelsList = linkedListCreate();

    scoped_icDeviceList *devices = deviceServiceGetDevicesByDeviceClass(CAMERA_DC);
    scoped_icLinkedListIterator *devicesIter = linkedListIteratorCreate(devices);

    while (linkedListIteratorHasNext(devicesIter))
    {
        const icDevice *device = linkedListIteratorGetNext(devicesIter);

        icDeviceResource *modelResource =
            deviceServiceFindDeviceResourceById((icDevice *) device, COMMON_DEVICE_RESOURCE_MODEL);
        if (modelResource == NULL)
        {
            icLogWarn(LOG_TAG,
                      "%s: unable to get model resource for a device %s of deviceClass %s",
                      __FUNCTION__,
                      device->uuid,
                      CAMERA_DC);
            continue;
        }

        const char *model = stringCoalesceAlt(modelResource->value, NULL);
        addDeviceModelsListEntry(cameraModelsList, model);
    }
    return cameraModelsList;
}

/**
 * Add camera split markers by model and count
 *
 * @param output - the runtimeStatistics hashMap containing connected camera device split markers by model and count
 * e.g.
 * {"TotalLocalCameraCount": "2"}
 * {"TotalLocalCamera": "XCam1,1;ICam2C,1;"}
 *
 */
void collectPairedCamerasInformation(GHashTable *output)
{
    g_return_if_fail(output != NULL);

    static icLinkedList *cameraModelsList = NULL;
    static uint16_t cameraCount = 0;

    uint16_t count = getCameraCount();
    // if number of camera are still the same in this iteration, assume nothing has changed and use cached information
    // in cameraModelsList.
    //
    if (count != cameraCount)
    {
        // There is a change in number of camera, flush out cached information.
        // Cleanup iteratively everything in cameraModelList
        //
        linkedListDestroy(cameraModelsList, destroyDeviceModelListItem);
        cameraModelsList = getPairedCamerasInfo();
        cameraCount = count;
    }

    scoped_generic char *valueString = stringBuilder("%" PRIu16, cameraCount);
    g_hash_table_insert(output, g_strdup("TotalLocalCameraCount"), g_steal_pointer(&valueString));

    GString *cameraTypeValue = g_string_new(NULL);
    scoped_icLinkedListIterator *cameraModelListIter = linkedListIteratorCreate(cameraModelsList);
    while (linkedListIteratorHasNext(cameraModelListIter))
    {
        const deviceModelListItem *cameraModelListItem = linkedListIteratorGetNext(cameraModelListIter);
        g_string_append_printf(cameraTypeValue, "%s,%d;", cameraModelListItem->model, cameraModelListItem->count);
    }

    scoped_generic char *value = g_string_free(g_steal_pointer(&cameraTypeValue), FALSE);
    g_hash_table_insert(output, g_strdup("TotalLocalCamera"), g_steal_pointer(&value));
}

/**
 * Collect all of the Zigbee counters from Zigbee core
 *
 * @param output - the runtimeStatistics hashMap
 */
void collectZigbeeNetworkCounters(GHashTable *strings, GHashTable *ints)
{
    g_return_if_fail(strings != NULL);
    g_return_if_fail(ints != NULL);

    scoped_cJSON *counters = zigbeeSubsystemGetAndClearCounters();

    if (counters != NULL)
    {
        cJSON *counter = NULL;

        cJSON_ArrayForEach(counter, counters)
        {
            if (cJSON_IsNumber(counter) && counter->valueint != 0)
            {
                int32_t *value = malloc(sizeof(int32_t));
                *value = counter->valueint;
                g_hash_table_insert(ints, g_strdup(counter->string), g_steal_pointer(&value));
            }
        }

        /*
         * Telemetry Network Health
         * Taking the counters and putting them in an order specific comma separated list
         * to send up in a single marker for telemetry
         */

        scoped_icStringBuffer *zigbeeCountersBuffer = stringBufferCreate(0);
        int index = 0;

        while (index < ARRAY_LENGTH(zigbeeNetworkCountersKeys))
        {
            if (index > 0)
            {
                stringBufferAppend(zigbeeCountersBuffer, ",");
            }

            cJSON *item = cJSON_GetObjectItem(counters, zigbeeNetworkCountersKeys[index]);
            int64_t value = 0;

            if (item != NULL)
            {
                value = (int64_t) cJSON_GetNumberValue(item);
            }

            scoped_generic char *valueString = stringBuilder("%" PRIi64, value);
            stringBufferAppend(zigbeeCountersBuffer, valueString);

            index++;
        }

        scoped_generic char *zigbeeCounterString = stringBufferToString(zigbeeCountersBuffer);
        g_hash_table_insert(strings, g_strdup(ZIGBEE_NETWORK_COUNTERS_KEY), g_steal_pointer(&zigbeeCounterString));
    }
}

/**
 * Collect all of the Zigbee Core Network status
 * includes panID, channel, openForJoin, network up,
 * and eui64
 *
 * @param output - the runtimeStatistics hashMap
 */
void collectZigbeeCoreNetworkStatistics(GHashTable *strings, GHashTable *longs)
{
    g_return_if_fail(strings != NULL);
    g_return_if_fail(longs != NULL);

    zhalSystemStatus zigbeeNetworkStatus;
    memset(&zigbeeNetworkStatus, 0, sizeof(zhalSystemStatus));

    int rc = zigbeeSubsystemGetSystemStatus(&zigbeeNetworkStatus);

    if (rc == 0)
    {
        g_hash_table_insert(strings, g_strdup(IS_ZIGBEE_NET_AVAILABLE_KEY), g_strdup("true"));
        g_hash_table_insert(strings, g_strdup(IS_ZIGBEE_NET_CONFIGURED_KEY), g_strdup("true"));

        g_hash_table_insert(
            strings, g_strdup(IS_ZIGBEE_NETWORK_UP_KEY), g_strdup(zigbeeNetworkStatus.networkIsUp ? "true" : "false"));
        g_hash_table_insert(strings,
                            g_strdup(OPEN_FOR_JOIN_KEY),
                            g_strdup(zigbeeNetworkStatus.networkIsOpenForJoin ? "true" : "false"));

        scoped_generic char *id = zigbeeSubsystemEui64ToId(zigbeeNetworkStatus.eui64);

        if (id != NULL)
        {
            g_hash_table_insert(strings, g_strdup(EUI_64_KEY), g_steal_pointer(&id));
        }
        else
        {
            g_hash_table_insert(strings, g_strdup(EUI_64_KEY), g_strdup("N/A"));
        }

        int64_t *channel = malloc(sizeof(int64_t));
        *channel = zigbeeNetworkStatus.channel;
        int64_t *panId = malloc(sizeof(int64_t));
        *panId = zigbeeNetworkStatus.panId;
        g_hash_table_insert(longs, g_strdup(CHANNEL_KEY), g_steal_pointer(&channel));
        g_hash_table_insert(longs, g_strdup(PAN_ID_KEY), g_steal_pointer(&panId));
    }
    else
    {
        g_hash_table_insert(strings, g_strdup(IS_ZIGBEE_NET_AVAILABLE_KEY), g_strdup("false"));
    }
}

/**
 * Collect all of the Zigbee device firmware failures/success
 *
 * @param output - the runtimeStatistics hashMap
 */
void collectAllDeviceFirmwareEvents(GHashTable *ints, GHashTable *longs)
{
    g_return_if_fail(ints != NULL);
    g_return_if_fail(longs != NULL);

    icLinkedList *failureEvents = zigbeeEventTrackerCollectFirmwareUpgradeFailureEvents();

    if (failureEvents != NULL)
    {
        scoped_icLinkedListIterator *listIter = linkedListIteratorCreate(failureEvents);

        while (linkedListIteratorHasNext(listIter))
        {
            const deviceUpgFailureItem *item = linkedListIteratorGetNext(listIter);

            if (item->deviceId != NULL)
            {
                scoped_generic char *key = stringBuilder("%s%s", DEVICE_FW_UPGRADE_FAILURE_KEY, item->deviceId);

                if (key != NULL)
                {
                    int64_t *failureTime = malloc(sizeof(int64_t));
                    *failureTime = item->failureTime;
                    g_hash_table_insert(longs, g_steal_pointer(&key), g_steal_pointer(&failureTime));
                }
            }
        }

        int upgradeFailureCount = linkedListCount(failureEvents);

        if (upgradeFailureCount != 0)
        {
            int32_t *upgradeFailCount = malloc(sizeof(int32_t));
            *upgradeFailCount = upgradeFailureCount;
            g_hash_table_insert(ints, g_strdup(DEVICE_FW_UPGRADE_FAIL_CNT_KEY), g_steal_pointer(&upgradeFailCount));
        }

        linkedListDestroy(failureEvents, destroyDeviceUpgFailureItem);
    }

    int upgradeSuccess = zigbeeEventTrackerCollectFirmwareUpgradeSuccessEvents();

    if (upgradeSuccess != 0)
    {
        int32_t *upgradeSucceeded = malloc(sizeof(int32_t));
        *upgradeSucceeded = upgradeSuccess;
        g_hash_table_insert(ints, g_strdup(DEVICE_FW_UPGRADE_SUCCESS_CNT_KEY), g_steal_pointer(&upgradeSucceeded));
    }
}

/**
 * Collect zigbee channel status and add them into
 * the runtime stats hash map
 *
 * @param output
 */
void collectChannelScanStats(GHashTable *ints)
{
    g_return_if_fail(ints != NULL);

    icLinkedList *channelStats = zigbeeEventTrackerCollectChannelEnergyScanStats();

    if (channelStats != NULL)
    {
        scoped_icLinkedListIterator *listIterator = linkedListIteratorCreate(channelStats);

        while (linkedListIteratorHasNext(listIterator))
        {
            const channelEnergyScanDataItem *item = linkedListIteratorGetNext(listIterator);

            if (item != NULL)
            {
                scoped_generic char *maxKey = stringBuilder("%s%" PRIu8, CHANNEL_SCAN_MAX_KEY, item->channel);
                scoped_generic char *minKey = stringBuilder("%s%" PRIu8, CHANNEL_SCAN_MIN_KEY, item->channel);
                scoped_generic char *avgKey = stringBuilder("%s%" PRIu8, CHANNEL_SCAN_AVG_KEY, item->channel);

                int32_t *max = malloc(sizeof(int32_t));
                *max = item->max;
                int32_t *min = malloc(sizeof(int32_t));
                *min = item->min;
                int32_t *avg = malloc(sizeof(int32_t));
                *avg = item->average;
                g_hash_table_insert(ints, g_steal_pointer(&maxKey), g_steal_pointer(&max));
                g_hash_table_insert(ints, g_steal_pointer(&minKey), g_steal_pointer(&min));
                g_hash_table_insert(ints, g_steal_pointer(&avgKey), g_steal_pointer(&avg));
            }
        }

        linkedListDestroy(channelStats, NULL);
    }
}

/*
 * gather zigbee devices network map
 */
static void gatherZigbeeNetworkMap(void)
{
    icLogTrace(LOG_TAG, "%s", __FUNCTION__);

    scoped_icDeviceList *deviceList = deviceServiceGetDevicesBySubsystem(ZIGBEE_SUBSYSTEM_NAME);
    uint16_t numOfDevices = linkedListCount(deviceList);

    if (numOfDevices > 0)
    {
        scoped_icLinkedListGeneric *zigbeeNetworkMap = zigbeeSubsystemGetNetworkMap();

        if (zigbeeNetworkMap != NULL)
        {
            scoped_icLinkedListIterator *iter = linkedListIteratorCreate(zigbeeNetworkMap);
            scoped_icStringBuffer *zigbeeNetworkMapStrBuf =
                stringBufferCreate(MAX_ZIGBEE_NETWORK_MAP_LENGTH * numOfDevices);

            while (linkedListIteratorHasNext(iter) == true)
            {
                const ZigbeeSubsystemNetworkMapEntry *item = linkedListIteratorGetNext(iter);

                scoped_generic char *eui64Str = zigbeeSubsystemEui64ToId(item->address);
                scoped_generic char *nextCloseHopEui64Str = zigbeeSubsystemEui64ToId(item->nextCloserHop);
                stringBufferAppendWithComma(zigbeeNetworkMapStrBuf, eui64Str, false);
                stringBufferAppendWithComma(zigbeeNetworkMapStrBuf, nextCloseHopEui64Str, false);

                scoped_generic char *lqiStr = stringBuilder("%" PRId32, item->lqi);
                stringBufferAppend(zigbeeNetworkMapStrBuf, lqiStr);
                stringBufferAppend(zigbeeNetworkMapStrBuf, ";");
            }

            pthread_mutex_lock(&ZIGBEE_NETWORK_MAP_MTX);
            free(zigbeeNetworkMapStr);
            zigbeeNetworkMapStr = stringBufferToString(zigbeeNetworkMapStrBuf);
            pthread_mutex_unlock(&ZIGBEE_NETWORK_MAP_MTX);
        }
    }
}

/*
 * callback function for gathering zigbee network map
 * task will be run at random hour within the next 24 hours
 */
static void gatherZigbeeNetworkMapCallback(void *arg)
{
    icLogDebug(LOG_TAG, "%s: gathering zigbee network map", __FUNCTION__);
    pthread_mutex_lock(&ZIGBEE_NETWORK_MAP_MTX);
    zigbeeMapGathererTask = 0;
    pthread_mutex_unlock(&ZIGBEE_NETWORK_MAP_MTX);
    gatherZigbeeNetworkMap();
}

/**
 * Collect zigbee network map and add them into
 * the runtime stats hash map
 * The format is EUI64,Next Hop EUI64,LQI,
 * for every device
 *
 * @param output
 */
void collectZigbeeNetworkMap(GHashTable *output)
{
    g_return_if_fail(output != NULL);

    pthread_mutex_lock(&ZIGBEE_NETWORK_MAP_MTX);

    if (zigbeeMapGathererTask == 0 || isDelayTaskWaiting(zigbeeMapGathererTask) == false)
    {
        uint64_t randomHour;
        bool isRandomHour = generateRandomNumberInRange(
            MIN_ZIGBEE_NETWORK_MAP_COLLECT_HOUR, MAX_ZIGBEE_NETWORK_MAP_COLLECT_HOUR, &randomHour);
        if (isRandomHour == true)
        {
            icLogDebug(LOG_TAG,
                       "%s: scheduling task to gather zigbee network map after %" PRIu8 " hour(s)",
                       __FUNCTION__,
                       (uint8_t) randomHour);
            zigbeeMapGathererTask =
                scheduleDelayTask((uint8_t) randomHour, DELAY_HOURS, gatherZigbeeNetworkMapCallback, NULL);

            if (zigbeeNetworkMapStr != NULL && stringIsEmpty(zigbeeNetworkMapStr) == false)
            {
                g_hash_table_insert(output, g_strdup(ZIGBEE_NETWORK_MAP_KEY), g_steal_pointer(&zigbeeNetworkMapStr));
            }
        }
        else
        {
            icLogError(LOG_TAG,
                       "%s Can't generate random number, not scheduling task to gather zigbee network map",
                       __FUNCTION__);
        }
    }

    pthread_mutex_unlock(&ZIGBEE_NETWORK_MAP_MTX);
}

/**
 * Collects stats about Cameras and add them into
 * the runtime stats hash map
 *
 * @param output - the hash map container
 */
void collectCameraDeviceStats(GHashTable *output)
{
    g_return_if_fail(output != NULL);

    scoped_icDeviceList *cameraList = deviceServiceGetDevicesByDeviceClass(CAMERA_DC);

    if (linkedListCount(cameraList) > 0)
    {
        scoped_icStringBuffer *allCameras = stringBufferCreate(INITIAL_CAMERA_BUFFER_SIZE);
        scoped_icStringBuffer *connectedCameras = stringBufferCreate(INITIAL_CAMERA_BUFFER_SIZE);
        scoped_icStringBuffer *disconnectedCameras = stringBufferCreate(INITIAL_CAMERA_BUFFER_SIZE);

        scoped_icLinkedListIterator *listIterator = linkedListIteratorCreate(cameraList);

        while (linkedListIteratorHasNext(listIterator) == true)
        {
            const icDevice *camera = linkedListIteratorGetNext(listIterator);

            if (camera == NULL || camera->uuid == NULL)
            {
                icLogError(LOG_TAG, "%s: got an unknown camera", __FUNCTION__);
                continue;
            }

            cameraDeviceInfo cameraInfo;
            initCameraDeviceInfo(&cameraInfo);
            collectCameraResources(&cameraInfo, camera);

            if (stringIsEmpty(cameraInfo.macAddress) == false)
            {
                if (stringIsEmpty(cameraInfo.commFail) == false)
                {
                    if (stringToBool(cameraInfo.commFail) == true)
                    {
                        stringBufferAppendWithComma(disconnectedCameras, cameraInfo.macAddress, true);
                    }
                    else
                    {
                        stringBufferAppendWithComma(connectedCameras, cameraInfo.macAddress, true);
                    }
                }
                else
                {
                    icLogError(
                        LOG_TAG, "%s: unable to determine Comm Failure for camera %s", __FUNCTION__, camera->uuid);
                }

                stringBufferAppendWithComma(allCameras, cameraInfo.macAddress, true);
            }
            else
            {
                icLogError(LOG_TAG, "%s: unable to locate MAC Address for camera %s", __FUNCTION__, camera->uuid);
            }

            // TODO: anything else needed?
        }

        scoped_generic char *allCamerasValue = stringBufferToString(allCameras);

        if (stringIsEmpty(allCamerasValue) == false)
        {
            g_hash_table_insert(output, g_strdup(CAMERA_TOTAL_DEVICE_LIST_KEY), g_steal_pointer(&allCamerasValue));
            allCamerasValue = NULL;
        }

        scoped_generic char *connectedCamerasValue = stringBufferToString(connectedCameras);

        if (stringIsEmpty(connectedCamerasValue) == false)
        {
            g_hash_table_insert(output, g_strdup(CAMERA_CONNECTED_LIST_KEY), g_steal_pointer(&connectedCamerasValue));
            connectedCamerasValue = NULL;
        }

        scoped_generic char *disconnectedCamerasValue = stringBufferToString(disconnectedCameras);

        if (stringIsEmpty(disconnectedCamerasValue) == false)
        {
            g_hash_table_insert(
                output, g_strdup(CAMERA_DISCONNECTED_LIST_KEY), g_steal_pointer(&disconnectedCamerasValue));
            disconnectedCamerasValue = NULL;
        }
    }
}

/**
 *  Helper function for handling the logic or adding
 *  a value to output if it has not been created yet.
 *  And determine if input is NULL or the special char
 *  NULL.
 *
 * @param input - the value to be copied
 * @return - either the input value or a custom NULL value
 */
static const char *customStringToString(const char *src)
{
    const char *dest;

    if ((src == NULL) || strcmp(src, DEVICE_NULL_VALUE) == 0)
    {
        dest = VALUE_IS_EMPTY_STRING;
    }
    else
    {
        dest = src;
    }

    return dest;
}

/**
 * Helper function to init basicDeviceInfo struct
 * by making all of the char points set to our
 * custom NULL char which is ""
 *
 * @param statList - the list of stats
 */
static void initDeviceInfo(basicDeviceInfo *deviceInfo)
{
    memset(deviceInfo, 0, sizeof(basicDeviceInfo));

    deviceInfo->manufacturer = customStringToString(NULL);
    deviceInfo->model = customStringToString(NULL);
    deviceInfo->firmVer = customStringToString(NULL);
    deviceInfo->hardVer = customStringToString(NULL);
    deviceInfo->farLqi = customStringToString(NULL);
    deviceInfo->nearLqi = customStringToString(NULL);
    deviceInfo->nearRssi = customStringToString(NULL);
    deviceInfo->farRssi = customStringToString(NULL);
    deviceInfo->temp = customStringToString(NULL);
    deviceInfo->batteryVolts = customStringToString(NULL);
    deviceInfo->lowBattery = customStringToString(NULL);
    deviceInfo->commFail = customStringToString(NULL);
    deviceInfo->troubled = customStringToString(NULL);
    deviceInfo->bypassed = customStringToString(NULL);
    deviceInfo->tampered = customStringToString(NULL);
    deviceInfo->faulted = customStringToString(NULL);
    deviceInfo->upgradeStatus = customStringToString(NULL);
    deviceInfo->batteryState = customStringToString(NULL);
}

/**
 * Helper function to init cameraDeviceInfo struct
 * by making all of the char points set to our
 * custom NULL char which is ""
 *
 * @param deviceInfo - the list of stats
 */
static void initCameraDeviceInfo(cameraDeviceInfo *deviceInfo)
{
    deviceInfo->macAddress = customStringToString(NULL);
    deviceInfo->commFail = customStringToString(NULL);
}

/**
 * Helper function to init attributeDeviceInfo struct
 * by making all of the char points set to our
 * custom NULL char which is ""
 *
 * @param attDeviceInfo - the list of attribute values
 * @param size - the number of attribute reports
 */
static void initAttDeviceInfo(attributeDeviceInfo *attDeviceInfo, int size)
{
    memset(attDeviceInfo, 0, size * sizeof(attributeDeviceInfo));

    for (int i = 0; i < size; i++)
    {
        attDeviceInfo[i].reportTime = customStringToString(NULL);
        attDeviceInfo[i].clusterId = customStringToString(NULL);
        attDeviceInfo[i].attributeId = customStringToString(NULL);
        attDeviceInfo[i].data = customStringToString(NULL);
    }
}

/**
 * Helper function to init rejoinDeviceInfo struct
 * by making all of the char points set to our
 * custom NULL char which is ""
 *
 * @param rejoinInfo - the list of rejoin values
 * @param size - the number of rejoins
 */
static void initRejoinDeviceInfo(rejoinDeviceInfo *rejoinInfo, int size)
{
    memset(rejoinInfo, 0, size * sizeof(rejoinDeviceInfo));

    for (int i = 0; i < size; i++)
    {
        rejoinInfo[i].time = customStringToString(NULL);
        rejoinInfo[i].isSecure = customStringToString(NULL);
    }
}

/**
 * Helper function to create checkInInfo struct
 * by making all elements in list point to our
 * the custom NULL char which is ""
 *
 * @param checkInInfo - the list of check-in values
 * @param size - the number of check ins
 */
static void initCheckInDeviceInfo(checkInDeviceInfo *checkInInfo, int size)
{
    memset(checkInInfo, 0, size * sizeof(checkInDeviceInfo));

    for (int i = 0; i < size; i++)
    {
        checkInInfo[i].time = customStringToString(NULL);
    }
}

/**
 * Helper function to sort through the resource list
 * and find the ones we want to gather
 *
 * @param statList - out stats collected list
 * @param device - the physical device
 */
static void collectResources(basicDeviceInfo *deviceInfo, const icDevice *device)
{
    if (device->resources == NULL)
    {
        icLogError(LOG_TAG, "%s: unable to use resource list for device %s", __FUNCTION__, device->uuid);
        return;
    }

    scoped_icLinkedListIterator *resourceIter = linkedListIteratorCreate(device->resources);

    while (linkedListIteratorHasNext(resourceIter))
    {
        const icDeviceResource *resource = linkedListIteratorGetNext(resourceIter);

        if (resource->id == NULL)
        {
            icLogError(LOG_TAG, "%s: unable to use resource %s", __FUNCTION__, resource->uri);
            continue;
        }

        if (strcmp(resource->id, COMMON_DEVICE_RESOURCE_MANUFACTURER) == 0)
        {
            deviceInfo->manufacturer = customStringToString(resource->value);
        }
        else if (strcmp(resource->id, COMMON_DEVICE_RESOURCE_MODEL) == 0)
        {
            deviceInfo->model = customStringToString(resource->value);
        }
        else if (strcmp(resource->id, COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION) == 0)
        {
            deviceInfo->firmVer = customStringToString(resource->value);
        }
        else if (strcmp(resource->id, COMMON_DEVICE_RESOURCE_HARDWARE_VERSION) == 0)
        {
            deviceInfo->hardVer = customStringToString(resource->value);
        }
        else if (strcmp(resource->id, COMMON_DEVICE_RESOURCE_NERSSI) == 0)
        {
            deviceInfo->nearRssi = customStringToString(resource->value);
        }
        else if (strcmp(resource->id, COMMON_DEVICE_RESOURCE_FERSSI) == 0)
        {
            deviceInfo->farRssi = customStringToString(resource->value);
        }
        else if (strcmp(resource->id, COMMON_DEVICE_RESOURCE_NELQI) == 0)
        {
            deviceInfo->nearLqi = customStringToString(resource->value);
        }
        else if (strcmp(resource->id, COMMON_DEVICE_RESOURCE_FELQI) == 0)
        {
            deviceInfo->farLqi = customStringToString(resource->value);
        }
        else if (strcmp(resource->id, COMMON_DEVICE_RESOURCE_TEMPERATURE) == 0)
        {
            deviceInfo->temp = customStringToString(resource->value);
        }
        else if (strcmp(resource->id, COMMON_DEVICE_RESOURCE_BATTERY_VOLTAGE) == 0)
        {
            deviceInfo->batteryVolts = customStringToString(resource->value);
        }
        else if (strcmp(resource->id, COMMON_DEVICE_RESOURCE_BATTERY_LOW) == 0)
        {
            deviceInfo->lowBattery = customStringToString(resource->value);
        }
        else if (strcmp(resource->id, COMMON_DEVICE_RESOURCE_COMM_FAIL) == 0)
        {
            deviceInfo->commFail = customStringToString(resource->value);
        }
        else if (strcmp(resource->id, COMMON_DEVICE_RESOURCE_FIRMWARE_UPDATE_STATUS) == 0)
        {
            deviceInfo->upgradeStatus = customStringToString(resource->value);
        }
    }

    const icDeviceResource *badBatteryResource =
        deviceServiceFindDeviceResourceById((icDevice *) device, COMMON_DEVICE_RESOURCE_BATTERY_BAD);

    if (badBatteryResource != NULL && stringCompare(badBatteryResource->value, "true", false) == 0)
    {
        deviceInfo->batteryState = BATTERY_STATE_BAD_STRING;
    }
    else if (stringCompare(deviceInfo->lowBattery, "true", false) == 0)
    {
        deviceInfo->batteryState = BATTERY_STATE_LOW_STRING;
    }
    else if (stringCompare(deviceInfo->lowBattery, "false", false) == 0)
    {
        deviceInfo->batteryState = BATTERY_STATE_GOOD_STRING;
    }
}

/**
 * Helper function to find and gather the resources in the endpoints
 *
 * @param statList - our stats collected list
 * @param device - the physical device
 */
static void collectEndpointResources(basicDeviceInfo *deviceInfo, const icDevice *device)
{
    scoped_generic icLinkedListIterator *endpointIter = linkedListIteratorCreate(device->endpoints);

    while (linkedListIteratorHasNext(endpointIter))
    {
        const icDeviceEndpoint *endpoint = linkedListIteratorGetNext(endpointIter);

        if (endpoint == NULL)
        {
            icLogError(LOG_TAG, "%s: unable to find endpoint for device %s", __FUNCTION__, device->uuid);
            continue;
        }

        scoped_generic icLinkedListIterator *endpointResourceIter = linkedListIteratorCreate(endpoint->resources);

        while (linkedListIteratorHasNext(endpointResourceIter))
        {
            const icDeviceResource *resource = linkedListIteratorGetNext(endpointResourceIter);

            if (resource == NULL)
            {
                icLogError(LOG_TAG,
                           "%s: unable to find resource for device %s on endpoint %s",
                           __FUNCTION__,
                           device->uuid,
                           endpoint->uri);
                continue;
            }

            if (strcmp(resource->id, COMMON_ENDPOINT_RESOURCE_TROUBLE) == 0)
            {
                deviceInfo->troubled = customStringToString(resource->value);
            }
            else if (strcmp(resource->id, SENSOR_PROFILE_RESOURCE_BYPASSED) == 0)
            {
                deviceInfo->bypassed = customStringToString(resource->value);
            }
            else if (strcmp(resource->id, COMMON_ENDPOINT_RESOURCE_TAMPERED) == 0)
            {
                deviceInfo->tampered = customStringToString(resource->value);
            }
            else if (strcmp(resource->id, SENSOR_PROFILE_RESOURCE_FAULTED) == 0)
            {
                deviceInfo->faulted = customStringToString(resource->value);
            }
        }
    }
}

/**
 * Helper function to sort through the resource list
 * for Cameras and find the ones we want to gather
 *
 * @param deviceInfo - our stats to collect
 * @param device - the physical device
 */
static void collectCameraResources(cameraDeviceInfo *deviceInfo, const icDevice *device)
{
    if (device->resources == NULL)
    {
        icLogError(LOG_TAG, "%s: unable to use resource list for device %s", __FUNCTION__, device->uuid);
        return;
    }

    scoped_icLinkedListIterator *resourceIter = linkedListIteratorCreate(device->resources);

    while (linkedListIteratorHasNext(resourceIter))
    {
        const icDeviceResource *resource = linkedListIteratorGetNext(resourceIter);

        if (resource->id == NULL)
        {
            icLogError(LOG_TAG, "%s: unable to use resource %s", __FUNCTION__, resource->uri);
            continue;
        }

        if (strcmp(resource->id, COMMON_DEVICE_RESOURCE_MAC_ADDRESS) == 0)
        {
            deviceInfo->macAddress = customStringToString(resource->value);
        }
        else if (strcmp(resource->id, COMMON_DEVICE_RESOURCE_COMM_FAIL) == 0)
        {
            deviceInfo->commFail = customStringToString(resource->value);
        }

        // TODO: collect more resources??
    }
}

/**
 * Helper function to collect the values
 * found in the attribute report list if
 * its not NULL and stores the values in
 * device Info
 *
 * @param attList - the list to go through
 * @param deviceInfo - the list of attributeDeviceInfo to hold the values
 */
static void convertAttributeReports(icLinkedList *attList, attributeDeviceInfo *deviceInfo)
{
    if (attList != NULL && deviceInfo != NULL)
    {
        int counter = 0;
        scoped_icLinkedListIterator *listIter = linkedListIteratorCreate(attList);

        while (linkedListIteratorHasNext(listIter))
        {
            const deviceAttributeItem *currentItem = linkedListIteratorGetNext(listIter);

            if (currentItem == NULL)
            {
                icLogWarn(LOG_TAG, "%s: unable to find attribute item in attribute list", __FUNCTION__);
                continue;
            }

            if (counter < MAX_NUMBER_OF_ATTRIBUTE_REPORTS)
            {
                deviceInfo[counter].reportTime = customStringToString(currentItem->reportTime);
                deviceInfo[counter].clusterId = customStringToString(currentItem->clusterId);
                deviceInfo[counter].attributeId = customStringToString(currentItem->attributeId);
                deviceInfo[counter].data = customStringToString(currentItem->data);

                counter++;
            }
            else
            {
                break;
            }
        }
    }
}

/**
 * Helper function to collect the values
 * found in the rejoin list if its not NULL
 * and stores the values in device Info
 *
 * @param rejoinList - the list to go though
 * @param deviceInfo - the list of rejoinDeviceInfo to hold the values
 */
static void convertRejoins(icLinkedList *rejoinList, rejoinDeviceInfo *deviceInfo)
{
    if (rejoinList != NULL && deviceInfo != NULL)
    {
        int counter = 0;
        scoped_icLinkedListIterator *listIter = linkedListIteratorCreate(rejoinList);

        while (linkedListIteratorHasNext(listIter))
        {
            const deviceRejoinItem *currItem = linkedListIteratorGetNext(listIter);

            if (currItem == NULL)
            {
                icLogWarn(LOG_TAG, "%s: unable to find rejoin item in rejoin event list", __FUNCTION__);
                continue;
            }

            if (counter < MAX_NUMBER_OF_REJOINS)
            {
                deviceInfo[counter].time = customStringToString(currItem->rejoinTime);
                deviceInfo[counter].isSecure = customStringToString(currItem->isSecure);

                counter++;
            }
            else
            {
                break;
            }
        }
    }
}

/**
 * Helper function to collect the values
 * found in the check-in list if its not NULL
 * and stores the values in device Info
 *
 * @param checkInList - the list to go though
 * @param deviceInfo - the list of check-ins to hold the values
 */
static void convertCheckIns(icLinkedList *checkInList, checkInDeviceInfo *deviceInfo)
{
    if (checkInList != NULL && deviceInfo != NULL)
    {
        int counter = 0;
        scoped_icLinkedListIterator *listIter = linkedListIteratorCreate(checkInList);

        while (linkedListIteratorHasNext(listIter))
        {
            const char *currItem = linkedListIteratorGetNext(listIter);

            if (counter < MAX_NUMBER_OF_CHECK_INS)
            {
                deviceInfo[counter].time = customStringToString(currItem);
                counter++;
            }
            else
            {
                break;
            }
        }
    }
}

/**
 * Helper function to create the string list
 * for all of the device's information and events
 *
 * NOTE: each device list has to be in the format:
 *
 * "uuid,manufacturer,model,firmwareVersion,upgradeStatus,
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
 * rssi(ne/fe),temperature,batteryVoltage,batteryState,
 * lowBattery,commFailure,troubled,
 * bypassed,tampered,faulted,
 * totalRejoinCounter,
 * totalSecureRejoinCounter,
 * totalUnSecureRejoinCounter,
 * totalDuplicateSequenceNumberCounter,
 * totalApsAckFailureCounter"
 *
 * NOTE: caller must free return object if not null
 *
 * @param uuid - the deivce id
 * @param deviceInfo - all of the basic resource and enpoint information
 * @param attList - the attribute reports list
 * @param rejoinList - the rejoin event list
 * @param checkInList - the check-in event list
 * @param deviceCounters - the device counters for device
 * @param deviceClass - the device type
 * @return - a massive string list containing all of the information above or NULL
 */
static char *createDeviceStringList(const char *uuid,
                                    basicDeviceInfo *deviceInfo,
                                    attributeDeviceInfo *attList,
                                    rejoinDeviceInfo *rejoinList,
                                    checkInDeviceInfo *checkInList,
                                    deviceEventCounterItem *deviceCounters,
                                    const char *deviceClass)
{
    if (uuid == NULL || deviceInfo == NULL || attList == NULL || rejoinList == NULL || checkInList == NULL ||
        deviceCounters == NULL || deviceClass == NULL)
    {
        return NULL;
    }

    char *listOfValues = stringBuilder("%s,%s,%s,%s,%s,"
                                       "%s,%s,%s,%s,"
                                       "%s,%s,%s,%s,"
                                       "%s,%s,%s,%s,"
                                       "%s,%s,%s,%s,"
                                       "%s,%s,%s,%s,"
                                       "%s,%s,%s,%s,"
                                       "%s,%s,%s,%s,"
                                       "%s,%s,%s,%s,"
                                       "%s,%s,"
                                       "%s,%s,"
                                       "%s,%s,"
                                       "%s,%s,"
                                       "%s,%s,"
                                       "%s,"
                                       "%s,"
                                       "%s,"
                                       "%s,"
                                       "%s,"
                                       "%s,%s,%s/%s,"
                                       "%s/%s,%s,%s,%s,"
                                       "%s,%s,%s,"
                                       "%s,%s,%s,"
                                       "%d,"
                                       "%d,"
                                       "%d,"
                                       "%d,"
                                       "%d",
                                       uuid,
                                       deviceInfo->manufacturer,
                                       deviceInfo->model,
                                       deviceInfo->firmVer,
                                       deviceInfo->upgradeStatus,
                                       attList[0].reportTime,
                                       attList[0].clusterId,
                                       attList[0].attributeId,
                                       attList[0].data,
                                       attList[1].reportTime,
                                       attList[1].clusterId,
                                       attList[1].attributeId,
                                       attList[1].data,
                                       attList[2].reportTime,
                                       attList[2].clusterId,
                                       attList[2].attributeId,
                                       attList[2].data,
                                       attList[3].reportTime,
                                       attList[3].clusterId,
                                       attList[3].attributeId,
                                       attList[3].data,
                                       attList[4].reportTime,
                                       attList[4].clusterId,
                                       attList[4].attributeId,
                                       attList[4].data,
                                       attList[5].reportTime,
                                       attList[5].clusterId,
                                       attList[5].attributeId,
                                       attList[5].data,
                                       attList[6].reportTime,
                                       attList[6].clusterId,
                                       attList[6].attributeId,
                                       attList[6].data,
                                       attList[7].reportTime,
                                       attList[7].clusterId,
                                       attList[7].attributeId,
                                       attList[7].data,
                                       rejoinList[0].time,
                                       rejoinList[0].isSecure,
                                       rejoinList[1].time,
                                       rejoinList[1].isSecure,
                                       rejoinList[2].time,
                                       rejoinList[2].isSecure,
                                       rejoinList[3].time,
                                       rejoinList[3].isSecure,
                                       rejoinList[4].time,
                                       rejoinList[4].isSecure,
                                       checkInList[0].time,
                                       checkInList[1].time,
                                       checkInList[2].time,
                                       checkInList[3].time,
                                       checkInList[4].time,
                                       deviceClass,
                                       deviceInfo->hardVer,
                                       deviceInfo->nearLqi,
                                       deviceInfo->farLqi,
                                       deviceInfo->nearRssi,
                                       deviceInfo->farRssi,
                                       deviceInfo->temp,
                                       deviceInfo->batteryVolts,
                                       deviceInfo->batteryState,
                                       deviceInfo->lowBattery,
                                       deviceInfo->commFail,
                                       deviceInfo->troubled,
                                       deviceInfo->bypassed,
                                       deviceInfo->tampered,
                                       deviceInfo->faulted,
                                       deviceCounters->totalRejoinEvents,
                                       deviceCounters->totalSecureRejoinEvents,
                                       deviceCounters->totalUnSecureRejoinEvents,
                                       deviceCounters->totalDuplicateSeqNumEvents,
                                       deviceCounters->totalDuplicateSeqNumEvents);

    return listOfValues;
}

#endif // BARTON_CONFIG_ZIGBEE
