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
 * zigbeeEventTracker.c
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

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <icConcurrent/repeatingTask.h>
#include <icConcurrent/timedWait.h>
#include <icLog/logging.h>
#include <icTypes/icHashMap.h>
#include <icTypes/icLinkedList.h>
#include <icTypes/icStringBuffer.h>
#include <icUtil/stringUtils.h>
#include <zhal/zhal.h>

#include "commonDeviceDefs.h"
#include "devicePrivateProperties.h"
#include "deviceService.h"
#include "deviceServiceConfiguration.h"
#include "icConcurrent/threadUtils.h"
#include "provider/barton-core-property-provider.h"
#include "subsystems/zigbee/zigbeeCommonIds.h"
#include "subsystems/zigbee/zigbeeEventTracker.h"
#include "subsystems/zigbee/zigbeeSubsystem.h"
#include "zigbeeClusters/iasZoneCluster.h"
#include "zigbeeClusters/pollControlCluster.h"

#define LOG_TAG                           "zigbeeEventTracker"

// the min and max Zigbee channels
//
#define MIN_ZIGBEE_CHANNEL                11
#define MAX_ZIGBEE_CHANNEL                25

// defaults for channel energy scans
//
#define DEFAULT_NUM_SCAN_PER_CHANNEL      10
#define DEFAULT_CHANNEL_SCAN_DUR_MS       100  // milliseconds
#define DEFAULT_SCAN_DELAY_PER_CHANNEL_MS 1000 // milliseconds
#define DEFAULT_CHANNEL_COLLECT_DELAY_MIN 60   // minutes

// defaults for collection enabled
//
#define DEFAULT_EVENT_COLLECT_ENABLED     true
#define DEFAULT_CHANNEL_COLLECT_ENABLED   false


// time conversions
#define NANOSECONDS_IN_MILLISECONDS       1000000

// the event value to look at in the holder
typedef enum
{
    BASIC_REJOIN_CHECK_IN_EVENT_TYPE = 0,
    APS_ACK_FAILURE_EVENT_TYPE,
    DUPLICATE_SEQ_NUM_EVENT_TYPE,
    DETAILED_REJOIN_EVENT_TYPE,
    CHECK_IN_EVENT_TYPE,
    ATTRIBUTE_REPORT_EVENT_TYPE,
    IAS_NOTIFY_SENSOR_DELAY_EVENT_TYPE,
    MAGNETIC_STRENGTH_REPORT_EVENT_TYPE
} statEventType;

// the event value as a sting
static const char *statEventTypeNames[] = {"BASIC_REJOIN_CHECK_IN_EVENT_TYPE",
                                           "APS_ACK_FAILURE_EVENT_TYPE",
                                           "DUPLICATE_SEQ_NUM_EVENT_TYPE",
                                           "DETAILED_REJOIN_EVENT_TYPE",
                                           "CHECK_IN_EVENT_TYPE",
                                           "ATTRIBUTE_REPORT_EVENT_TYPE",
                                           "IAS_NOTIFY_SENSOR_DELAY_EVENT_TYPE",
                                           "MAGNETIC_STRENGTH_REPORT_EVENT_TYPE",
                                           NULL};

// items in the deviceCollection
typedef struct _deviceStatHolder
{
    icLinkedList *attributeReportList;
    icLinkedList *detailRejoinList;
    icLinkedList *checkInList;
    char *magneticStrength; // magnetic strength for hall effect sensor
    deviceEventCounterItem *eventCounters;
    uint32_t previousSeqNum;
} deviceStatHolder;

// the collection of events
static int numberOfDeviceUpgSuccess = 0;
static icLinkedList *deviceUpgradeFailures = NULL;
static icLinkedList *channelCollection = NULL;
static icHashMap *deviceCollection = NULL;

// mutex for collections
static pthread_mutex_t eventTrackerMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t channelCondition;

// flags for report event
static bool reportEventCollectingTurnedOn = DEFAULT_EVENT_COLLECT_ENABLED;
static bool channelEventCollectingTurnedOn = DEFAULT_CHANNEL_COLLECT_ENABLED;

// task id for collecting channel scans
static uint32_t channelStartRepeatingTaskId = 0;
static uint32_t channelRunRepeatingTaskId = 0;

// parameters for the Channel Energy Scan collecting
static uint32_t channelScanDuration = 0;    // the channel scan duration in milliseconds
static uint32_t numofScanPerChannel = 0;    // the number of scans be channel
static uint32_t scanDelayPerChannel = 0;    // the delay time before scanning the next channel in milliseconds
static uint32_t channelCollectionDelay = 0; // the delay before looking through

// private functions
//
static bool locateAndAddEventToCollection(statEventType type, void *arg, const char *deviceId);
static bool addEventInfoToDeviceStatHolderList(deviceStatHolder *holder, void *arg, statEventType listEventType);
static bool addEventCounterInfoToDeviceStatHolder(deviceStatHolder *holder, void *arg, statEventType counterEventType);
static void addSensorDelayEvent(const char *deviceUuid, uint16_t *delayQS);
static void checkAndAddDuplicateSequenceNumEvent(const char *uuid, uint8_t seqNum);
static void addCheckInEvent(const char *uuid);
static void initChannelDataCollection();
static void updateChannelDataCollectionDelayAmount();
static void stopChannelDataCollecting();
static void startChannelRepeatingTaskCallback(void *args);
static void channelEnergyDataCollectingCallback(void *arg);
static bool isReportCollectingTurnedOn();
static bool isChannelCollectingTurnedOn();
static char *dataToString(const uint8_t *dataList, uint16_t len);
static deviceStatHolder *createDeviceStatHolder();
static deviceAttributeItem *createDeviceAttributeItem(ReceivedAttributeReport *newReport);
static deviceRejoinItem *createDeviceRejoinItem(bool isSecure);
static deviceUpgFailureItem *createDeviceFailureItem(const char *deviceId);
static void destroyDeviceStatHolder(void *key, void *value);

/**
 * Adds attribute report for NON-SENSOR Devices into our collection
 * It adds the item based off of Device UUID or EUI64.
 *
 * @param report - the attribute report received
 */
void zigbeeEventTrackerAddAttributeReportEvent(ReceivedAttributeReport *report)
{
    // need to check to see if event collecting is turned on
    if (isReportCollectingTurnedOn() == false)
    {
        return;
    }

    // sanity check
    if (report == NULL)
    {
        icLogError(LOG_TAG, "%s: unable to add received attribute report for telemetry, report was NULL", __FUNCTION__);
        return;
    }

    // get device ID
    char *uuid = zigbeeSubsystemEui64ToId(report->eui64);
    if (uuid == NULL)
    {
        icLogError(LOG_TAG, "%s: got a bad device ID string value for attribute report event", __FUNCTION__);
        return;
    }

    // need to look at the device type
    //
    bool shouldBail = false;
    icDevice *physicalDevice = NULL;

    // now get the device class
    physicalDevice = deviceServiceGetDevice(uuid);
    if (physicalDevice != NULL && physicalDevice->deviceClass != NULL)
    {
        // ignore sensor devices
        //
        if (strcasecmp(physicalDevice->deviceClass, "sensor") == 0)
        {
            shouldBail = true;
        }
        else
        {
            // no longer needed
            deviceDestroy(physicalDevice);
        }
    }
    else
    {
        icLogError(LOG_TAG, "%s: got a bad device for attribute report", __FUNCTION__);
        shouldBail = true;
    }

    // clean up and exit
    if (shouldBail)
    {
        free(uuid);
        deviceDestroy(physicalDevice);
        return;
    }

    // now make the report
    deviceAttributeItem *newReport = createDeviceAttributeItem(report);
    if (newReport != NULL)
    {
        // now add new attribute report
        if (!locateAndAddEventToCollection(ATTRIBUTE_REPORT_EVENT_TYPE, newReport, uuid))
        {
            // need to cleanup, since didn't add attribute report event
            destroyDeviceAttributeItem(newReport);
            icLogWarn(
                LOG_TAG, "%s: unable to save information about attribute report for device %s", __FUNCTION__, uuid);
        }
    }
    else
    {
        icLogWarn(
            LOG_TAG, "%s: unable to create a duplicate of the attribute report for device %s", __FUNCTION__, uuid);
    }

    // cleanup
    free(uuid);
}

/**
 * Adds rejoin event per device,
 * should be added for every device
 *
 * @param eui64 - the device eui64
 */
void zigbeeEventTrackerAddRejoinEvent(uint64_t eui64, bool wasSecure)
{
    // need to check to see if event collecting is turned on
    if (isReportCollectingTurnedOn() == false)
    {
        return;
    }

    // get device ID
    char *uuid = zigbeeSubsystemEui64ToId(eui64);
    if (uuid == NULL)
    {
        icLogError(LOG_TAG, "%s: got a bad device ID  string value for rejoin check in event", __FUNCTION__);
        return;
    }

    // Log line used for Telemetry... DO NOT CHANGE
    //
    icLogInfo(LOG_TAG, "got a %s rejoin for device %s", wasSecure ? "SECURE" : "UNSECURE", uuid);

    // prepare was secure arg
    bool *wasSecureInput = malloc(sizeof(bool));
    *wasSecureInput = wasSecure;

    // add to basic rejoin event counter
    if (!locateAndAddEventToCollection(BASIC_REJOIN_CHECK_IN_EVENT_TYPE, wasSecureInput, uuid))
    {
        icLogWarn(LOG_TAG, "%s: unable to save basic rejoin information for device %s", __FUNCTION__, uuid);
    }

    // no longer needed
    free(wasSecureInput);

    // need to create the rejoin item
    deviceRejoinItem *newRejoinItem = createDeviceRejoinItem(wasSecure);
    if (newRejoinItem != NULL)
    {
        // now add the new rejoin info
        if (!locateAndAddEventToCollection(DETAILED_REJOIN_EVENT_TYPE, newRejoinItem, uuid))
        {
            // need to cleanup, since didn't add rejoin item event
            destroyDeviceRejoinItem(newRejoinItem);
            icLogWarn(LOG_TAG, "%s: unable to save detailed check in rejoin event for device %s", __FUNCTION__, uuid);
        }
    }
    else
    {
        icLogWarn(LOG_TAG, "%s: unable to create new rejoin check-in data for device %s", __FUNCTION__, uuid);
    }

    // cleanup
    free(uuid);
}

/**
 * Adds check-in event and/or duplicate sequence number event
 * per device, should be added for every device
 *
 * @param command - the clusterCommand received
 */
void zigbeeEventTrackerAddClusterCommandEvent(ReceivedClusterCommand *command)
{
    // need to check to see if event collecting is turned on
    if (isReportCollectingTurnedOn() == false)
    {
        return;
    }

    // sanity check
    if (command == NULL)
    {
        icLogError(
            LOG_TAG, "%s: got a bad command for check-in event and/or duplicate sequence number event", __FUNCTION__);
        return;
    }

    // get device ID
    char *uuid = zigbeeSubsystemEui64ToId(command->eui64);
    if (uuid == NULL)
    {
        icLogError(LOG_TAG,
                   "%s: got a bad device ID string value for check-in event and/or duplicate sequence number event",
                   __FUNCTION__);
        return;
    }

    // always check on the duplicate sequence numbers
    //
    checkAndAddDuplicateSequenceNumEvent(uuid, command->seqNum);

    // need to check for valid reasons that this is a check-in event
    // if DEVICE_CHECKIN is defined, we have legacy security device support enabled and this short circuits the check
#ifdef DEVICE_CHECKIN
    if (command->commandId == DEVICE_CHECKIN)
    {
        addCheckInEvent(uuid);
    }
    else
#endif
        if ((command->clusterId == POLL_CONTROL_CLUSTER_ID && command->commandId == POLL_CONTROL_CHECKIN_COMMAND_ID) ||
            (command->mfgSpecific && command->mfgCode == COMCAST_MFG_ID_INCORRECT &&
             command->clusterId == IAS_ZONE_CLUSTER_ID &&
             command->commandId == IAS_ZONE_STATUS_CHANGE_NOTIFICATION_COMMAND_ID))
    {
        addCheckInEvent(uuid);
    }

    // Send the delay to telemetry only if its a sensor with an IAS Zone status change.
    icDevice *physicalDevice = NULL;
    physicalDevice = deviceServiceGetDevice(uuid);
    if (physicalDevice != NULL && physicalDevice->deviceClass != NULL)
    {
        if ((command->clusterId == IAS_ZONE_CLUSTER_ID &&
             command->commandId == IAS_ZONE_STATUS_CHANGE_NOTIFICATION_COMMAND_ID) &&
            (strcasecmp(physicalDevice->deviceClass, SENSOR_DC) == 0))
        {
            IASZoneStatusChangedNotification payload;
            if ((iasZoneClusterExtract(&payload, command) == true) && (payload.delayQS > 0))
            {
                addSensorDelayEvent(uuid, &payload.delayQS);
            }
        }
    }
    // cleanup
    deviceDestroy(physicalDevice);
    free(uuid);
}

void zigbeeEventTrackerAddMagneticStrengthReport(const char *deviceUuid, const char *valueString)
{
    if (isReportCollectingTurnedOn() == false)
    {
        return;
    }

    if (deviceUuid == NULL || valueString == NULL)
    {
        return;
    }

    if (locateAndAddEventToCollection(MAGNETIC_STRENGTH_REPORT_EVENT_TYPE, (void *) valueString, deviceUuid) == false)
    {
        icLogWarn(LOG_TAG, "%s: unable to add magnetic strength report event for device %s", __func__, deviceUuid);
    }
}

/**
 * Adds sensor fault/restore Message Delay value
 *
 * @param deviceUuid - the device uuid
 * @param delayQS - the sensor delay (in quarter seconds)
 */
static void addSensorDelayEvent(const char *deviceUuid, uint16_t *delayQS)
{
    if (isReportCollectingTurnedOn() == false)
    {
        return;
    }

    // sanity check
    if (deviceUuid == NULL)
    {
        return;
    }

    if (!locateAndAddEventToCollection(IAS_NOTIFY_SENSOR_DELAY_EVENT_TYPE, (void *) delayQS, deviceUuid))
    {
        icLogWarn(LOG_TAG, "%s: unable to add Sensor delay time for device %s", __func__, deviceUuid);
    }
}

/**
 * Adds apsAckFailure events per device,
 * should be added for every device
 *
 * @param eui64 - the device eui64
 */
void zigbeeEventTrackerAddApsAckFailureEvent(uint64_t eui64)
{
    // need to check to see if event collecting is turned on
    if (isReportCollectingTurnedOn() == false)
    {
        return;
    }

    // get device ID
    char *uuid = zigbeeSubsystemEui64ToId(eui64);
    if (uuid == NULL)
    {
        icLogError(LOG_TAG, "%s: got a bad device ID string value for aps ack failure event", __FUNCTION__);
        return;
    }

    // add to aps ack failure event counter
    if (!locateAndAddEventToCollection(APS_ACK_FAILURE_EVENT_TYPE, NULL, uuid))
    {
        icLogWarn(LOG_TAG, "%s: unable to add aps ack failure event for device %s", __FUNCTION__, uuid);
    }

    // cleanup
    free(uuid);
}

/**
 * Adds a count for how many devices have a successful
 * upgrade, should be for every device
 */
void zigbeeEventTrackerAddDeviceFirmwareUpgradeSuccessEvent()
{
    // need to check to see if event collecting is turned on
    if (isReportCollectingTurnedOn() == false)
    {
        return;
    }

    icLogDebug(LOG_TAG, "%s: got a successful device FW upgrade event, increasing counter", __FUNCTION__);

    pthread_mutex_lock(&eventTrackerMutex);

    // only need to track the number of successes
    numberOfDeviceUpgSuccess++;

    pthread_mutex_unlock(&eventTrackerMutex);
}

/**
 * Adds a device upgrade failure per device,
 * should be for every device
 *
 * @param eui64 - the device eui64
 */
void zigbeeEventTrackerAddDeviceFirmwareUpgradeFailureEvent(uint64_t eui64)
{
    // need to check to see if event collecting is turned on
    if (isReportCollectingTurnedOn() == false)
    {
        return;
    }

    // get device id
    char *uuid = zigbeeSubsystemEui64ToId(eui64);
    if (uuid == NULL)
    {
        icLogError(LOG_TAG, "%s: got a bad device ID string value for device upgrade failure event", __FUNCTION__);
        return;
    }

    icLogDebug(LOG_TAG, "%s: got a device FW upgrade failure event for device %s", __FUNCTION__, uuid);

    // create the failure item
    deviceUpgFailureItem *newUpgradeFailureItem = createDeviceFailureItem(uuid);
    if (newUpgradeFailureItem != NULL)
    {
        pthread_mutex_lock(&eventTrackerMutex);

        // add upgrade failure
        if (!linkedListAppend(deviceUpgradeFailures, newUpgradeFailureItem))
        {
            // need to cleanup, since didn't add upgrade failure event
            icLogWarn(LOG_TAG, "%s: unable to add device FW upgrade event for device %s", __FUNCTION__, uuid);
            destroyDeviceUpgFailureItem(newUpgradeFailureItem);
        }
        else
        {
            icLogDebug(LOG_TAG,
                       "%s: was able to successfully add device FW upgrade failure event for device %s",
                       __FUNCTION__,
                       uuid);
        }

        pthread_mutex_unlock(&eventTrackerMutex);
    }
    else
    {
        icLogWarn(LOG_TAG, "%s: unable to create firmware failure item for device %s", __FUNCTION__, uuid);
    }

    // cleanup
    free(uuid);
}

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
icLinkedList *zigbeeEventTrackerCollectAttributeReportEventsForDevice(const char *deviceId)
{
    icLinkedList *retVal = NULL;

    // sanity check
    if (deviceId == NULL || isReportCollectingTurnedOn() == false)
    {
        return retVal;
    }

    pthread_mutex_lock(&eventTrackerMutex);

    // sanity check
    if (deviceCollection != NULL)
    {
        // locate device id
        //
        deviceStatHolder *tmp =
            (deviceStatHolder *) hashMapGet(deviceCollection, (void *) deviceId, (uint16_t) strlen(deviceId));
        if (tmp != NULL && tmp->attributeReportList != NULL)
        {
            // now make a copy of the linked list
            //
            retVal = linkedListClone(tmp->attributeReportList);
        }
        else
        {
            icLogTrace(
                LOG_TAG, "%s: unable to find device %s in collection, no events have occurred", __FUNCTION__, deviceId);
        }
    }

    pthread_mutex_unlock(&eventTrackerMutex);

    return retVal;
}

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
icLinkedList *zigbeeEventTrackerCollectRejoinEventsForDevice(const char *deviceId)
{
    icLinkedList *retVal = NULL;

    // sanity check
    if (deviceId == NULL || isReportCollectingTurnedOn() == false)
    {
        return retVal;
    }

    pthread_mutex_lock(&eventTrackerMutex);

    // sanity check
    if (deviceCollection != NULL)
    {
        // locate device id
        //
        deviceStatHolder *tmp =
            (deviceStatHolder *) hashMapGet(deviceCollection, (void *) deviceId, (uint16_t) strlen(deviceId));
        if (tmp != NULL && tmp->detailRejoinList != NULL)
        {
            // now make a copy of the linked list
            //
            retVal = linkedListClone(tmp->detailRejoinList);
        }
        else
        {
            icLogTrace(
                LOG_TAG, "%s: unable to find device %s in collection, no events have occurred", __FUNCTION__, deviceId);
        }
    }

    pthread_mutex_unlock(&eventTrackerMutex);

    return retVal;
}

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
icLinkedList *zigbeeEventTrackerCollectCheckInEventsForDevice(const char *deviceId)
{
    icLinkedList *retVal = NULL;

    // sanity check and need to check to see if event collecting is turned on
    if (deviceId == NULL || isReportCollectingTurnedOn() == false)
    {
        return retVal;
    }

    pthread_mutex_lock(&eventTrackerMutex);

    // sanity check
    if (deviceCollection != NULL)
    {
        // locate device id
        //
        deviceStatHolder *tmp =
            (deviceStatHolder *) hashMapGet(deviceCollection, (void *) deviceId, (uint16_t) strlen(deviceId));
        if (tmp != NULL && tmp->checkInList != NULL)
        {
            // now make a copy of the linked list
            //
            retVal = linkedListClone(tmp->checkInList);
        }
        else
        {
            icLogTrace(
                LOG_TAG, "%s: unable to find device %s in collection, no events have occurred", __FUNCTION__, deviceId);
        }
    }

    pthread_mutex_unlock(&eventTrackerMutex);

    return retVal;
}

/**
 * Collects the magnetic strength report for device
 *
 * NOTE - caller must free return string if not NULL
 *
 * @param deviceId - the device ID to grab
 * @return - returns a copy of the magnetic strength string
 */
char *zigbeeEventTrackerCollectMagneticStrengthReportsForDevice(const char *deviceId)
{
    char *retVal = NULL;

    if (deviceId == NULL || isReportCollectingTurnedOn() == false)
    {
        return retVal;
    }

    LOCK_SCOPE(eventTrackerMutex);

    if (deviceCollection != NULL)
    {
        deviceStatHolder *tmp =
            (deviceStatHolder *) hashMapGet(deviceCollection, (void *) deviceId, (uint16_t) strlen(deviceId));
        if (tmp != NULL && tmp->magneticStrength != NULL)
        {
            retVal = strdupOpt(tmp->magneticStrength);
        }
        else
        {
            icLogTrace(LOG_TAG,
                       "%s: unable to find device %s in collection, no magnetic strength report events have occurred",
                       __func__,
                       deviceId);
        }
    }

    return retVal;
}

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
deviceEventCounterItem *zigbeeEventTrackerCollectEventCountersForDevice(const char *deviceId)
{
    deviceEventCounterItem *retVal = calloc(1, sizeof(deviceEventCounterItem));

    // sanity check and need to check to see if event collecting is turned on
    if (deviceId == NULL || isReportCollectingTurnedOn() == false)
    {
        return retVal;
    }

    pthread_mutex_lock(&eventTrackerMutex);

    // sanity check
    if (deviceCollection != NULL)
    {
        // locate device id
        //
        deviceStatHolder *tmp =
            (deviceStatHolder *) hashMapGet(deviceCollection, (void *) deviceId, (uint16_t) strlen(deviceId));
        if (tmp != NULL && tmp->eventCounters != NULL)
        {
            // now make a copy of device event counter item
            //
            memcpy(retVal, tmp->eventCounters, sizeof(deviceEventCounterItem));
        }
        else
        {
            icLogTrace(
                LOG_TAG, "%s: unable to find device %s in collection, no events have occurred", __FUNCTION__, deviceId);
        }
    }

    pthread_mutex_unlock(&eventTrackerMutex);

    return retVal;
}

/**
 * Collects all the successful device upgrade events,
 * will reset the number once this is called
 *
 * @return - the number of successful upgrades
 */
int zigbeeEventTrackerCollectFirmwareUpgradeSuccessEvents()
{
    int retVal = 0;

    // need to check to see if event collecting is turned on
    if (isReportCollectingTurnedOn() == false)
    {
        return retVal;
    }

    pthread_mutex_lock(&eventTrackerMutex);

    // get and reset counter
    retVal = numberOfDeviceUpgSuccess;
    numberOfDeviceUpgSuccess = 0;

    pthread_mutex_unlock(&eventTrackerMutex);

    return retVal;
}

/**
 * Collects all the failures device upgrade events,
 * will reset the the list once this is called
 *
 * NOTE: caller must free return object if not NULL
 * via destroyDeviceUpgFailureItem
 *
 * @return - returns a copy of deviceUpgFailureItems list
 */
icLinkedList *zigbeeEventTrackerCollectFirmwareUpgradeFailureEvents()
{
    icLinkedList *retVal = NULL;

    // need to check to see if event collecting is turned on
    if (isReportCollectingTurnedOn() == false)
    {
        return retVal;
    }

    pthread_mutex_lock(&eventTrackerMutex);

    // sanity check
    if (deviceUpgradeFailures != NULL)
    {
        // return the list and create a new one
        retVal = deviceUpgradeFailures;
        deviceUpgradeFailures = linkedListCreate();
    }

    pthread_mutex_unlock(&eventTrackerMutex);

    return retVal;
}

/**
 * Collects all of the channel energy scan stats
 *
 * NOTE: caller must free return object if not NULL
 * via standardFree function
 *
 * @return - returns a copy of channelEnergyScanCollection
 */
icLinkedList *zigbeeEventTrackerCollectChannelEnergyScanStats()
{
    icLinkedList *retVal = NULL;

    // need to check to see if channel collecting is turned on
    if (isChannelCollectingTurnedOn() == false)
    {
        return retVal;
    }

    pthread_mutex_lock(&eventTrackerMutex);

    // sanity check
    if (channelCollection != NULL)
    {
        // create a copy of channel stats list
        retVal = linkedListClone(channelCollection);
    }

    pthread_mutex_unlock(&eventTrackerMutex);

    return retVal;
}

/**
 * function for freeing memory for the device attribute item
 *
 * @param item - the item to destroy
 */
void destroyDeviceAttributeItem(void *item)
{
    deviceAttributeItem *tmp = (deviceAttributeItem *) item;

    if (tmp != NULL)
    {
        free(tmp->reportTime);
        free(tmp->clusterId);
        free(tmp->attributeId);
        free(tmp->data);
        free(tmp);
    }
}

/**
 * function for freeing memory for the device rejoin item
 *
 * @param item - the item to destroy
 */
void destroyDeviceRejoinItem(void *item)
{
    deviceRejoinItem *tmp = (deviceRejoinItem *) item;

    if (tmp != NULL)
    {
        free(tmp->rejoinTime);
        free(tmp->isSecure);
        free(tmp);
    }
}

/**
 * function for freeing memory for the upgrade failure item
 *
 * @param item - the item to destroy
 */
void destroyDeviceUpgFailureItem(void *item)
{
    deviceUpgFailureItem *tmp = (deviceUpgFailureItem *) item;

    if (tmp != NULL)
    {
        free(tmp->deviceId);
        free(tmp);
    }
}

/**
 * Used to set up the event tracker
 */
void initEventTracker()
{
    pthread_mutex_lock(&eventTrackerMutex);

    // create device collection hash map, upgrade failure event list,
    // and channel energy scan stat list
    //
    deviceCollection = hashMapCreate();
    deviceUpgradeFailures = linkedListCreate();
    channelCollection = linkedListCreate();

    // get properties at the start
    //
    g_autoptr(BCorePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();

    reportEventCollectingTurnedOn = b_core_property_provider_get_property_as_bool(
        propertyProvider, CPE_ZIGBEE_REPORT_DEVICE_INFO_ENABLED, DEFAULT_EVENT_COLLECT_ENABLED);
    channelEventCollectingTurnedOn = b_core_property_provider_get_property_as_bool(
        propertyProvider, CPE_DIAGNOSTIC_ZIGBEEDATA_ENABLED, DEFAULT_CHANNEL_COLLECT_ENABLED);
    channelScanDuration = b_core_property_provider_get_property_as_uint32(
        propertyProvider, CPE_DIAGNOSTIC_ZIGBEEDATA_CHANNEL_SCAN_DURATION_MS, DEFAULT_CHANNEL_SCAN_DUR_MS);
    numofScanPerChannel = b_core_property_provider_get_property_as_uint32(
        propertyProvider, CPE_DIAGNOSTIC_ZIGBEEDATA_PER_CHANNEL_NUMBER_OF_SCANS, DEFAULT_NUM_SCAN_PER_CHANNEL);
    scanDelayPerChannel = b_core_property_provider_get_property_as_uint32(
        propertyProvider, CPE_DIAGNOSTIC_ZIGBEEDATA_CHANNEL_SCAN_DELAY_MS, DEFAULT_SCAN_DELAY_PER_CHANNEL_MS);
    channelCollectionDelay = b_core_property_provider_get_property_as_uint32(
        propertyProvider, CPE_DIAGNOSTIC_ZIGBEEDATA_COLLECTION_DELAY_MIN, DEFAULT_CHANNEL_COLLECT_DELAY_MIN);

    // need to init the conditional for channel collecting
    //
    initTimedWaitCond(&channelCondition);

    bool startChannelCollecting = channelEventCollectingTurnedOn;

    pthread_mutex_unlock(&eventTrackerMutex);

    // if channel collecting is turned on
    // start the repeating task for
    // collecting channel stats
    //
    if (startChannelCollecting)
    {
        initChannelDataCollection();
    }
}

/**
 * Used to clean up all of the collections
 */
void shutDownEventTracker()
{
    // un-schedule channel scan task(s)
    //
    stopChannelDataCollecting();

    // cleanup the device collection hash map, upgrade failure list,
    // and the channel energy scan stat list
    //
    pthread_mutex_lock(&eventTrackerMutex);

    hashMapDestroy(deviceCollection, destroyDeviceStatHolder);
    linkedListDestroy(deviceUpgradeFailures, destroyDeviceUpgFailureItem);
    linkedListDestroy(channelCollection, NULL);
    deviceCollection = NULL;
    deviceUpgradeFailures = NULL;
    channelCollection = NULL;

    pthread_mutex_unlock(&eventTrackerMutex);
}

/**
 * Helper function for finding the device stat holder in deviceCollection
 * if a holder is not found then one is created and added, then adds the
 * event. Uses the input arg depending on the event
 *
 * NOTE: will grab the lock
 *
 * @param type - the event type to look at
 * @param arg - the input needed for event type, value can be NULL
 * @param deviceId - the device ID
 * @return - true if item was added, false if other wise
 */
static bool locateAndAddEventToCollection(statEventType type, void *arg, const char *deviceId)
{
    bool retVal = false;

    // sanity check
    if (deviceId == NULL)
    {
        icLogError(LOG_TAG, "unable to add new value into collection inputs are null");
        return retVal;
    }

    icLogTrace(LOG_TAG,
               "%s: attempting to collect event type %s for device %s",
               __FUNCTION__,
               statEventTypeNames[type],
               deviceId);

    uint16_t keyLen = (uint16_t) strlen(deviceId);

    pthread_mutex_lock(&eventTrackerMutex);

    // look though collection list to find the device id
    //
    deviceStatHolder *currHolder = hashMapGet(deviceCollection, (void *) deviceId, keyLen);

    // the holder does not exist
    // need to make one and
    // add to collection
    //
    if (currHolder == NULL)
    {
        currHolder = createDeviceStatHolder();
        char *deviceIdCopy = strdup(deviceId);

        // if the new report was not added
        // need to cleanup
        //
        if (!hashMapPut(deviceCollection, deviceIdCopy, keyLen, currHolder))
        {
            icLogError(LOG_TAG,
                       "%s: unable to add new deviceStatHolder %s for adding item into collection",
                       __FUNCTION__,
                       deviceId);
            destroyDeviceStatHolder(deviceIdCopy, currHolder);
            pthread_mutex_unlock(&eventTrackerMutex);
            return retVal;
        }
    }

    // determine how to add new event
    //
    switch (type)
    {
        // for updating device counters
        case BASIC_REJOIN_CHECK_IN_EVENT_TYPE:
        case APS_ACK_FAILURE_EVENT_TYPE:
        case DUPLICATE_SEQ_NUM_EVENT_TYPE:
        case IAS_NOTIFY_SENSOR_DELAY_EVENT_TYPE:
            retVal = addEventCounterInfoToDeviceStatHolder(currHolder, arg, type);
            break;

        // for adding events into a list
        case DETAILED_REJOIN_EVENT_TYPE:
        case CHECK_IN_EVENT_TYPE:
        case ATTRIBUTE_REPORT_EVENT_TYPE:
            retVal = addEventInfoToDeviceStatHolderList(currHolder, arg, type);
            break;

        case MAGNETIC_STRENGTH_REPORT_EVENT_TYPE:
            // remove old value
            free(currHolder->magneticStrength);

            currHolder->magneticStrength = arg;
            break;
        // should never happen
        default:
            icLogError(LOG_TAG,
                       "%s: unable to determine event type %s for new event on device %s",
                       __FUNCTION__,
                       statEventTypeNames[type],
                       deviceId);
            break;
    }

    pthread_mutex_unlock(&eventTrackerMutex);

    return retVal;
}

/**
 * Function to reset the sensor delay counter for device after a telemetry upload
 *
 * @param deviceId - the device ID to grab
 */
void zigbeeEventTrackerResetSensorDelayCountersForDevice(const char *deviceId)
{
    if (deviceId != NULL)
    {
        uint16_t keyLen = (uint16_t) strlen(deviceId);
        LOCK_SCOPE(eventTrackerMutex);
        deviceStatHolder *currHolder = hashMapGet(deviceCollection, (void *) deviceId, keyLen);
        if (currHolder != NULL)
        {
            currHolder->eventCounters->cumulativeSensorDelayQS = 0;
        }
    }
}

/**
 * Helper function for adding the new event item into
 * one of the stats holder's list.
 *
 * This is used for the attribute report list,
 * the detailed rejoin list, and the check-in list
 *
 * NOTE: assumes event lock is held
 *
 * @param holder - the holder
 * @param arg - the event item to add, value CANNOT be NULL
 * @param listEventType - the type of list being looked at
 * @return - true if it was added to the list, false if otherwise
 */
static bool addEventInfoToDeviceStatHolderList(deviceStatHolder *holder, void *arg, statEventType listEventType)
{
    // sanity check
    if (holder == NULL || arg == NULL)
    {
        icLogError(LOG_TAG,
                   "%s: got a bad argument or bad holder for event type %s ... so bailing",
                   __FUNCTION__,
                   statEventTypeNames[listEventType]);
        return false;
    }

    // determine the current size, the max size allowed,
    // and which list to look at, if list is at max size
    // remove the last item
    //
    int currentNumOfItems = 0;
    icLinkedList *holderList = NULL;
    switch (listEventType)
    {
        // device attribute report events
        case ATTRIBUTE_REPORT_EVENT_TYPE:
        {
            holderList = holder->attributeReportList;
            currentNumOfItems = linkedListCount(holderList);

            // if at max, remove the last item
            if (currentNumOfItems == MAX_NUMBER_OF_ATTRIBUTE_REPORTS)
            {
                deviceAttributeItem *oldItem =
                    (deviceAttributeItem *) linkedListRemove(holderList, (uint32_t) currentNumOfItems - 1);
                destroyDeviceAttributeItem(oldItem);
            }

            break;
        }

        // device rejoin events
        case DETAILED_REJOIN_EVENT_TYPE:
        {
            holderList = holder->detailRejoinList;
            currentNumOfItems = linkedListCount(holderList);

            // if at max, remove the last item
            if (currentNumOfItems == MAX_NUMBER_OF_REJOINS)
            {
                deviceRejoinItem *oldItem =
                    (deviceRejoinItem *) linkedListRemove(holderList, (uint32_t) currentNumOfItems - 1);
                destroyDeviceRejoinItem(oldItem);
            }

            break;
        }

        // device check-in events
        case CHECK_IN_EVENT_TYPE:
        {
            holderList = holder->checkInList;
            currentNumOfItems = linkedListCount(holderList);

            // if at max, remove the last item
            if (currentNumOfItems == MAX_NUMBER_OF_CHECK_INS)
            {
                char *oldItem = (char *) linkedListRemove(holderList, (uint32_t) currentNumOfItems - 1);
                free(oldItem);
            }

            break;
        }

        // something has gone horribly wrong...
        default:
        {
            icLogError(LOG_TAG,
                       "%s: unable to determine event type %s when determining which event list to use ... so bailing",
                       __FUNCTION__,
                       statEventTypeNames[listEventType]);
            return false;
        }
    }

    // add the new item to the beginning of list
    //
    if (linkedListPrepend(holderList, arg) == true)
    {
        icLogTrace(
            LOG_TAG, "%s: was successfully able to add event %s", __FUNCTION__, statEventTypeNames[listEventType]);
        return true;
    }
    else
    {
        // could not add item for some reason
        icLogWarn(
            LOG_TAG,
            "%s: unable to add new event type %s into the deviceStatHolder, for some strange reason ... so bailing",
            __FUNCTION__,
            statEventTypeNames[listEventType]);
        return false;
    }
}

/**
 * Helper function for adding a event for the
 * eventCounters in the device holder inside the
 * deviceCollections.
 *
 * This is used for the rejoin event counters
 * (un-secure, secure, and total rejoins), aps ack failure
 * event counter, and the duplicate sequence number
 * event counter
 *
 * NOTE: assumes event lock is held
 *
 * @param holder - the device holder
 * @param arg - input for certain event needed, can be NULL
 * @param counterEventType - the type of counterEventType
 * @return - true if the value was updated, false if otherwise
 */
static bool addEventCounterInfoToDeviceStatHolder(deviceStatHolder *holder, void *arg, statEventType counterEventType)
{
    bool retVal = false;

    // sanity check
    if (holder == NULL || holder->eventCounters == NULL)
    {
        icLogError(LOG_TAG,
                   "%s: unable to use holder for event type %s ... so bailing",
                   __FUNCTION__,
                   statEventTypeNames[counterEventType]);
        return retVal;
    }

    // look at event type
    //
    switch (counterEventType)
    {
        // device rejoin counter events
        case BASIC_REJOIN_CHECK_IN_EVENT_TYPE:
        {
            bool *isSecureCheckIn = (bool *) arg;
            if (isSecureCheckIn != NULL)
            {
                // see which counter to increase, based
                // on, if the rejoin was secure or not
                //
                if (*isSecureCheckIn == true)
                {
                    holder->eventCounters->totalSecureRejoinEvents++;
                    icLogTrace(LOG_TAG, "%s: successfully increased secure rejoin counter", __FUNCTION__);
                }
                else
                {
                    holder->eventCounters->totalUnSecureRejoinEvents++;
                    icLogTrace(LOG_TAG, "%s: successfully increased un-secure rejoin counter", __FUNCTION__);
                }

                // always increase the total rejoin counter
                holder->eventCounters->totalRejoinEvents++;
                icLogTrace(LOG_TAG,
                           "%s: successfully increased %s counters",
                           __FUNCTION__,
                           statEventTypeNames[counterEventType]);
                retVal = true;
            }
            else
            {
                icLogError(LOG_TAG,
                           "%s: got a bad argument for event type %s ... so bailing",
                           __FUNCTION__,
                           statEventTypeNames[counterEventType]);
            }

            break;
        }

        // device aps ack failure counter events
        case APS_ACK_FAILURE_EVENT_TYPE:
        {
            holder->eventCounters->totalApsAckFailureEvents++;
            retVal = true;
            icLogTrace(
                LOG_TAG, "%s: successfully increased %s counter", __FUNCTION__, statEventTypeNames[counterEventType]);
            break;
        }

        // duplicate seqNum counter events
        case DUPLICATE_SEQ_NUM_EVENT_TYPE:
        {
            uint8_t *newSeqNum = (uint8_t *) arg;
            if (newSeqNum != NULL)
            {
                // see if the sequences are the same
                //
                if (*newSeqNum == holder->previousSeqNum)
                {
                    holder->eventCounters->totalDuplicateSeqNumEvents++;
                    icLogTrace(LOG_TAG,
                               "%s: successfully increased %s counter",
                               __FUNCTION__,
                               statEventTypeNames[counterEventType]);
                }
                else
                {
                    // if they are not, store the new sequence number
                    holder->previousSeqNum = *newSeqNum;
                    icLogTrace(LOG_TAG, "%s: seq Numbers are not the same, not increasing event counter", __FUNCTION__);
                }

                retVal = true;
            }
            else
            {
                icLogError(LOG_TAG,
                           "%s: got a bad argument for event type %s ... so bailing",
                           __FUNCTION__,
                           statEventTypeNames[counterEventType]);
            }

            break;
        }

            // sensor fault/restore delay events
        case IAS_NOTIFY_SENSOR_DELAY_EVENT_TYPE:
        {
            uint16_t delayQS = *(uint16_t *) arg;
            holder->eventCounters->cumulativeSensorDelayQS += delayQS;
            icLogTrace(LOG_TAG,
                       "%s: Total Device Delay value %" PRIu16,
                       __func__,
                       holder->eventCounters->cumulativeSensorDelayQS);
            retVal = true;
            break;
        }

        // something has gone horribly wrong...
        default:
        {
            icLogError(LOG_TAG,
                       "%s: unable to determine the even type %s ... so bailing",
                       __FUNCTION__,
                       statEventTypeNames[counterEventType]);
            break;
        }
    }

    return retVal;
}

/**
 * Helper function to Check the previous seq num from the
 * cluster command and compares it to the new seq num,
 * if they are the same then we will increase the counter
 *
 * @param uuid - the device UUID
 * @param seqNum - the new sequence number
 */
static void checkAndAddDuplicateSequenceNumEvent(const char *uuid, uint8_t seqNum)
{
    // sanity check
    if (uuid == NULL)
    {
        icLogError(LOG_TAG, "%s: got a bad device ID string value for duplicate sequence number event", __FUNCTION__);
        return;
    }

    // prepare new Sequence Number arg
    uint8_t *newSeqNum = malloc(1);
    *newSeqNum = seqNum;

    // add to duplicate seq number event counter
    if (locateAndAddEventToCollection(DUPLICATE_SEQ_NUM_EVENT_TYPE, newSeqNum, uuid) == false)
    {
        icLogWarn(LOG_TAG, "%s: unable to add duplicate sequence number event for device %s", __FUNCTION__, uuid);
    }

    // cleanup
    free(newSeqNum);
}

/**
 * Helper function to add check-in event per device,
 * should be added for every single device
 * Only adds event if property is turned on
 *
 * @param uuid - the device UUID
 */
static void addCheckInEvent(const char *uuid)
{
    // sanity check
    if (uuid == NULL)
    {
        icLogError(LOG_TAG, "%s: got a bad device ID string value for check-in event", __FUNCTION__);
        return;
    }

    // create the time stamp arg
    time_t currentTime = time(NULL);
    char *timeStamp = stringBuilder("%ld", currentTime);

    // add to check-in event list
    if (locateAndAddEventToCollection(CHECK_IN_EVENT_TYPE, timeStamp, uuid) == false)
    {
        // need to cleanup, since didn't add timestamp
        free(timeStamp);
        icLogWarn(LOG_TAG, "%s: unable to add check-in event for device %s", __FUNCTION__, uuid);
    }
}

/**
 * Helper function to start the channel collection
 * with the a initial delay of channelCollectionDelay
 *
 * TODO: remove the delayed task from starting the repeating task once repeating task with initial delay is implemented
 */
static void initChannelDataCollection()
{
    pthread_mutex_lock(&eventTrackerMutex);

    // make sure both channel tasks are not running
    //
    if (channelStartRepeatingTaskId == 0 && channelRunRepeatingTaskId == 0)
    {
        // schedule a delayed task to start the repeating task for channel collecting
        //
        icLogTrace(LOG_TAG,
                   "%s: starting repeating task for collecting channel scans, with time of %" PRIu32 " minutes",
                   __FUNCTION__,
                   channelCollectionDelay);
        channelStartRepeatingTaskId =
            scheduleDelayTask(channelCollectionDelay, DELAY_MINS, startChannelRepeatingTaskCallback, NULL);
    }
    else
    {
        icLogTrace(LOG_TAG, "%s: already collecting channel scans, not starting another", __FUNCTION__);
    }

    pthread_mutex_unlock(&eventTrackerMutex);
}

/**
 * Helper function to update the the delay amount
 * for the channel scan collecting
 */
static void updateChannelDataCollectionDelayAmount()
{
    // grab values in the lock
    //
    pthread_mutex_lock(&eventTrackerMutex);
    uint32_t repeatingTaskId = channelRunRepeatingTaskId;
    uint32_t delayedTaskId = channelStartRepeatingTaskId;
    uint32_t delayChange = channelCollectionDelay;
    pthread_mutex_unlock(&eventTrackerMutex);

    // look at delayed task first
    //
    if (delayedTaskId != 0)
    {
        // update the delayed task with the new time
        //
        icLogTrace(LOG_TAG,
                   "%s: updating delayed task for starting collecting channel scans, with a new time of %" PRIu32
                   " minutes",
                   __FUNCTION__,
                   delayChange);
        rescheduleDelayTask(delayedTaskId, delayChange, DELAY_MINS);
    }

    // now look at the repeating task if delayed task does not exist
    //
    else if (repeatingTaskId != 0)
    {
        // update the repeating task with the new time
        //
        icLogTrace(LOG_TAG,
                   "%s: updating repeating task for collecting channel scans, with a new time of %" PRIu32 " minutes",
                   __FUNCTION__,
                   delayChange);
        changeRepeatingTask(repeatingTaskId, delayChange, DELAY_MINS, false);
    }
}

/**
 * Helper function for stopping the channel
 * scan collecting repeating task
 *
 * NOTE: will grab the lock
 */
static void stopChannelDataCollecting()
{
    // grab values in the lock
    //
    pthread_mutex_lock(&eventTrackerMutex);
    uint32_t repeatingTaskId = channelRunRepeatingTaskId;
    uint32_t delayedTaskedId = channelStartRepeatingTaskId;
    // Turn it off too so it will stop
    channelEventCollectingTurnedOn = false;
    pthread_mutex_unlock(&eventTrackerMutex);

    // for the repeating task
    //
    if (repeatingTaskId != 0)
    {
        pthread_mutex_lock(&eventTrackerMutex);

        // reset the task id
        //
        channelRunRepeatingTaskId = 0;

        // in-case we are sleeping tell the condition to finish
        //
        pthread_cond_broadcast(&channelCondition);

        pthread_mutex_unlock(&eventTrackerMutex);

        // cancel the repeating task
        //
        icLogTrace(LOG_TAG, "%s: stopping repeating task for collecting channel scans", __FUNCTION__);
        cancelRepeatingTask(repeatingTaskId);
    }

    // for the delayed task
    //
    if (delayedTaskedId != 0)
    {
        // reset the task id
        //
        pthread_mutex_lock(&eventTrackerMutex);
        channelStartRepeatingTaskId = 0;
        pthread_mutex_unlock(&eventTrackerMutex);

        // cancel the delayed task
        //
        icLogTrace(LOG_TAG, "%s: stopping delayed task starting collecting channel scans", __FUNCTION__);
        cancelDelayTask(delayedTaskedId);
    }
}

/**
 * Callback from a delayed task to start the
 * repeating task for channel scan collecting
 *
 * TODO: remove the delayed task from starting the repeating task once repeating task with initial delay is implemented
 *
 * NOTE: will grab the lock
 *
 * @param arg - this should be NULL at all times
 */
static void startChannelRepeatingTaskCallback(void *args)
{
    pthread_mutex_lock(&eventTrackerMutex);

    // make sure no repeating task is running
    //
    if (channelRunRepeatingTaskId == 0)
    {
        // schedule the repeating task and store the task ID
        //
        channelRunRepeatingTaskId =
            createRepeatingTask(channelCollectionDelay, DELAY_MINS, channelEnergyDataCollectingCallback, NULL);
    }
    else
    {
        icLogTrace(LOG_TAG, "%s: already collecting channel scans, not starting another", __FUNCTION__);
    }

    // reset the delayed task id, for starting channel collecting
    //
    channelStartRepeatingTaskId = 0;

    pthread_mutex_unlock(&eventTrackerMutex);
}

/**
 * The callback for channel energy scan stat collecting
 *
 * NOTE: will grab lock when needed
 *
 * @param arg - this should be NULL at all times
 */
static void channelEnergyDataCollectingCallback(void *arg)
{
    icLogTrace(LOG_TAG, "%s: starting channel energy scans", __FUNCTION__);

    // set up parameters,
    // doing this so the lock does not have
    // to be grabbed and released inside
    // the loop more than needed
    //
    pthread_mutex_lock(&eventTrackerMutex);

    // channel scan configuration
    uint32_t scanDur = channelScanDuration;
    uint32_t numOfScan = numofScanPerChannel;

    pthread_mutex_unlock(&eventTrackerMutex);

    // loop through the channels
    //
    for (uint8_t channelNum = MIN_ZIGBEE_CHANNEL; channelNum <= MAX_ZIGBEE_CHANNEL; channelNum++)
    {
        // see if collecting is turned off during our sleep
        //
        if (isChannelCollectingTurnedOn() == false)
        {
            break;
        }

        // set channel to can for
        //
        uint8_t channelToScan[1] = {channelNum};

        // run energy scan for channel
        //
        icLinkedList *response = zhalPerformEnergyScan(channelToScan, 1, scanDur, numOfScan);

        // sanity check
        if (response != NULL)
        {
            // look at the first element of scan result
            //
            zhalEnergyScanResult *scanResult = (zhalEnergyScanResult *) linkedListGetElementAt(response, 0);
            if (scanResult != NULL)
            {
                pthread_mutex_lock(&eventTrackerMutex);

                // see if we already have that item in our list
                //
                channelEnergyScanDataItem *scanDataItem =
                    (channelEnergyScanDataItem *) linkedListGetElementAt(channelCollection, channelNum);
                if (scanDataItem == NULL)
                {
                    // since it does not exist create it and populate its information
                    //
                    scanDataItem = calloc(1, sizeof(channelEnergyScanDataItem));
                    scanDataItem->channel = channelNum;
                    scanDataItem->average = scanResult->averageRssi;
                    scanDataItem->max = scanResult->maxRssi;
                    scanDataItem->min = scanResult->minRssi;

                    // and add it to list
                    //
                    linkedListAppend(channelCollection, scanDataItem);
                }
                else
                {
                    // since it does exist just update the new values found
                    //
                    scanDataItem->channel = channelNum;
                    scanDataItem->average = scanResult->averageRssi;
                    scanDataItem->max = scanResult->maxRssi;
                    scanDataItem->min = scanResult->minRssi;
                }

                pthread_mutex_unlock(&eventTrackerMutex);
            }
            else
            {
                icLogWarn(LOG_TAG, "%s: did not find scan result from response", __FUNCTION__);
            }

            // cleanup
            linkedListDestroy(response, NULL);
        }
        else
        {
            icLogWarn(LOG_TAG, "%s: did not get a response for channel energy scan from zhal", __FUNCTION__);
        }

        // now wait the given delay time before next channel scan in milliseconds,
        // if not the last channel to look at
        //
        if (channelNum != MAX_ZIGBEE_CHANNEL)
        {
            pthread_mutex_lock(&eventTrackerMutex);
            incrementalCondTimedWaitMillis(&channelCondition, &eventTrackerMutex, scanDelayPerChannel);
            pthread_mutex_unlock(&eventTrackerMutex);
        }
    }

    icLogTrace(LOG_TAG, "%s: done scanning channels", __FUNCTION__);
}

/**
 * Callback method for the property change event listener
 *
 * NOTE: will grab the lock when needed
 *
 */
void zigbeeEventTrackerPropertyCallback(const char *propertyName, const char *propertyValue)
{
    if (propertyName != NULL && propertyValue != NULL)
    {
        // turn report event collecting on or off
        //
        if (strcmp(propertyName, CPE_ZIGBEE_REPORT_DEVICE_INFO_ENABLED) == 0)
        {
            bool tmp = stringToBool(propertyValue);

            // store new value
            //
            pthread_mutex_lock(&eventTrackerMutex);
            reportEventCollectingTurnedOn = tmp;
            pthread_mutex_unlock(&eventTrackerMutex);
        }

        // turn channel event collecting on or off
        //
        else if (strcmp(propertyName, CPE_DIAGNOSTIC_ZIGBEEDATA_ENABLED) == 0)
        {
            bool tmp = stringToBool(propertyValue);

            // store new value
            //
            pthread_mutex_lock(&eventTrackerMutex);
            channelEventCollectingTurnedOn = tmp;
            pthread_mutex_unlock(&eventTrackerMutex);

            // start/stop channel collecting depending on if property is true or false
            //
            if (tmp)
            {
                initChannelDataCollection();
            }
            else
            {
                stopChannelDataCollecting();
            }
        }

        // the channel collection delay in minutes
        //
        else if (strcmp(propertyName, CPE_DIAGNOSTIC_ZIGBEEDATA_COLLECTION_DELAY_MIN) == 0)
        {
            uint32_t tmp = DEFAULT_CHANNEL_COLLECT_DELAY_MIN;
            stringToUint32(propertyValue, &tmp);

            // store new value
            //
            pthread_mutex_lock(&eventTrackerMutex);
            channelCollectionDelay = tmp;
            pthread_mutex_unlock(&eventTrackerMutex);

            // since the collection delay has changed need to update the repeating task
            //
            updateChannelDataCollectionDelayAmount();
        }

        // the delay of scan per channel in milliseconds
        //
        else if (strcmp(propertyName, CPE_DIAGNOSTIC_ZIGBEEDATA_CHANNEL_SCAN_DELAY_MS) == 0)
        {
            uint32_t tmp = DEFAULT_SCAN_DELAY_PER_CHANNEL_MS;
            stringToUint32(propertyValue, &tmp);

            // store new value
            //
            pthread_mutex_lock(&eventTrackerMutex);
            scanDelayPerChannel = tmp;
            pthread_mutex_unlock(&eventTrackerMutex);
        }

        // the scan duration per channel in milliseconds
        //
        else if (strcmp(propertyName, CPE_DIAGNOSTIC_ZIGBEEDATA_CHANNEL_SCAN_DURATION_MS) == 0)
        {
            uint32_t tmp = DEFAULT_CHANNEL_SCAN_DUR_MS;
            stringToUint32(propertyValue, &tmp);

            // store new value
            //
            pthread_mutex_lock(&eventTrackerMutex);
            channelScanDuration = tmp;
            pthread_mutex_unlock(&eventTrackerMutex);
        }

        // the number of scan per channel
        //
        else if (strcmp(propertyName, CPE_DIAGNOSTIC_ZIGBEEDATA_PER_CHANNEL_NUMBER_OF_SCANS) == 0)
        {
            uint32_t tmp = DEFAULT_NUM_SCAN_PER_CHANNEL;
            stringToUint32(propertyValue, &tmp);

            // store new value
            //
            pthread_mutex_lock(&eventTrackerMutex);
            numofScanPerChannel = tmp;
            pthread_mutex_unlock(&eventTrackerMutex);
        }
    }
    else
    {
        icLogError(LOG_TAG, "%s: got an property event with no key and value", __FUNCTION__);
    }
}

/**
 * Helper function for getting if report collecting is turn on or off
 *
 * NOTE: will grab the collecting configuration lock
 *
 * @return - either true or false
 */
static bool isReportCollectingTurnedOn()
{
    pthread_mutex_lock(&eventTrackerMutex);
    bool retVal = reportEventCollectingTurnedOn;
    pthread_mutex_unlock(&eventTrackerMutex);

    return retVal;
}

/**
 * Helper function for getting if channel collecting is turn on or off
 *
 * NOTE: will grab the collecting configuration lock
 *
 * @return - either true or false
 */
static bool isChannelCollectingTurnedOn()
{
    pthread_mutex_lock(&eventTrackerMutex);
    bool retVal = channelEventCollectingTurnedOn;
    pthread_mutex_unlock(&eventTrackerMutex);

    return retVal;
}

/**
 * Helper function for converting the data list into a string list
 *
 * NOTE: caller must free return object, it will never be null
 *
 * @param dataList  - the int list
 * @param len - the size of the list
 * @return - string created from list
 */
static char *dataToString(const uint8_t *dataList, uint16_t len)
{
    char *retval = NULL;

    // sanity check
    if (dataList == NULL || len == 0)
    {
        // just do empty string
        retval = strdup("");
    }
    // if there is only one item
    else if (len == 1)
    {
        retval = stringBuilder("[%d]", dataList[0]);
    }
    // for everything else
    else
    {
        // determine the total size first
        // this really means:
        // (the num of items) * (the max number of digits uint8_t could be) + the num of (',') needed + '/0' + "[]"
        uint32_t totalSize = (uint32_t) ((len * 3) + len + 2);

        // now build the string
        icStringBuffer *strBuff = stringBufferCreate(totalSize);
        if (strBuff != NULL)
        {
            // add the beginning of the bracket
            stringBufferAppend(strBuff, "[");

            // loop through each element and add to string
            for (int count = 0; count < len; count++)
            {
                // create tmp string
                char *tmp = stringBuilder("%d", dataList[count]);
                if (tmp != NULL)
                {
                    // add string
                    stringBufferAppend(strBuff, tmp);

                    // reset memory
                    free(tmp);
                }

                // if count is not the last item
                if (count < (len - 1))
                {
                    stringBufferAppend(strBuff, ",");
                }
            }

            // add ending of the bracket
            stringBufferAppend(strBuff, "]");

            // convert to a string
            retval = stringBufferToString(strBuff);

            // and cleanup
            stringBufferDestroy(strBuff);
        }
        else
        {
            // just do empty string
            retval = strdup("");
        }
    }

    return retval;
}

/**
 * Helper function for creating the device stat holder
 *
 * NOTE: caller must free return object if not being added into
 * the hash map
 *
 * @return - the new holder
 */
static deviceStatHolder *createDeviceStatHolder()
{
    // create the memory
    deviceStatHolder *newHolder = calloc(1, sizeof(deviceStatHolder));

    // create the lists
    newHolder->attributeReportList = linkedListCreate();
    newHolder->detailRejoinList = linkedListCreate();
    newHolder->checkInList = linkedListCreate();
    newHolder->magneticStrength = NULL;

    // create the eventCountersItem
    newHolder->eventCounters = calloc(1, sizeof(deviceEventCounterItem));

    // default the seqNum
    newHolder->previousSeqNum = (uint32_t) -1;

    return newHolder;
}

/**
 * Creates the attribute item to be stored in the attribute holder
 *
 * NOTE: caller must free return object if not being added
 * into list and if not NULL
 *
 * @param reportTime - the time of event
 * @param newReport - the report to copy
 * @return - the new item
 */
static deviceAttributeItem *createDeviceAttributeItem(ReceivedAttributeReport *newReport)
{
    // sanity check
    if (newReport == NULL)
    {
        return NULL;
    }

    deviceAttributeItem *newItem = calloc(1, sizeof(deviceAttributeItem));

    // the time
    time_t currTime = time(NULL);
    newItem->reportTime = stringBuilder("%ld", currTime);

    // the cluster id
    newItem->clusterId = stringBuilder("%d", newReport->clusterId);

    // the attribute id
    newItem->attributeId = stringBuilder("%d", newReport->sourceEndpoint);

    // the data
    newItem->data = dataToString(newReport->reportData, newReport->reportDataLen);

    return newItem;
}

/**
 * Helper function for creating the device rejoin event item
 *
 * NOTE: caller must free return object if not being added into
 * the list and if not NULL
 *
 * @param isSecure - if it was a secure rejoin
 * @return - the new item
 */
static deviceRejoinItem *createDeviceRejoinItem(bool isSecure)
{
    deviceRejoinItem *newItem = calloc(1, sizeof(deviceRejoinItem));

    // the time
    time_t currTime = time(NULL);
    newItem->rejoinTime = stringBuilder("%ld", currTime);

    // if secure
    newItem->isSecure = strdup(isSecure ? "true" : "false");

    return newItem;
}

/**
 * Helper function for creating the device failure event item
 *
 * NOTE: caller must free return object if not being added into
 * the list and if not NULL
 *
 * @param failureTime - the time event occurred
 * @param deviceId - the device id
 * @return - the new item
 */
static deviceUpgFailureItem *createDeviceFailureItem(const char *deviceId)
{
    // sanity check
    if (deviceId == NULL)
    {
        return NULL;
    }

    deviceUpgFailureItem *newItem = calloc(1, sizeof(deviceUpgFailureItem));

    // the device id
    newItem->deviceId = strdup(deviceId);

    // the time
    time_t currTime = time(NULL);
    newItem->failureTime = currTime;

    return newItem;
}

/**
 * function for freeing memory for the attribute report holder item
 *
 * @param key - the key to free
 * @param value - the item to free
 */
static void destroyDeviceStatHolder(void *key, void *value)
{
    char *tmpKey = (char *) key;
    deviceStatHolder *tmpValue = (deviceStatHolder *) value;

    free(tmpKey);

    if (tmpValue != NULL)
    {
        linkedListDestroy(tmpValue->attributeReportList, destroyDeviceAttributeItem);
        linkedListDestroy(tmpValue->detailRejoinList, destroyDeviceRejoinItem);
        linkedListDestroy(tmpValue->checkInList, NULL);
        free(tmpValue->magneticStrength);
        free(tmpValue->eventCounters);
        free(tmpValue);
    }
}
