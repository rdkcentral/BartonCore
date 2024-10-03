//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2015 iControl Networks, Inc.
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
// iControl Networks retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------
/*
 * Created by Thomas Lea on 9/19/16.
 */

#include "zhalEventHandler.h"
#include "zhalPrivate.h"
#include <cjson/cJSON.h>
#include <icConcurrent/threadUtils.h>
#include <icLog/logging.h>
#include <icUtil/base64.h>
#include <icUtil/stringUtils.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <zhal/zhal.h>

static pthread_mutex_t lastApsSeqNumsMtx = PTHREAD_MUTEX_INITIALIZER;
static icHashMap *lastApsSeqNums = NULL; // a map from eui64 to the last APS sequence number received from the device

void zhalEventHandlerInit(void)
{
    LOCK_SCOPE(lastApsSeqNumsMtx);

    lastApsSeqNums = hashMapCreate();
}

void zhalEventHandlerTerm(void)
{
    LOCK_SCOPE(lastApsSeqNumsMtx);

    hashMapDestroy(lastApsSeqNums, NULL);
    lastApsSeqNums = NULL;
}

/**
 * See if the provided APS sequence number is the same as the last one received from this device.  If not, store it
 * for later comparison.
 *
 * @param eui64
 * @param apsSeqNum
 * @return true if the provided sequence number is the same as the last message received from the device
 */
static bool isDuplicateApsSequenceNumber(uint64_t eui64, uint8_t apsSeqNum)
{
    bool result = false;

    LOCK_SCOPE(lastApsSeqNumsMtx);

    uint8_t *lastSeqNum = hashMapGet(lastApsSeqNums, &eui64, sizeof(eui64));
    if (lastSeqNum != NULL)
    {
        if (*lastSeqNum == apsSeqNum)
        {
            result = true;
        }
        else
        {
            // delete the last one and clear our pointer so we know to save this one
            hashMapDelete(lastApsSeqNums, &eui64, sizeof(eui64), NULL);
            lastSeqNum = NULL;
        }
    }

    if (lastSeqNum == NULL)
    {
        // we either didnt have a previous entry or it was removed.  either way. save this new one
        lastSeqNum = malloc(sizeof(*lastSeqNum));
        *lastSeqNum = apsSeqNum;
        uint64_t *savedEui64 = malloc(sizeof(*savedEui64));
        *savedEui64 = eui64;

        hashMapPut(lastApsSeqNums, savedEui64, sizeof(*savedEui64), lastSeqNum);
    }

    return result;
}

static void handleClusterCommandReceived(cJSON *event)
{
    cJSON *eui64Json = cJSON_GetObjectItem(event, "eui64");
    cJSON *sourceEndpointJson = cJSON_GetObjectItem(event, "sourceEndpoint");
    cJSON *profileIdJson = cJSON_GetObjectItem(event, "profileId");
    cJSON *directionJson = cJSON_GetObjectItem(event, "direction");
    cJSON *clusterIdJson = cJSON_GetObjectItem(event, "clusterId");
    cJSON *commandIdJson = cJSON_GetObjectItem(event, "commandId");
    cJSON *mfgSpecificJson = cJSON_GetObjectItem(event, "mfgSpecific");
    cJSON *mfgCodeJson = cJSON_GetObjectItem(event, "mfgCode");
    cJSON *seqNumJson = cJSON_GetObjectItem(event, "seqNum");
    cJSON *apsSeqNumJson = cJSON_GetObjectItem(event, "apsSeqNum");
    cJSON *rssiJson = cJSON_GetObjectItem(event, "rssi");
    cJSON *lqiJson = cJSON_GetObjectItem(event, "lqi");
    cJSON *encodedBufJson = cJSON_GetObjectItem(event, "encodedBuf");

    uint64_t eui64 = 0;

    if (eui64Json == NULL || sourceEndpointJson == NULL || profileIdJson == NULL || directionJson == NULL ||
        clusterIdJson == NULL || commandIdJson == NULL || mfgSpecificJson == NULL || mfgCodeJson == NULL ||
        seqNumJson == NULL || apsSeqNumJson == NULL || rssiJson == NULL || lqiJson == NULL || encodedBufJson == NULL)
    {
        icLogError(LOG_TAG, "handleClusterCommandReceived: received incomplete event JSON");
    }
    else if (stringToUnsignedNumberWithinRange(eui64Json->valuestring, &eui64, 16, 0, UINT64_MAX) == true)
    {
        // drop this command if it has the same APS sequence number as the last one we received from this device
        if (isDuplicateApsSequenceNumber(eui64, apsSeqNumJson->valueint) == true)
        {
            icLogWarn(LOG_TAG, "%s: duplicate APS sequence number detected!  Ignoring", __func__);
        }
        else
        {
            ReceivedClusterCommand *command = (ReceivedClusterCommand *) calloc(1, sizeof(ReceivedClusterCommand));

            command->sourceEndpoint = (uint8_t) sourceEndpointJson->valueint;
            command->profileId = (uint16_t) profileIdJson->valueint;
            command->fromServer = directionJson->valueint == 1 ? true : false;
            command->clusterId = (uint16_t) clusterIdJson->valueint;
            command->commandId = (uint8_t) commandIdJson->valueint;
            command->mfgSpecific = (uint8_t) mfgSpecificJson->valueint;
            command->mfgCode = (uint16_t) mfgCodeJson->valueint;
            command->seqNum = (uint8_t) seqNumJson->valueint;
            command->apsSeqNum = (uint8_t) apsSeqNumJson->valueint;
            command->rssi = (int8_t) rssiJson->valueint;
            command->lqi = (uint8_t) lqiJson->valueint;
            command->eui64 = eui64;

            if (icDecodeBase64(encodedBufJson->valuestring, &command->commandData, &command->commandDataLen))
            {
                getCallbacks()->clusterCommandReceived(getCallbackContext(), command);
            }

            freeReceivedClusterCommand(command);
        }
    }
}

static void handleAttributeReportReceived(cJSON *event)
{
    cJSON *eui64Json = cJSON_GetObjectItem(event, "eui64");
    cJSON *sourceEndpointJson = cJSON_GetObjectItem(event, "sourceEndpoint");
    cJSON *clusterIdJson = cJSON_GetObjectItem(event, "clusterId");
    cJSON *encodedBufJson = cJSON_GetObjectItem(event, "encodedBuf");
    cJSON *rssiJson = cJSON_GetObjectItem(event, "rssi");
    cJSON *lqiJson = cJSON_GetObjectItem(event, "lqi");
    cJSON *mfgCodeJson = cJSON_GetObjectItem(event, "mfgCode");

    if (eui64Json == NULL || sourceEndpointJson == NULL || clusterIdJson == NULL || encodedBufJson == NULL ||
        rssiJson == NULL || lqiJson == NULL)
    {
        icLogError(LOG_TAG, "handleAttributeReportReceived: received incomplete event JSON");
    }
    else
    {
        ReceivedAttributeReport *report = (ReceivedAttributeReport *) calloc(1, sizeof(ReceivedAttributeReport));

        report->eui64 = 0;
        report->sourceEndpoint = (uint8_t) sourceEndpointJson->valueint;
        report->clusterId = (uint16_t) clusterIdJson->valueint;
        report->rssi = (int8_t) rssiJson->valueint;
        report->lqi = (uint8_t) lqiJson->valueint;

        sscanf(eui64Json->valuestring, "%016" PRIx64, &report->eui64);

        if (mfgCodeJson != NULL)
        {
            report->mfgId = (uint16_t) mfgCodeJson->valueint;
        }

        if (icDecodeBase64(encodedBufJson->valuestring, &report->reportData, &report->reportDataLen))
        {
            getCallbacks()->attributeReportReceived(getCallbackContext(), report);
        }

        freeReceivedAttributeReport(report);
    }
}

static OtaUpgradeEvent *parseOtaUpgradeMessageEventJson(cJSON *event)
{
    cJSON *otaEventTypeJson = cJSON_GetObjectItem(event, "otaEventType");

    cJSON *eui64Json = cJSON_GetObjectItem(event, "eui64");
    cJSON *timestampJson = cJSON_GetObjectItem(event, "timestamp");
    cJSON *encodedBufJson = cJSON_GetObjectItem(event, "encodedBuf");

    cJSON *sentStatusJson = cJSON_GetObjectItem(event, "sentStatus");

    if (otaEventTypeJson == NULL || eui64Json == NULL || timestampJson == NULL || encodedBufJson == NULL)
    {
        icLogError(LOG_TAG, "%s: received incomplete event JSON", __func__);
        return NULL;
    }
    else
    {
        zhalOtaEventType eventType = ZHAL_OTA_INVALID_EVENT;

        uint64_t eui64 = 0;
        uint64_t timestamp = 0;

        if (otaEventTypeJson->valuestring == NULL || eui64Json->valuestring == NULL ||
            timestampJson->valuestring == NULL)
        {
            icLogError(LOG_TAG, "%s: received NULL JSON value(s)", __func__);
            return NULL;
        }

        if (strcmp(otaEventTypeJson->valuestring, "imageNotifySent") == 0)
        {
            eventType = ZHAL_OTA_IMAGE_NOTIFY_EVENT;
        }
        else if (strcmp(otaEventTypeJson->valuestring, "queryNextImageRequest") == 0)
        {
            eventType = ZHAL_OTA_QUERY_NEXT_IMAGE_REQUEST_EVENT;
        }
        else if (strcmp(otaEventTypeJson->valuestring, "queryNextImageResponseSent") == 0)
        {
            eventType = ZHAL_OTA_QUERY_NEXT_IMAGE_RESPONSE_EVENT;
        }
        else if (strcmp(otaEventTypeJson->valuestring, "upgradeStarted") == 0)
        {
            eventType = ZHAL_OTA_UPGRADE_STARTED_EVENT;
        }
        else if (strcmp(otaEventTypeJson->valuestring, "upgradeEndRequest") == 0)
        {
            eventType = ZHAL_OTA_UPGRADE_END_REQUEST_EVENT;
        }
        else if (strcmp(otaEventTypeJson->valuestring, "upgradeEndResponseSent") == 0)
        {
            eventType = ZHAL_OTA_UPGRADE_END_RESPONSE_EVENT;
        }
        else if (strcmp(otaEventTypeJson->valuestring, "legacyBootloadUpgradeStarted") == 0)
        {
            eventType = ZHAL_OTA_LEGACY_BOOTLOAD_UPGRADE_STARTED_EVENT;
        }
        else if (strcmp(otaEventTypeJson->valuestring, "legacyBootloadUpgradeFailed") == 0)
        {
            eventType = ZHAL_OTA_LEGACY_BOOTLOAD_UPGRADE_FAILED_EVENT;
        }
        else if (strcmp(otaEventTypeJson->valuestring, "legacyBootloadUpgradeCompleted") == 0)
        {
            eventType = ZHAL_OTA_LEGACY_BOOTLOAD_UPGRADE_COMPLETED_EVENT;
        }

        if (stringToUnsignedNumberWithinRange(eui64Json->valuestring, &eui64, 16, 0, UINT64_MAX) &&
            stringToUnsignedNumberWithinRange(timestampJson->valuestring, &timestamp, 10, 0, UINT64_MAX))
        {
            OtaUpgradeEvent *otaEvent = (OtaUpgradeEvent *) calloc(1, sizeof(OtaUpgradeEvent));

            otaEvent->eui64 = eui64;
            otaEvent->timestamp = timestamp;
            otaEvent->eventType = eventType;

            if (sentStatusJson != NULL)
            {
                otaEvent->sentStatus = malloc(sizeof(ZHAL_STATUS));
                *otaEvent->sentStatus = (ZHAL_STATUS) sentStatusJson->valueint;
            }

            if (icDecodeBase64(encodedBufJson->valuestring, &otaEvent->buffer, &otaEvent->bufferLen))
            {
                return otaEvent;
            }
            else
            {
                icLogError(LOG_TAG, "%s: received invalid event JSON. Base64 decoding failed", __func__);

                freeOtaUpgradeEvent(otaEvent);
                return NULL;
            }
        }
        else
        {
            icLogError(LOG_TAG,
                       "%s: received invalid event JSON. Value out of bounds for input [eui64]: %s, [timestamp]: %s",
                       __func__,
                       eui64Json->valuestring,
                       timestampJson->valuestring);
            return NULL;
        }
    }
}

static void handleOtaUpgradeMessageSentEventReceived(cJSON *event)
{
    OtaUpgradeEvent *otaEvent = parseOtaUpgradeMessageEventJson(event);

    if (otaEvent != NULL)
    {
        switch (otaEvent->eventType)
        {
            case ZHAL_OTA_IMAGE_NOTIFY_EVENT:
            case ZHAL_OTA_QUERY_NEXT_IMAGE_RESPONSE_EVENT:
            case ZHAL_OTA_UPGRADE_END_RESPONSE_EVENT:

                // Validate sentStatus JSON
                if (otaEvent->sentStatus == NULL)
                {
                    icLogError(LOG_TAG,
                               "%s: received invalid event JSON. otaEventType %d must have sentStatus field",
                               __func__,
                               otaEvent->eventType);
                    break;
                }

                getCallbacks()->deviceOtaUpgradeMessageSent(getCallbackContext(), otaEvent);
                break;

            default:
                icLogError(
                    LOG_TAG, "%s: received invalid event JSON. Invalid otaEventType %d", __func__, otaEvent->eventType);
                break;
        }

        freeOtaUpgradeEvent(otaEvent);
    }
}

static void handleOtaUpgradeMessageReceivedEventReceived(cJSON *event)
{
    OtaUpgradeEvent *otaEvent = parseOtaUpgradeMessageEventJson(event);

    if (otaEvent != NULL)
    {
        switch (otaEvent->eventType)
        {
            case ZHAL_OTA_LEGACY_BOOTLOAD_UPGRADE_STARTED_EVENT:
            case ZHAL_OTA_LEGACY_BOOTLOAD_UPGRADE_FAILED_EVENT:
            case ZHAL_OTA_LEGACY_BOOTLOAD_UPGRADE_COMPLETED_EVENT:
            case ZHAL_OTA_QUERY_NEXT_IMAGE_REQUEST_EVENT:
            case ZHAL_OTA_UPGRADE_STARTED_EVENT:
            case ZHAL_OTA_UPGRADE_END_REQUEST_EVENT:

                getCallbacks()->deviceOtaUpgradeMessageReceived(getCallbackContext(), otaEvent);
                break;

            default:
                icLogError(
                    LOG_TAG, "%s: received invalid event JSON. Invalid otaEventType %d", __func__, otaEvent->eventType);
                break;
        }

        freeOtaUpgradeEvent(otaEvent);
    }
}

static void handleDeviceCommunicationSucceededEventReceived(cJSON *event)
{
    cJSON *eui64Json = cJSON_GetObjectItem(event, "eui64");

    if (eui64Json == NULL)
    {
        icLogError(LOG_TAG, "handleDeviceCommunicationSucceededEventReceived: received incomplete event JSON");
    }
    else
    {
        uint64_t eui64 = 0;
        sscanf(eui64Json->valuestring, "%016" PRIx64, &eui64);

        getCallbacks()->deviceCommunicationSucceeded(getCallbackContext(), eui64);
    }
}

static void handleDeviceCommunicationFailedEventRecieved(cJSON *event)
{
    cJSON *eui64Json = cJSON_GetObjectItem(event, "eui64");

    if (eui64Json == NULL)
    {
        icLogError(LOG_TAG, "%s: received incomplete event JSON", __FUNCTION__);
    }
    else
    {
        uint64_t eui64 = 0;
        sscanf(eui64Json->valuestring, "%016" PRIx64, &eui64);

        getCallbacks()->deviceCommunicationFailed(getCallbackContext(), eui64);
    }
}

static void handleDeviceAnnounced(cJSON *event)
{
    cJSON *eui64Json = cJSON_GetObjectItem(event, "eui64");
    cJSON *deviceTypeJson = cJSON_GetObjectItem(event, "deviceType");
    cJSON *powerSourceJson = cJSON_GetObjectItem(event, "powerSource");
    if (eui64Json != NULL && deviceTypeJson != NULL && powerSourceJson != NULL)
    {
        uint64_t eui64;
        sscanf(eui64Json->valuestring, "%016" PRIx64, &eui64);

        zhalDeviceType deviceType = deviceTypeUnknown;
        if (strcmp(deviceTypeJson->valuestring, "endDevice") == 0)
        {
            deviceType = deviceTypeEndDevice;
        }
        else if (strcmp(deviceTypeJson->valuestring, "router") == 0)
        {
            deviceType = deviceTypeRouter;
        }

        zhalPowerSource powerSource = powerSourceUnknown;
        if (strcmp(powerSourceJson->valuestring, "mains") == 0)
        {
            powerSource = powerSourceMains;
        }
        else if (strcmp(powerSourceJson->valuestring, "battery") == 0)
        {
            powerSource = powerSourceBattery;
        }

        getCallbacks()->deviceAnnounced(getCallbackContext(), eui64, deviceType, powerSource);
    }
}

static void handleDeviceRejoinedEventReceived(cJSON *event)
{
    cJSON *eui64Json = cJSON_GetObjectItem(event, "eui64");
    cJSON *isSecureJson = cJSON_GetObjectItem(event, "isSecure");
    if (eui64Json != NULL && isSecureJson != NULL)
    {
        uint64_t eui64;
        sscanf(eui64Json->valuestring, "%016" PRIx64, &eui64);

        bool isSecure = false;
        if (cJSON_IsBool(isSecureJson))
        {
            isSecure = (bool) cJSON_IsTrue(isSecureJson);
        }

        getCallbacks()->deviceRejoined(getCallbackContext(), eui64, isSecure);
    }
}

static void handleLinkKeyUpdatedEventReceived(cJSON *event)
{
    cJSON *eui64Json = cJSON_GetObjectItem(event, "eui64");
    cJSON *isUsingHashBasedKeyJson = cJSON_GetObjectItem(event, "isUsingHashBasedKey");
    if (eui64Json != NULL)
    {
        uint64_t eui64;
        sscanf(eui64Json->valuestring, "%016" PRIx64, &eui64);

        bool isUsingHashBasedKey = false;
        if (isUsingHashBasedKeyJson != NULL && cJSON_IsBool(isUsingHashBasedKeyJson) == true)
        {
            isUsingHashBasedKey = (bool) cJSON_IsTrue(isUsingHashBasedKeyJson);
        }

        getCallbacks()->linkKeyUpdated(getCallbackContext(), eui64, isUsingHashBasedKey);
    }
}

static void handleNetworkHealthProblemEventReceived(cJSON *event)
{
    getCallbacks()->networkHealthProblem(getCallbackContext());
}

static void handleNetworkHealthProblemRestoredEventReceived(cJSON *event)
{
    getCallbacks()->networkHealthProblemRestored(getCallbackContext());
}

static void handlePanIdAttackEventReceived(cJSON *event)
{
    getCallbacks()->panIdAttackDetected(getCallbackContext());
}

static void handlePanIdAttackClearedEventReceived(cJSON *event)
{
    getCallbacks()->panIdAttackCleared(getCallbackContext());
}

static void handleBeaconEventReceived(cJSON *event)
{
    cJSON *eui64Json = cJSON_GetObjectItem(event, "eui64");
    cJSON *panIdJson = cJSON_GetObjectItem(event, "panId");
    cJSON *isOpenJson = cJSON_GetObjectItem(event, "isOpen");
    cJSON *hasEndDeviceCapacityJson = cJSON_GetObjectItem(event, "hasEndDeviceCapacity");
    cJSON *hasRouterCapacityJson = cJSON_GetObjectItem(event, "hasRouterCapacity");
    cJSON *depthJson = cJSON_GetObjectItem(event, "depth");

    if (eui64Json == NULL || panIdJson == NULL || isOpenJson == NULL || hasEndDeviceCapacityJson == NULL ||
        hasRouterCapacityJson == NULL || depthJson == NULL)
    {
        icLogError(LOG_TAG, "%s: received incomplete event JSON", __func__);
        return;
    }

    uint64_t eui64 = 0;
    bool conversionSuccess = stringToUnsignedNumberWithinRange(eui64Json->valuestring, &eui64, 16, 0, UINT64_MAX);
    conversionSuccess &= (panIdJson->valueint >= 0 && panIdJson->valueint <= UINT16_MAX);
    conversionSuccess &= (depthJson->valueint >= 0 && depthJson->valueint <= UINT8_MAX);

    if (conversionSuccess)
    {
        getCallbacks()->beaconReceived(getCallbackContext(),
                                       eui64,
                                       panIdJson->valueint,
                                       cJSON_IsTrue(isOpenJson),
                                       cJSON_IsTrue(hasEndDeviceCapacityJson),
                                       cJSON_IsTrue(hasRouterCapacityJson),
                                       depthJson->valueint);
    }
    else
    {
        icLogError(LOG_TAG, "%s: received invalid event JSON", __func__);
    }
}

int zhalHandleEvent(cJSON *event)
{
    void *callbackContext = getCallbackContext();
    char *eventString = cJSON_PrintUnformatted(event);
    icLogDebug(LOG_TAG, "got event: %s", eventString);
    free(eventString);

    cJSON *eventType = cJSON_GetObjectItem(event, "eventType");
    if (eventType == NULL)
    {
        icLogError(LOG_TAG, "Invalid event received (missing eventType)");
        return 0;
    }

    if (strcmp(eventType->valuestring, "zhalStartup") == 0 && getCallbacks()->startup != NULL)
    {
        getCallbacks()->startup(callbackContext);
    }
    else if (strcmp(eventType->valuestring, "networkConfigChanged") == 0 &&
             getCallbacks()->networkConfigChanged != NULL)
    {
        cJSON *data = cJSON_GetObjectItem(event, "networkConfigData");
        if (data != NULL)
        {
            getCallbacks()->networkConfigChanged(callbackContext, data->valuestring);
        }
    }
    else if (strcmp(eventType->valuestring, "deviceAnnounced") == 0 && getCallbacks()->deviceAnnounced != NULL)
    {
        handleDeviceAnnounced(event);
    }
    else if (strcmp(eventType->valuestring, "deviceJoined") == 0 && getCallbacks()->deviceJoined != NULL)
    {
        cJSON *data = cJSON_GetObjectItem(event, "eui64");
        if (data != NULL)
        {
            uint64_t eui64;
            sscanf(data->valuestring, "%016" PRIx64, &eui64);
            getCallbacks()->deviceJoined(callbackContext, eui64);
        }
    }
    else if (strcmp(eventType->valuestring, "deviceRejoined") == 0 && getCallbacks()->deviceRejoined != NULL)
    {
        handleDeviceRejoinedEventReceived(event);
    }
    else if (strcmp(eventType->valuestring, "linkKeyUpdated") == 0 && getCallbacks()->linkKeyUpdated != NULL)
    {
        handleLinkKeyUpdatedEventReceived(event);
    }
    else if (strcmp(eventType->valuestring, "apsAckFailure") == 0 && getCallbacks()->apsAckFailure != NULL)
    {
        cJSON *data = cJSON_GetObjectItem(event, "eui64");
        if (data != NULL)
        {
            uint64_t eui64;
            sscanf(data->valuestring, "%016" PRIx64, &eui64);
            getCallbacks()->apsAckFailure(callbackContext, eui64);
        }
    }
    else if (strcmp(eventType->valuestring, "clusterCommandReceived") == 0 &&
             getCallbacks()->clusterCommandReceived != NULL)
    {
        handleClusterCommandReceived(event);
    }
    else if (strcmp(eventType->valuestring, "attributeReport") == 0 && getCallbacks()->attributeReportReceived != NULL)
    {
        handleAttributeReportReceived(event);
    }
    else if (strcmp(eventType->valuestring, "deviceOtaUpgradeMessageSentEvent") == 0 &&
             getCallbacks()->deviceOtaUpgradeMessageSent != NULL)
    {
        handleOtaUpgradeMessageSentEventReceived(event);
    }
    else if (strcmp(eventType->valuestring, "deviceOtaUpgradeMessageReceivedEvent") == 0 &&
             getCallbacks()->deviceOtaUpgradeMessageReceived != NULL)
    {
        handleOtaUpgradeMessageReceivedEventReceived(event);
    }
    else if (strcmp(eventType->valuestring, "deviceCommunicationSucceededEvent") == 0 &&
             getCallbacks()->deviceCommunicationSucceeded != NULL)
    {
        handleDeviceCommunicationSucceededEventReceived(event);
    }
    else if (strcmp(eventType->valuestring, "deviceCommunicationFailedEvent") == 0 &&
             getCallbacks()->deviceCommunicationFailed != NULL)
    {
        handleDeviceCommunicationFailedEventRecieved(event);
    }
    else if (strcmp(eventType->valuestring, "networkHealthProblem") == 0 &&
             getCallbacks()->networkHealthProblem != NULL)
    {
        handleNetworkHealthProblemEventReceived(event);
    }
    else if (strcmp(eventType->valuestring, "networkHealthProblemRestored") == 0 &&
             getCallbacks()->networkHealthProblemRestored != NULL)
    {
        handleNetworkHealthProblemRestoredEventReceived(event);
    }
    else if (strcmp(eventType->valuestring, "panIdAttack") == 0 && getCallbacks()->panIdAttackDetected != NULL)
    {
        handlePanIdAttackEventReceived(event);
    }
    else if (strcmp(eventType->valuestring, "panIdAttackCleared") == 0 && getCallbacks()->panIdAttackCleared != NULL)
    {
        handlePanIdAttackClearedEventReceived(event);
    }
    else if (getCallbacks()->beaconReceived != NULL &&
             stringCompare(eventType->valuestring, "beaconReceived", false) == 0)
    {
        handleBeaconEventReceived(event);
    }

    // Cleanup since we are saying we handled it
    cJSON_Delete(event);

    return 1; // we handled it
}
