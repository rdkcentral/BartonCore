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
/*
 * Created by Thomas Lea on 3/22/16.
 */

#include <inttypes.h>
#include <string.h>

#include "subsystems/zigbee/zigbeeCommonIds.h"
#include "subsystems/zigbee/zigbeeSubsystem.h"
#include "zigbeeDefender.h"
#include "zigbeeEventHandler.h"
#include "zigbeeHealthCheck.h"
#include "zigbeeSubsystemPrivate.h"
#include <deviceCommunicationWatchdog.h>
#include <deviceService.h>
#include <deviceServicePrivate.h>
#include <icLog/logging.h>
#include <icUtil/stringUtils.h>
#include <pthread.h>
#include <zhal/zhal.h>

#define LOG_TAG "zigbeeEventHandler"

static bool systemReady = false;

static icHashMap *announcedDevices = NULL;

static pthread_mutex_t announcedDevicesMtx = PTHREAD_MUTEX_INITIALIZER;

// a map of eui64s that have been processed as new during discovery
static icHashMap *devicesProcessed = NULL;

static pthread_mutex_t devicesProcessedMtx = PTHREAD_MUTEX_INITIALIZER;

typedef struct
{
    bool hasJoined;
    zhalDeviceType deviceType;
    zhalPowerSource powerSource;
} AnnouncedDevice;

static void startup(void *ctx)
{
    icLogDebug(LOG_TAG, "startup callback");

    zigbeeSubsystemHandleZhalStartupEvent();
}

/*
 * Save the data from the join or announce, creating the hash map as needed.
 *
 * Return true if we have both announce and join info ready.
 */
static bool saveJoinAnnounceInfo(uint64_t eui64, bool isJoin, zhalDeviceType deviceType, zhalPowerSource powerSource)
{
    bool result = false;

    pthread_mutex_lock(&announcedDevicesMtx);

    // if we havent yet created the hash map, do it now
    if (announcedDevices == NULL)
    {
        announcedDevices = hashMapCreate();
    }

    AnnouncedDevice *ad = hashMapGet(announcedDevices, &eui64, sizeof(uint64_t));
    if (ad == NULL)
    {
        ad = (AnnouncedDevice *) calloc(1, sizeof(AnnouncedDevice));
        uint64_t *key = (uint64_t *) malloc(sizeof(uint64_t));
        *key = eui64;
        hashMapPut(announcedDevices, key, sizeof(uint64_t), ad);
    }

    if (isJoin)
    {
        // ignore the other parameters since they come as part of the announce
        ad->hasJoined = true;
    }
    else
    {
        // this is just the announce, save those bits only
        ad->deviceType = deviceType;
        ad->powerSource = powerSource;
    }

    // check to see if this entry is complete
    if (ad->deviceType != deviceTypeUnknown && ad->powerSource != powerSourceUnknown && ad->hasJoined)
    {
        // ok we are all good for this device
        result = true;
    }

    pthread_mutex_unlock(&announcedDevicesMtx);

    return result;
}

static void clearJoinAnnounceInfo(uint64_t eui64)
{
    pthread_mutex_lock(&announcedDevicesMtx);
    hashMapDelete(announcedDevices, &eui64, sizeof(uint64_t), NULL);
    pthread_mutex_unlock(&announcedDevicesMtx);
}


/*
 * The provided device has joined and announced, so we can discover it
 */
static void processNewDevice(uint64_t eui64)
{
    pthread_mutex_lock(&devicesProcessedMtx);
    if (hashMapGet(devicesProcessed, &eui64, sizeof(uint64_t)) != NULL)
    {
        icLogWarn(LOG_TAG, "%s: %016" PRIx64 " already processed", __FUNCTION__, eui64);
        pthread_mutex_unlock(&devicesProcessedMtx);
        return;
    }
    else
    {
        uint64_t *key = (uint64_t *) malloc(sizeof(uint64_t));
        *key = eui64;
        uint8_t *value = (uint8_t *) malloc(1); // we have to store something in the map
        hashMapPut(devicesProcessed, key, sizeof(uint64_t), value);
    }
    pthread_mutex_unlock(&devicesProcessedMtx);

    IcDiscoveredDeviceDetails *details = zigbeeSubsystemDiscoverDeviceDetails(eui64);

    if (details != NULL)
    {
        pthread_mutex_lock(&announcedDevicesMtx);
        AnnouncedDevice *ad = hashMapGet(announcedDevices, &eui64, sizeof(uint64_t));
        if (ad != NULL)
        {
            details->deviceType = ad->deviceType;
            details->powerSource = ad->powerSource;
        }
        pthread_mutex_unlock(&announcedDevicesMtx);

        // Device commissioning may go awry, causing the device to leave and then rejoin to try again, so
        // the join/announce info needs to be cleared here before the device can try to join/announce again.
        clearJoinAnnounceInfo(eui64);

        zigbeeSubsystemDeviceDiscovered(details);

        freeIcDiscoveredDeviceDetails(details);
    }
    else
    {
        // forget about this device for now... maybe it will come around again during this discovery session
        pthread_mutex_lock(&devicesProcessedMtx);
        hashMapDelete(devicesProcessed, &eui64, sizeof(uint64_t), NULL);
        pthread_mutex_unlock(&devicesProcessedMtx);

        // The device's join/announce info must be cleared here before requesting that it leaves
        // and thus before the device has a chance to join/announce again.
        clearJoinAnnounceInfo(eui64);

        zigbeeSubsystemRequestDeviceLeave(eui64, false, false);
    }
}

/**
 * The ZigbeeCore notified us that a device has announced.
 * If the DeviceJoin ("Association") has already been processed, then start discovery and notify the subsystem.
 * If the DeviceJoin has not been processed yet, add this device to the hash map for consideration later.
 *     Only the DeviceType and PowerSource will get added. deviceJoined will add the eui64.
 *
 * @param eui64
 * @param deviceType the {@link DeviceType} of the zigbee device (end device, router, etc)
 * @param powerSource  the {@link PowerSource} for the device (mains, battery, etc)
 */
static void deviceAnnounced(void *ctx, uint64_t eui64, zhalDeviceType deviceType, zhalPowerSource powerSource)
{
    icLogDebug(LOG_TAG, "deviceAnnounced callback: %016" PRIx64, eui64);

    char *uuid = zigbeeSubsystemEui64ToId(eui64);
    bool known = deviceServiceIsDeviceKnown(uuid);
    free(uuid);
    bool repairing = deviceServiceIsInRecoveryMode();
    icLogDebug(LOG_TAG,
               "%s: known is %s, repairing is %s",
               __FUNCTION__,
               stringValueOfBool(known),
               stringValueOfBool(repairing));
    if ((known == true) && (repairing == false))
    {
        icLogWarn(LOG_TAG, "Device already known, notifying zigbee subsystem.");
        zigbeeSubsystemDeviceAnnounced(eui64, deviceType, powerSource);
    }
    else if ((known == false) && (repairing == true))
    {
        icLogWarn(LOG_TAG, "Device is not already known, but we are in find orphaned mode; ignoring announce");
    }
    else
    {
        if (saveJoinAnnounceInfo(eui64, false, deviceType, powerSource))
        {
            // we have everything we need... discover the device and pass it on
            processNewDevice(eui64);
        }
    }
}

/**
 * The ZigbeeCore notified us that a device has joined (associated).
 *  If the DeviceAnnounce has already been processed, then create the physical device.
 *  If the DeviceAnnounce has not been processed yet, add this device to the hash map for consideration later.
 *     Only the eui64 will get added. deviceAnnounced will add the DeviceType and PowerSource.
 *
 * @param eui64
 */
static void deviceJoined(void *ctx, uint64_t eui64)
{
    icLogDebug(LOG_TAG, "deviceJoined callback: %016" PRIx64, eui64);

    AUTO_CLEAN(free_generic__auto) char *uuid = zigbeeSubsystemEui64ToId(eui64);
    bool known = deviceServiceIsDeviceKnown(uuid);
    bool repairing = deviceServiceIsInRecoveryMode();
    bool previouslyRejected = deviceServiceWasDeviceRejected(uuid);
    bool previouslyFailedToPair = deviceServiceDidDeviceFailToPair(uuid);
    bool shouldRequestLeave = false;

    icLogDebug(LOG_TAG,
               "%s: known=%s repairing=%s previouslyRejected=%s",
               __FUNCTION__,
               stringValueOfBool(known),
               stringValueOfBool(repairing),
               stringValueOfBool(previouslyRejected));

    if (previouslyRejected == true)
    {
        icLogWarn(LOG_TAG, "Device previously rejected, requesting that it leave");
        shouldRequestLeave = true;
    }
    else if ((known == true) && (repairing == false))
    {
        icLogWarn(LOG_TAG, "Device already known, ignoring join.");
    }
    else if ((known == false) && (repairing == true))
    {
        icLogWarn(LOG_TAG, "Device is not already known, but we are in find orphaned mode; requesting that it leave");
        shouldRequestLeave = true;
    }
    else
    {
        if (previouslyFailedToPair == true)
        {
            // this device was previously claimed but failed to pair, so we'll try processing it again
            pthread_mutex_lock(&devicesProcessedMtx);
            hashMapDelete(devicesProcessed, &eui64, sizeof(uint64_t), NULL);
            pthread_mutex_unlock(&devicesProcessedMtx);
        }
        if (saveJoinAnnounceInfo(eui64, true, deviceTypeUnknown, powerSourceUnknown))
        {
            // we have everything we need... discover the device and pass it on
            processNewDevice(eui64);
        }
    }

    if (shouldRequestLeave == true && zigbeeSubsystemRequestDeviceLeave(eui64, false, false) == false)
    {
        icLogWarn(LOG_TAG, "%s: request for device to leave network failed: %s", __FUNCTION__, uuid);
    }
}

static void deviceLeft(void *ctx, uint64_t eui64)
{
    icLogDebug(LOG_TAG, "deviceLeft callback: %016" PRIx64, eui64);
}

static void deviceRejoined(void *ctx, uint64_t eui64, bool isSecure)
{
    icLogDebug(LOG_TAG, "deviceRejoined callback: %016" PRIx64 " isSecure %s", eui64, isSecure ? "true" : "false");
    zigbeeSubsystemDeviceRejoined(eui64, isSecure);
}

static void linkKeyUpdated(void *ctx, uint64_t eui64, bool isUsingHashBasedKey)
{
    icLogDebug(LOG_TAG,
               "linkKeyUpdated callback: %016" PRIx64 " and isUsingHashBasedKey : %s",
               eui64,
               stringValueOfBool(isUsingHashBasedKey));
    zigbeeSubsystemLinkKeyUpdated(eui64, isUsingHashBasedKey);
}

static void apsAckFailure(void *ctx, uint64_t eui64)
{
    icLogDebug(LOG_TAG, "apsAckFailure callback: %016" PRIx64, eui64);
    zigbeeSubsystemApsAckFailure(eui64);
}

static void attributeReportReceived(void *ctx, ReceivedAttributeReport *report)
{
    icLogDebug(LOG_TAG,
               "attributeReportReceived callback: %016" PRIx64 " ep %d, cluster %04x",
               report->eui64,
               report->sourceEndpoint,
               report->clusterId);
    zigbeeSubsystemAttributeReportReceived(report);
}

static void clusterCommandReceived(void *ctx, ReceivedClusterCommand *command)
{
    // The Zigbee UART cluster used by M1 LTE adapter get this cluster command
    // a lot when it is paired and any communication with server is going on
    // over cellular and fills up the device service logs
    if (command->clusterId != ZIGBEE_UART_CLUSTER)
    {
        icLogDebug(LOG_TAG,
                   "clusterCommandReceived callback: %016" PRIx64 " ep %d, profileId %04x, cluster %04x",
                   command->eui64,
                   command->sourceEndpoint,
                   command->profileId,
                   command->clusterId);
    }

    zigbeeSubsystemClusterCommandReceived(command);
}

static void zigbeeDeviceOtaUpgradeMessageSent(void *ctx, OtaUpgradeEvent *otaEvent)
{
    icLogDebug(LOG_TAG, "%s callback: %016" PRIx64, __func__, otaEvent->eui64);
    zigbeeSubsystemDeviceOtaUpgradeMessageSent(otaEvent);
}

static void zigbeeDeviceOtaUpgradeMessageReceived(void *ctx, OtaUpgradeEvent *otaEvent)
{
    icLogDebug(LOG_TAG, "%s callback: %016" PRIx64, __func__, otaEvent->eui64);
    zigbeeSubsystemDeviceOtaUpgradeMessageReceived(otaEvent);
}

static void deviceCommunicationSucceeded(void *ctx, uint64_t eui64)
{
    icLogDebug(LOG_TAG, "deviceCommunicationSucceeded callback: %016" PRIx64, eui64);
    char *uuid = zigbeeSubsystemEui64ToId(eui64);

    deviceCommunicationWatchdogPetDevice(uuid);

    free(uuid);
}

static void deviceCommunicationFailed(void *ctx, uint64_t eui64)
{
    icLogDebug(LOG_TAG, "deviceCommunicationFailed callback: %016" PRIx64, eui64);
    char *uuid = zigbeeSubsystemEui64ToId(eui64);

    deviceCommunicationWatchdogForceDeviceInCommFail(uuid);

    free(uuid);
}

static void networkConfigChanged(void *ctx, char *networkConfigData)
{
    icLogDebug(LOG_TAG, "networkConfigChanged callback: networkConfigData=%s", networkConfigData);
    if (systemReady)
    {
        // save this
        deviceServiceSetSystemProperty(NETWORK_BLOB_PROPERTY_NAME, networkConfigData);
        icLogDebug(LOG_TAG, "Saved updated network blob");
    }
    else
    {
        icLogDebug(LOG_TAG, "Ignoring network blob since we are not yet ready");
    }
}

static void networkHealthProblem(void *ctx)
{
    icLogDebug(LOG_TAG, "networkHealthProblem callback");
    zigbeeHealthCheckSetProblem(true);
}

static void networkHealthProblemRestored(void *ctx)
{
    icLogDebug(LOG_TAG, "networkHealthProblemRestored callback");
    zigbeeHealthCheckSetProblem(false);
}

static void panIdAttackDetected(void *ctx)
{
    icLogDebug(LOG_TAG, "panIdAttackDetected callback");
    zigbeeDefenderSetPanIdAttack(true);
}

static void panIdAttackCleared(void *ctx)
{
    icLogDebug(LOG_TAG, "panIdAttackCleared callback");
    zigbeeDefenderSetPanIdAttack(false);
}

static void beaconReceived(void *ctx,
                           uint64_t eui64,
                           uint16_t panId,
                           bool isOpen,
                           bool hasEndDeviceCapacity,
                           bool hasRouterCapability,
                           uint8_t depth)
{
    zigbeeSubsystemDeviceBeaconReceived(eui64, panId, isOpen, hasEndDeviceCapacity, hasRouterCapability, depth);
}


int zigbeeEventHandlerInit(zhalCallbacks *callbacks)
{
    callbacks->startup = startup;
    callbacks->deviceAnnounced = deviceAnnounced;
    callbacks->deviceLeft = deviceLeft;
    callbacks->deviceJoined = deviceJoined;
    callbacks->deviceRejoined = deviceRejoined;
    callbacks->linkKeyUpdated = linkKeyUpdated;
    callbacks->apsAckFailure = apsAckFailure;
    callbacks->attributeReportReceived = attributeReportReceived;
    callbacks->clusterCommandReceived = clusterCommandReceived;
    callbacks->deviceOtaUpgradeMessageSent = zigbeeDeviceOtaUpgradeMessageSent;
    callbacks->deviceOtaUpgradeMessageReceived = zigbeeDeviceOtaUpgradeMessageReceived;
    callbacks->deviceCommunicationSucceeded = deviceCommunicationSucceeded;
    callbacks->deviceCommunicationFailed = deviceCommunicationFailed;
    callbacks->networkConfigChanged = networkConfigChanged;
    callbacks->networkHealthProblem = networkHealthProblem;
    callbacks->networkHealthProblemRestored = networkHealthProblemRestored;
    callbacks->panIdAttackDetected = panIdAttackDetected;
    callbacks->panIdAttackCleared = panIdAttackCleared;
    callbacks->beaconReceived = beaconReceived;

    return 0;
}

int zigbeeEventHandlerSystemReady()
{
    systemReady = true;

    return 0;
}

void zigbeeEventHandlerDiscoveryRunning(bool isRunning)
{
    if (isRunning)
    {
        pthread_mutex_lock(&devicesProcessedMtx);
        devicesProcessed = hashMapCreate();
        pthread_mutex_unlock(&devicesProcessedMtx);
    }
    else
    {
        pthread_mutex_lock(&devicesProcessedMtx);
        hashMapDestroy(devicesProcessed, NULL);
        devicesProcessed = NULL;
        pthread_mutex_unlock(&devicesProcessedMtx);

        pthread_mutex_lock(&announcedDevicesMtx);
        hashMapClear(announcedDevices, NULL);
        pthread_mutex_unlock(&announcedDevicesMtx);
    }
}
