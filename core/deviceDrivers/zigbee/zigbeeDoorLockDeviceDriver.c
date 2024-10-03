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
// Created by tlea on 2/20/19.
//

#include "device-driver/device-driver-manager.h"
#include "deviceDrivers/zigbeeDriverCommon.h"
#include <commonDeviceDefs.h>
#include <ctype.h>
#include <device/deviceModelHelper.h>
#include <deviceServicePrivate.h>
#include <icConcurrent/delayedTask.h>
#include <icConcurrent/threadUtils.h>
#include <icConcurrent/timedWait.h>
#include <icLog/logging.h>
#include <icTime/timeUtils.h>
#include <icUtil/stringUtils.h>
#include <jsonHelper/jsonHelper.h>
#include <memory.h>
#include <pthread.h>
#include <resourceTypes.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>
#include <zigbeeClusters/doorLockCluster.h>
#include <zigbeeClusters/powerConfigurationCluster.h>

#define LOG_TAG                                       "zigbeeDoorLockDD"
#define DRIVER_NAME                                   "zigbeeDoorLock"
#define DEVICE_CLASS_NAME                             "doorLock"
#define DOOR_LOCK_PROGRAM_PIN_CODES_DELAY_MS_METADATA "doorLockProgramPinCodesDelayMs"

#define MY_DC_VERSION                                 2
#define MY_DOORLOCK_PROFILE_VERSION                   2

#define DEFAULT_LOCKOUT_TIME                          60
#define CLEAR_ALL_PIN_CODES_TIMEOUT_SECS              5
#define SET_PIN_CODE_TIMEOUT_SECS                     5

#ifdef BARTON_CONFIG_ZIGBEE

typedef struct
{
    char *uuid;
    char *endpointName;
} RestoreLockoutArg;

static uint16_t myDeviceIds[] = {DOORLOCK_DEVICE_ID};

static const char *mapDeviceIdToProfile(ZigbeeDriverCommon *ctx, uint16_t deviceId);

static bool fetchInitialResourceValues(ZigbeeDriverCommon *ctx,
                                       icDevice *device,
                                       IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                                       icInitialResourceValues *values);

static bool registerResources(ZigbeeDriverCommon *ctx,
                              icDevice *device,
                              IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                              icInitialResourceValues *values);

static bool
preConfigureCluster(ZigbeeDriverCommon *ctx, ZigbeeCluster *cluster, DeviceConfigurationContext *deviceConfigContext);

static bool writeEndpointResource(ZigbeeDriverCommon *ctx,
                                  uint32_t endpointNumber,
                                  icDeviceResource *resource,
                                  const char *previousValue,
                                  const char *newValue,
                                  bool *baseDriverUpdatesResource);

static void synchronizeDevice(ZigbeeDriverCommon *ctx, icDevice *device, IcDiscoveredDeviceDetails *details);

static bool deviceNeedsReconfiguring(ZigbeeDriverCommon *ctx, icDevice *device, IcDiscoveredDeviceDetails *details);

static void lockedStateChanged(uint64_t eui64,
                               uint8_t endpointId,
                               bool isLocked,
                               const char *source,
                               uint16_t userId,
                               const void *ctx);

static void jammedStateChanged(uint64_t eui64, uint8_t endpointId, bool isJammed, const void *ctx);

static void tamperedStateChanged(uint64_t eui64, uint8_t endpointId, bool isTampered, const void *ctx);

static void invalidCodeEntryLimitChanged(uint64_t eui64, uint8_t endpointId, bool limitExceeded, const void *ctx);

static void clearAllPinCodesResponse(uint64_t eui64, uint8_t endpointId, bool success, const void *ctx);

static void setPinCodeResponse(uint64_t eui64, uint8_t endpointId, uint8_t result, const void *ctx);

static void keypadProgrammingEventNotification(uint64_t eui64,
                                               uint8_t endpointId,
                                               uint8_t programmingEventCode,
                                               uint16_t userId,
                                               const char *pin,
                                               uint8_t userType,
                                               uint8_t userStatus,
                                               uint32_t localTime,
                                               const char *data,
                                               const void *ctx);

static void autoRelockTimeChanged(uint64_t eui64, uint8_t endpointId, uint32_t autoRelockSeconds, const void *ctx);

static void freeRestoreLockoutArg(RestoreLockoutArg *arg);

static void restoreLockoutCallback(void *arg);

static void cancelLockoutExpiryTask(int taskHandle);

static void preStartup(ZigbeeDriverCommon *ctx, uint32_t *commFailTimeoutSeconds);

static void postShutdown(ZigbeeDriverCommon *ctx);

static const ZigbeeDriverCommonCallbacks commonCallbacks = {
    .preStartup = preStartup,
    .postShutdown = postShutdown,
    .fetchInitialResourceValues = fetchInitialResourceValues,
    .registerResources = registerResources,
    .preConfigureCluster = preConfigureCluster,
    .mapDeviceIdToProfile = mapDeviceIdToProfile,
    .writeEndpointResource = writeEndpointResource,
    .synchronizeDevice = synchronizeDevice,
};

static const DoorLockClusterCallbacks doorLockClusterCallbacks = {
    .lockedStateChanged = lockedStateChanged,
    .jammedStateChanged = jammedStateChanged,
    .tamperedStateChanged = tamperedStateChanged,
    .invalidCodeEntryLimitChanged = invalidCodeEntryLimitChanged,
    .autoRelockTimeChanged = autoRelockTimeChanged,
    .clearAllPinCodesResponse = clearAllPinCodesResponse,
    .setPinCodeResponse = setPinCodeResponse,
    .keypadProgrammingEventNotification = keypadProgrammingEventNotification,
};

// Map of eui64 to delayed task handle
static icHashMap *lockoutExpiryTasks = NULL;
static pthread_mutex_t MTX = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;

typedef enum
{
    doorLockResponseTypeUnknown,
    doorLockResponseTypeClearAllPins,
    doorLockResponseTypeClearPin,
    doorLockResponseTypeSetPin,
    doorLockResponseTypeGetPin,
} DoorLockResponseType;

typedef struct
{
    pthread_cond_t asyncCond;
    pthread_mutex_t asyncMtx;
    DoorLockResponseType responseType;
    bool success;
    DoorLockClusterUser user;
} DoorLockRequestSynchronizer;

// Map of eui64 to DoorLockRequestSynchronizer
static icHashMap *requestSynchronizers = NULL;
static pthread_mutex_t requestSynchronizersMtx = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
static DoorLockRequestSynchronizer *getRequestSynchronizer(uint64_t eui64);

typedef struct
{
    pthread_cond_t cond;
    pthread_mutex_t mtx;
    uint64_t eui64;
    uint8_t endpointId;
    icLinkedList *users;
    bool result;
    bool complete;
} ProgramPinCodesOnLockArgs;

// zigbee device driver registration order matters, so we pick constructor priority carefully
__attribute__((constructor(300))) static void driverRegister(void)
{
    DeviceDriver *myDriver = zigbeeDriverCommonCreateDeviceDriver(DRIVER_NAME,
                                                                  DEVICE_CLASS_NAME,
                                                                  MY_DC_VERSION,
                                                                  myDeviceIds,
                                                                  sizeof(myDeviceIds) / sizeof(uint16_t),
                                                                  RX_MODE_PSEUDO_SLEEPY,
                                                                  &commonCallbacks,
                                                                  false);

    DRIVER_REGISTER_PROFILE_VERSION(myDriver, DOORLOCK_PROFILE, MY_DOORLOCK_PROFILE_VERSION);

    zigbeeDriverCommonAddCluster(myDriver, doorLockClusterCreate(&doorLockClusterCallbacks, myDriver));

    // enable periodic collection of common diagnostics info (rssi, lqi, etc).
    zigbeeDriverCommonSetDiagnosticsCollectionEnabled(myDriver, true);

    // enable pair time reading of battery voltage alarm thresholds
    zigbeeDriverCommonRegisterBatteryThresholdResource(myDriver, true);

    deviceDriverManagerRegisterDriver(myDriver);
}

static void preStartup(ZigbeeDriverCommon *ctx, uint32_t *commFailTimeoutSeconds)
{
    mutexLock(&requestSynchronizersMtx);
    requestSynchronizers = hashMapCreate();
    mutexUnlock(&requestSynchronizersMtx);
}

static void requestSynchronizersDestroyFunc(void *key, void *value)
{
    free(key);

    if (value != NULL)
    {
        DoorLockRequestSynchronizer *synchr = (DoorLockRequestSynchronizer *) value;
        pthread_cond_destroy(&synchr->asyncCond);
        pthread_mutex_destroy(&synchr->asyncMtx);
        free(synchr);
    }
}

static void postShutdown(ZigbeeDriverCommon *ctx)
{
    mutexLock(&requestSynchronizersMtx);
    hashMapDestroy(requestSynchronizers, requestSynchronizersDestroyFunc);
    requestSynchronizers = NULL;
    mutexUnlock(&requestSynchronizersMtx);
}

static bool fetchInitialResourceValues(ZigbeeDriverCommon *ctx,
                                       icDevice *device,
                                       IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                                       icInitialResourceValues *values)
{
    bool result = true;

    icLogDebug(LOG_TAG, "%s: uuid=%s", __FUNCTION__, device->uuid);

    // get the eui64 for the device, which is the uuid
    uint64_t eui64 = zigbeeSubsystemIdToEui64(device->uuid);

    for (uint8_t i = 0; i < discoveredDeviceDetails->numEndpoints; i++)
    {
        uint8_t endpointId = discoveredDeviceDetails->endpointDetails[i].endpointId;

        AUTO_CLEAN(free_generic__auto) char *epName = stringBuilder("%" PRIu8, endpointId);

        bool isLocked = false;
        if (doorLockClusterIsLocked(eui64, endpointId, &isLocked) == false)
        {
            icLogError(LOG_TAG, "%s: unable to determine initial isLocked state", __FUNCTION__);
            result = false;
            break;
        }

        initialResourceValuesPutEndpointValue(
            values, epName, DOORLOCK_PROFILE_RESOURCE_LOCKED, stringValueOfBool(isLocked));

        // optional attribute may not exist
        uint8_t maxPinCodeLength = 0;
        if (doorLockClusterGetMaxPinCodeLength(eui64, endpointId, &maxPinCodeLength) == true)
        {
            AUTO_CLEAN(free_generic__auto) char *temp = stringBuilder("%" PRIu8, maxPinCodeLength);
            initialResourceValuesPutEndpointValue(values, epName, DOORLOCK_PROFILE_RESOURCE_MAX_PIN_CODE_LENGTH, temp);
        }

        // optional attribute may not exist
        uint8_t minPinCodeLength = 0;
        if (doorLockClusterGetMinPinCodeLength(eui64, endpointId, &minPinCodeLength) == true)
        {
            AUTO_CLEAN(free_generic__auto) char *temp = stringBuilder("%" PRIu8, minPinCodeLength);
            initialResourceValuesPutEndpointValue(values, epName, DOORLOCK_PROFILE_RESOURCE_MIN_PIN_CODE_LENGTH, temp);
        }

        // optional attribute may not exist
        uint16_t maxPinCodeUsers = 0;
        if (doorLockClusterGetMaxPinCodeUsers(eui64, endpointId, &maxPinCodeUsers) == true)
        {
            // Subtract 1 because some locks seem to reserve one of their slots for the master pin
            AUTO_CLEAN(free_generic__auto) char *temp = stringBuilder("%" PRIu16, maxPinCodeUsers - 1);
            initialResourceValuesPutEndpointValue(values, epName, DOORLOCK_PROFILE_RESOURCE_MAX_PIN_CODES, temp);
        }

        // optional attribute may not exist
        uint32_t autoRelockSeconds = 0;
        if (doorLockClusterGetAutoRelockTime(eui64, endpointId, &autoRelockSeconds) == true)
        {
            AUTO_CLEAN(free_generic__auto) char *temp = stringBuilder("%" PRIu32, autoRelockSeconds);
            initialResourceValuesPutEndpointValue(values, epName, DOORLOCK_PROFILE_RESOURCE_AUTOLOCK_SECS, temp);
        }

        // These we can't get the initial state now
        initialResourceValuesPutEndpointValue(values, epName, DOORLOCK_PROFILE_RESOURCE_JAMMED, "false");
        initialResourceValuesPutEndpointValue(values, epName, DOORLOCK_PROFILE_RESOURCE_TAMPERED, "false");
        initialResourceValuesPutEndpointValue(
            values, epName, DOORLOCK_PROFILE_RESOURCE_INVALID_CODE_ENTRY_LIMIT, "false");
        initialResourceValuesPutEndpointValue(values, epName, COMMON_DEVICE_RESOURCE_LAST_USER_INTERACTION_DATE, NULL);
    }

    return result;
}

static bool registerResources(ZigbeeDriverCommon *ctx,
                              icDevice *device,
                              IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                              icInitialResourceValues *values)
{
    bool result = true;

    icLogDebug(LOG_TAG, "%s: uuid=%s", __FUNCTION__, device->uuid);

    for (uint8_t i = 0; i < discoveredDeviceDetails->numEndpoints; i++)
    {
        uint8_t endpointId = discoveredDeviceDetails->endpointDetails[i].endpointId;

        AUTO_CLEAN(free_generic__auto) char *epName = stringBuilder("%" PRIu8, endpointId);

        icDeviceEndpoint *endpoint = createEndpoint(device, epName, DOORLOCK_PROFILE, true);
        endpoint->profileVersion = MY_DOORLOCK_PROFILE_VERSION;

        result &= createEndpointResourceIfAvailable(endpoint,
                                                    DOORLOCK_PROFILE_RESOURCE_LOCKED,
                                                    values,
                                                    RESOURCE_TYPE_BOOLEAN,
                                                    RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_DYNAMIC |
                                                        RESOURCE_MODE_EMIT_EVENTS,
                                                    CACHING_POLICY_ALWAYS) != NULL;

        result &= createEndpointResourceIfAvailable(endpoint,
                                                    DOORLOCK_PROFILE_RESOURCE_JAMMED,
                                                    values,
                                                    RESOURCE_TYPE_BOOLEAN,
                                                    RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC |
                                                        RESOURCE_MODE_EMIT_EVENTS,
                                                    CACHING_POLICY_ALWAYS) != NULL;

        result &= createEndpointResourceIfAvailable(endpoint,
                                                    DOORLOCK_PROFILE_RESOURCE_TAMPERED,
                                                    values,
                                                    RESOURCE_TYPE_BOOLEAN,
                                                    RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC |
                                                        RESOURCE_MODE_EMIT_EVENTS,
                                                    CACHING_POLICY_ALWAYS) != NULL;

        result &= createEndpointResourceIfAvailable(endpoint,
                                                    DOORLOCK_PROFILE_RESOURCE_INVALID_CODE_ENTRY_LIMIT,
                                                    values,
                                                    RESOURCE_TYPE_BOOLEAN,
                                                    RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC |
                                                        RESOURCE_MODE_EMIT_EVENTS,
                                                    CACHING_POLICY_ALWAYS) != NULL;

        result &= createEndpointResource(endpoint,
                                         DOORLOCK_PROFILE_RESOURCE_PIN_CODES,
                                         NULL,
                                         RESOURCE_TYPE_DOORLOCK_PIN_CODES,
                                         RESOURCE_MODE_WRITEABLE,
                                         CACHING_POLICY_NEVER) != NULL;

        result &= createEndpointResource(endpoint,
                                         DOORLOCK_PROFILE_RESOURCE_LAST_PROGRAMMING_EVENT,
                                         NULL,
                                         RESOURCE_TYPE_DOORLOCK_PROGRAMMING_EVENT,
                                         RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_EMIT_EVENTS,
                                         CACHING_POLICY_ALWAYS) != NULL;

        // optional resources that dont cause pairing failure
        createEndpointResourceIfAvailable(endpoint,
                                          DOORLOCK_PROFILE_RESOURCE_AUTOLOCK_SECS,
                                          values,
                                          RESOURCE_TYPE_SECONDS,
                                          RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_DYNAMIC |
                                              RESOURCE_MODE_EMIT_EVENTS,
                                          CACHING_POLICY_ALWAYS);

        createEndpointResourceIfAvailable(endpoint,
                                          DOORLOCK_PROFILE_RESOURCE_MAX_PIN_CODE_LENGTH,
                                          values,
                                          RESOURCE_TYPE_INTEGER,
                                          RESOURCE_MODE_READABLE,
                                          CACHING_POLICY_ALWAYS);

        createEndpointResourceIfAvailable(endpoint,
                                          DOORLOCK_PROFILE_RESOURCE_MIN_PIN_CODE_LENGTH,
                                          values,
                                          RESOURCE_TYPE_INTEGER,
                                          RESOURCE_MODE_READABLE,
                                          CACHING_POLICY_ALWAYS);

        createEndpointResourceIfAvailable(endpoint,
                                          DOORLOCK_PROFILE_RESOURCE_MAX_PIN_CODES,
                                          values,
                                          RESOURCE_TYPE_INTEGER,
                                          RESOURCE_MODE_READABLE,
                                          CACHING_POLICY_ALWAYS);

        zigbeeDriverCommonSetEndpointNumber(endpoint, endpointId);
    }

    return result;
}

static bool
preConfigureCluster(ZigbeeDriverCommon *ctx, ZigbeeCluster *cluster, DeviceConfigurationContext *deviceConfigContext)
{
    if (ctx == NULL || cluster == NULL || deviceConfigContext == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return false;
    }

    icLogDebug(LOG_TAG,
               "%s: cluster 0x%.4" PRIx16 " endpoint %" PRIu8,
               __FUNCTION__,
               cluster->clusterId,
               deviceConfigContext->endpointId);

    if (cluster->clusterId == POLL_CONTROL_CLUSTER_ID)
    {
        return false;
    }
    else if (cluster->clusterId == POWER_CONFIGURATION_CLUSTER_ID)
    {
        // setting reporting interval to 18 hours
        // the maximum value at which we can set reporting is 0xFFFE which is approximately 18 hours
        //
        powerConfigurationClusterSetConfigureBatteryVoltageMaxInterval(deviceConfigContext, REPORTING_INTERVAL_MAX);
        powerConfigurationClusterSetConfigureBatteryVoltage(deviceConfigContext, true);
        powerConfigurationClusterSetConfigureBatteryPercentage(deviceConfigContext, true);
    }
    return true;
}

static DoorLockClusterUser *parsePinUser(const cJSON *userJson)
{
    if (cJSON_IsObject(userJson) == false)
    {
        icLogError(LOG_TAG, "%s: invalid user JSON", __func__);
        return NULL;
    }

    cJSON *userIdJson = cJSON_GetObjectItem(userJson, DOORLOCK_PROFILE_USER_ID);
    cJSON *pinJson = cJSON_GetObjectItem(userJson, DOORLOCK_PROFILE_USER_PIN);

    if (userIdJson == NULL || pinJson == NULL)
    {
        AUTO_CLEAN(free_generic__auto) char *tmp = cJSON_PrintUnformatted(userJson);
        icLogError(LOG_TAG, "%s: invalid pin code JSON", __func__);
        return NULL;
    }

    DoorLockClusterUser user;
    memset(&user, 0, sizeof(user));

    user.userId = userIdJson->valueint;
    user.userType = 0;   // we only support 'unrestricted' user type
    user.userStatus = 1; // we only support active slot, enabled

    // walk the chars in the pin and confirm that they are numbers
    char *pinPtr = pinJson->valuestring;
    int pinPos = 0;
    while (*pinPtr != '\0')
    {
        if (pinPos + 1 >= sizeof(user.pin))
        {
            // there are more characters than we can handle
            icLogError(LOG_TAG, "%s: pin code too long", __func__);
            return NULL;
        }

        char pinChar = *pinPtr;
        if (isdigit(pinChar) == 0)
        {
            icLogError(LOG_TAG, "%s: invalid pin character: %c", __func__, pinChar);
            return NULL;
        }

        user.pin[pinPos++] = (uint8_t) pinChar;
        pinPtr++;
    }

    // we made it here, so everything checked out.  Allocate our result and copy over what we parsed
    DoorLockClusterUser *result = calloc(1, sizeof(DoorLockClusterUser));
    memcpy(result, &user, sizeof(user));
    return result;
}

static void pinMapKeyFreeFunc(void *key, void *value)
{
    // free the key but not the value
    free(key);
    (void) value; // unused
}

static icLinkedList *parsePinCodes(const char *pinCodes)
{
    icHashMap *pinMap = NULL; // prevent duplicate pin codes with a hash map
    icHashMap *idMap = NULL;  // prevent duplicate ids with a hash map

    AUTO_CLEAN(cJSON_Delete__auto) cJSON *json = cJSON_Parse(pinCodes);
    if (json != NULL && cJSON_IsArray(json) == true)
    {
        pinMap = hashMapCreate();
        idMap = hashMapCreate();

        const cJSON *element = NULL;
        cJSON_ArrayForEach(element, json)
        {
            bool parseError = false;
            AUTO_CLEAN(free_generic__auto) DoorLockClusterUser *user = parsePinUser(element);

            // if one user fails to parse, abort the whole thing and clean up
            if (user == NULL)
            {
                icLogError(LOG_TAG, "%s: user failed to parse", __func__);
                parseError = true;
            }
            else if (hashMapPutCopy(pinMap, user->pin, strlen(user->pin), user, sizeof(DoorLockClusterUser)) == false)
            {
                // if this pin code has already been used, abort the whole thing and clean up
                icLogError(LOG_TAG, "%s: duplicate pin code provided", __func__);
                parseError = true;
            }
            else if (hashMapPutCopy(idMap, &user->userId, sizeof(user->userId), user, sizeof(DoorLockClusterUser)) ==
                     false)
            {
                // if this id has already been used, abort the whole thing and clean up
                icLogError(LOG_TAG, "%s: duplicate user id provided", __func__);
                parseError = true;
            }

            if (parseError == true)
            {
                // free both keys and values
                hashMapDestroy(pinMap, NULL);
                hashMapDestroy(idMap, NULL);
                pinMap = NULL;
                idMap = NULL;
                break;
            }
        }
    }
    else
    {
        icLogError(LOG_TAG, "%s: invalid pinCodes JSON", __func__);
    }

    // if we have a pinMap then we are good.  Build a linked list to return to caller
    icLinkedList *result = NULL;
    if (pinMap != NULL)
    {
        result = linkedListCreate();

        sbIcHashMapIterator *it = hashMapIteratorCreate(pinMap);
        while (hashMapIteratorHasNext(it) == true)
        {
            char *pin = NULL;
            uint16_t pinLen = 0;
            DoorLockClusterUser *user = NULL;

            // remove the entry from the map, freeing only the key
            hashMapIteratorGetNext(it, (void **) &pin, &pinLen, (void **) &user);
            hashMapIteratorDeleteCurrent(it, pinMapKeyFreeFunc);

            // put it in the result list
            linkedListAppend(result, user);
        }
    }

    // free both keys and values of anything that might be left.
    hashMapDestroy(pinMap, NULL);
    hashMapDestroy(idMap, NULL);

    return result;
}

static void programPinCodesOnLock(void *args)
{
    icLogDebug(LOG_TAG, "%s", __func__);

    ProgramPinCodesOnLockArgs *myArgs = (ProgramPinCodesOnLockArgs *) args;

    // FIXME: why is this section so large?
    pthread_mutex_lock(&myArgs->mtx);

    // assume success
    myArgs->result = true;

    DoorLockRequestSynchronizer *syncr = getRequestSynchronizer(myArgs->eui64);
    if (syncr != NULL)
    {
        // now iterate over our entries setting the pins.  Continue on error since we wiped everything already.
        sbIcLinkedListIterator *it = linkedListIteratorCreate(myArgs->users);
        while (linkedListIteratorHasNext(it) == true)
        {
            DoorLockClusterUser *user = linkedListIteratorGetNext(it);

            mutexLock(&syncr->asyncMtx);
            syncr->success = false; // the async callback will set this to true if it is actually successful
            bool setResult = doorLockClusterSetPinCode(myArgs->eui64, myArgs->endpointId, user);
            if (setResult == true)
            {
                // wait for response
                if (incrementalCondTimedWait(&syncr->asyncCond, &syncr->asyncMtx, SET_PIN_CODE_TIMEOUT_SECS) != 0 ||
                    syncr->success == false || syncr->responseType != doorLockResponseTypeSetPin)
                {
                    icLogError(LOG_TAG, "%s: failed to set pin code for user id %" PRIu16, __func__, user->userId);
                    myArgs->result = false; // set the overall result to failure
                }
            }
            else
            {
                icLogError(LOG_TAG, "%s: failed to send set pin code for user id %" PRIu16, __func__, user->userId);
                myArgs->result = false; // set the overall result to failure
            }

            mutexUnlock(&syncr->asyncMtx);
        }

        // indicate we have finished processing
        myArgs->complete = true;
    }
    else
    {
        icLogError(LOG_TAG, "%s: failed to synchronize pin codes", __func__);
        myArgs->result = false; // set the overall result to failure
    }

    pthread_cond_signal(&myArgs->cond);
    pthread_mutex_unlock(&myArgs->mtx);
}

/*
 * Get the number of milliseconds we should delay before starting to program the pin codes on a lock.  This works
 * around a defect on some door locks like the Yale YRD256 deadbolt (see XHFW-629).
 */
static uint64_t getSetPinCodeDelayMillis(uint64_t eui64)
{
    uint64_t delayMillis = 0;

    AUTO_CLEAN(free_generic__auto) char *uuid = zigbeeSubsystemEui64ToId(eui64);
    AUTO_CLEAN(deviceDestroy__auto) icDevice *device = deviceServiceGetDevice(uuid);
    if (device != NULL)
    {
        const char *metadata = deviceGetMetadata(device, DOOR_LOCK_PROGRAM_PIN_CODES_DELAY_MS_METADATA);
        if (metadata != NULL)
        {
            if (stringToUint64(metadata, &delayMillis) == true)
            {
                icLogDebug(
                    LOG_TAG, "%s: using pin code programming delay of %" PRIu64 " milliseconds", __func__, delayMillis);
            }
            else
            {
                icLogWarn(LOG_TAG, "%s: failed to parse pin code programming delay metadata '%s'", __func__, metadata);
            }
        }
    }

    return delayMillis;
}

/*
 * pinCodes is a JSON array of pin code definitions.  For example:
 [
   {
      "id": 1,
      "pin":"1234"
   },
   {
      "id": 2,
      "pin":"5678"
   }
]
 * id is a unique user id per lock (from 0 up to DOORLOCK_PROFILE_RESOURCE_MAX_PIN_CODES)
 * pin is a unique code per lock
 *
 * The strategy here is to clear all pin codes first, then proceed setting each provided pin on the lock.
 *
 * Each of these requests to the lock are asynchronous and we must wait for the corresponding response command.  If
 * we dont get a response within some reasonable timeout, we fail it.
 *
 * NOTE: an empty array will clear all codes.
 */
static bool setPinCodes(uint64_t eui64, uint8_t endpointId, const char *pinCodes)
{
    bool result = false;

    icLogDebug(LOG_TAG, "%s", __func__);

    // first ensure the pin codes that have been provided are all valid and parse correctly
    scoped_icLinkedListGeneric *users = parsePinCodes(pinCodes);

    if (users != NULL)
    {
        // ok we have a valid list.  now get our request synchronizer (will create one if required)
        DoorLockRequestSynchronizer *syncr = getRequestSynchronizer(eui64);

        if (syncr != NULL)
        {
            // next clear all pin codes
            mutexLock(&syncr->asyncMtx);
            syncr->success = false;
            result = doorLockClusterClearAllPinCodes(eui64, endpointId);
            if (result == true)
            {
                // wait for the response
                if (incrementalCondTimedWait(&syncr->asyncCond, &syncr->asyncMtx, CLEAR_ALL_PIN_CODES_TIMEOUT_SECS) !=
                        0 ||
                    syncr->success == false || syncr->responseType != doorLockResponseTypeClearAllPins)
                {
                    icLogError(LOG_TAG, "%s: failed to clear all pin codes", __func__);
                    result = false;
                }
            }
            mutexUnlock(&syncr->asyncMtx);

            if (result == true)
            {
                // some door locks have a bug whereby the 'clear all pin codes response' comes early, before the lock is
                // actually done clearing codes, which causes 'set pin code request' to fail.  Here we insert an
                // optional delay, based on metadata loaded by the device descriptor.

                ProgramPinCodesOnLockArgs args;
                args.eui64 = eui64;
                args.endpointId = endpointId;
                args.users = users;
                args.result = false;
                args.complete = false;
                initTimedWaitCond(&args.cond);
                mutexInitWithType(&args.mtx, PTHREAD_MUTEX_ERRORCHECK);

                uint64_t delayMillis = getSetPinCodeDelayMillis(eui64);

                pthread_mutex_lock(&args.mtx);
                scheduleDelayTask(delayMillis, DELAY_MILLIS, programPinCodesOnLock, &args);

                // coverity[loop_condition] args.complete is set by another thread
                while (args.complete == false)
                {
                    pthread_cond_wait(&args.cond, &args.mtx);
                }
                pthread_mutex_unlock(&args.mtx);
            }
        }
        else
        {
            // This should only happen if we are shutting down when trying to process the request
            icLogError(LOG_TAG, "%s: failed to synchronize door lock pin codes, couldn't synchronize", __func__);
        }
    }
    else
    {
        icLogError(LOG_TAG, "%s: invalid pin codes JSON: '%s'", __func__, stringCoalesce(pinCodes));
    }

    return result;
}

static bool writeEndpointResource(ZigbeeDriverCommon *ctx,
                                  uint32_t endpointNumber,
                                  icDeviceResource *resource,
                                  const char *previousValue,
                                  const char *newValue,
                                  bool *baseDriverUpdatesResource)
{
    bool result = false;

    (void) baseDriverUpdatesResource; // unused

    if (resource == NULL || endpointNumber == 0 || newValue == NULL)
    {
        icLogDebug(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return false;
    }

    icLogDebug(LOG_TAG, "%s: endpoint %s: id=%s", __FUNCTION__, resource->endpointId, resource->id);

    uint64_t eui64 = zigbeeSubsystemIdToEui64(resource->deviceUuid);

    if (stringCompare(DOORLOCK_PROFILE_RESOURCE_LOCKED, resource->id, false) == 0)
    {
        result = doorLockClusterSetLocked(eui64, (uint8_t) endpointNumber, stringToBool(newValue));

        // we dont want the write resource operation to update the locked state... that happens when we get the
        // operation
        //  event notification command
        *baseDriverUpdatesResource = false;
    }
    else if (stringCompare(DOORLOCK_PROFILE_RESOURCE_AUTOLOCK_SECS, resource->id, false) == 0)
    {
        uint32_t autoRelockSeconds = 0;
        if (stringToUint32(newValue, &autoRelockSeconds) == true)
        {
            result = doorLockClusterSetAutoRelockTime(eui64, (uint8_t) endpointNumber, autoRelockSeconds);
        }
        else
        {
            icLogError(LOG_TAG, "%s: invalid value '%s' for %s", __func__, newValue, resource->id);
        }
    }
    else if (stringCompare(DOORLOCK_PROFILE_RESOURCE_PIN_CODES, resource->id, false) == 0)
    {
        result = setPinCodes(eui64, (uint8_t) endpointNumber, newValue);
        *baseDriverUpdatesResource = false; // we do not want the written value persisted
    }

    return result;
}

static void setLockBoltJammed(const char *uuid, const char *epName, bool isJammed)
{
    icLogDebug(LOG_TAG, "%s: %s", __FUNCTION__, stringValueOfBool(isJammed));

    updateResource(uuid, epName, DOORLOCK_PROFILE_RESOURCE_JAMMED, stringValueOfBool(isJammed), NULL);
}

static void setTampered(const char *uuid, const char *epName, bool isTampered)
{
    icLogDebug(LOG_TAG, "%s: %s", __FUNCTION__, stringValueOfBool(isTampered));

    updateResource(uuid, epName, DOORLOCK_PROFILE_RESOURCE_TAMPERED, stringValueOfBool(isTampered), NULL);
}

static void setInvalidCodeEntryLimit(const char *uuid, const char *epName, bool isAtLimit)
{
    icLogDebug(LOG_TAG, "%s: %s", __FUNCTION__, stringValueOfBool(isAtLimit));

    updateResource(
        uuid, epName, DOORLOCK_PROFILE_RESOURCE_INVALID_CODE_ENTRY_LIMIT, stringValueOfBool(isAtLimit), NULL);
}

static void lockedStateChanged(uint64_t eui64,
                               uint8_t endpointId,
                               bool isLocked,
                               const char *source,
                               uint16_t userId,
                               const void *ctx)
{
    AUTO_CLEAN(free_generic__auto) char *uuid = zigbeeSubsystemEui64ToId(eui64);
    AUTO_CLEAN(free_generic__auto) char *epName = stringBuilder("%" PRIu8, endpointId);

    AUTO_CLEAN(cJSON_Delete__auto) cJSON *sourceJson = cJSON_CreateObject();
    cJSON_AddStringToObject(sourceJson, DOORLOCK_PROFILE_LOCKED_SOURCE, source);
    cJSON_AddNumberToObject(sourceJson, DOORLOCK_PROFILE_LOCKED_USERID, userId);

    updateResource(uuid, epName, DOORLOCK_PROFILE_RESOURCE_LOCKED, stringValueOfBool(isLocked), sourceJson);

    // TODO this code should be in common driver... its copied all over the place
    AUTO_CLEAN(free_generic__auto) const char *dateStr = stringBuilder("%" PRIu64, getCurrentUnixTimeMillis());
    updateResource(uuid, NULL, COMMON_DEVICE_RESOURCE_LAST_USER_INTERACTION_DATE, dateStr, NULL);

    // if the lock was tampered, let this state change clear it
    tamperedStateChanged(eui64, endpointId, false, ctx);

    // we clear jammed state upon any manual lock/unlock.
    if (stringCompare(source, DOORLOCK_PROFILE_LOCKED_SOURCE_MANUAL, false) == 0)
    {
        jammedStateChanged(eui64, endpointId, false, ctx);
    }
}

static void jammedStateChanged(uint64_t eui64, uint8_t endpointId, bool isJammed, const void *ctx)
{
    AUTO_CLEAN(free_generic__auto) char *uuid = zigbeeSubsystemEui64ToId(eui64);
    AUTO_CLEAN(free_generic__auto) char *epName = stringBuilder("%" PRIu8, endpointId);
    setLockBoltJammed(uuid, epName, isJammed);
}

static void tamperedStateChanged(uint64_t eui64, uint8_t endpointId, bool isTampered, const void *ctx)
{
    AUTO_CLEAN(free_generic__auto) char *uuid = zigbeeSubsystemEui64ToId(eui64);
    AUTO_CLEAN(free_generic__auto) char *epName = stringBuilder("%" PRIu8, endpointId);
    setTampered(uuid, epName, isTampered);
}

static void freeRestoreLockoutArg(RestoreLockoutArg *arg)
{
    free(arg->uuid);
    free(arg->endpointName);
    free(arg);
}

/**
 * Called by delay task when we set the lockout back to false
 * @param arg RestoreLockoutArg type with details on the device/endpoint to restore
 */
static void restoreLockoutCallback(void *arg)
{
    RestoreLockoutArg *restoreLockoutArg = (RestoreLockoutArg *) arg;
    setInvalidCodeEntryLimit(restoreLockoutArg->uuid, restoreLockoutArg->endpointName, false);
    // Clean up our map entry
    mutexLock(&MTX);
    if (lockoutExpiryTasks != NULL)
    {
        hashMapDelete(lockoutExpiryTasks, restoreLockoutArg->uuid, strlen(restoreLockoutArg->uuid), NULL);
    }
    mutexUnlock(&MTX);
    freeRestoreLockoutArg(restoreLockoutArg);
}

/**
 * Handles cancelling any delayed task with the given task handle
 * @param taskHandle the task handle
 */
static void cancelLockoutExpiryTask(int taskHandle)
{
    RestoreLockoutArg *arg = (RestoreLockoutArg *) cancelDelayTask(taskHandle);
    if (arg != NULL)
    {
        freeRestoreLockoutArg(arg);
    }
}

static void invalidCodeEntryLimitChanged(uint64_t eui64, uint8_t endpointId, bool limitExceeded, const void *ctx)
{
    AUTO_CLEAN(free_generic__auto) char *uuid = zigbeeSubsystemEui64ToId(eui64);
    AUTO_CLEAN(free_generic__auto) char *epName = stringBuilder("%" PRIu8, endpointId);

    setInvalidCodeEntryLimit(uuid, epName, limitExceeded);

    if (limitExceeded)
    {
        // We just expire the lockout after a period of time
        uint8_t lockoutTimeSecs = DEFAULT_LOCKOUT_TIME;
        if (doorLockClusterGetInvalidLockoutTimeSecs(eui64, endpointId, &lockoutTimeSecs) == false)
        {
            icLogDebug(LOG_TAG, "Failed to get lockout time, defaulting to %d", DEFAULT_LOCKOUT_TIME);
        }
        icLogInfo(LOG_TAG, "Door lock %s lockout time is %d secs", uuid, lockoutTimeSecs);

        // Schedule a task to clear it, since we don't get a clear event
        mutexLock(&MTX);
        if (lockoutExpiryTasks == NULL)
        {
            lockoutExpiryTasks = hashMapCreate();
        }
        RestoreLockoutArg *restoreLockoutArg = (RestoreLockoutArg *) calloc(1, sizeof(RestoreLockoutArg));
        restoreLockoutArg->uuid = strdup(uuid);
        restoreLockoutArg->endpointName = strdup(epName);
        // Check if we have a pending task, just to be safe
        int *delayedTaskHandle = hashMapGet(lockoutExpiryTasks, uuid, strlen(uuid));
        int taskToCancel = -1;
        if (delayedTaskHandle == NULL)
        {
            delayedTaskHandle = (int *) malloc(sizeof(int));
        }
        else
        {
            taskToCancel = *delayedTaskHandle;
            hashMapDelete(lockoutExpiryTasks, uuid, strlen(uuid), NULL);
        }
        *delayedTaskHandle = scheduleDelayTask(lockoutTimeSecs, DELAY_SECS, restoreLockoutCallback, restoreLockoutArg);

        hashMapPut(lockoutExpiryTasks, strdup(uuid), strlen(uuid), delayedTaskHandle);
        mutexUnlock(&MTX);

        // Cancel outside the lock for safety
        if (taskToCancel != -1)
        {
            cancelLockoutExpiryTask(taskToCancel);
        }
    }
}

static bool handleAsyncResponse(uint64_t eui64,
                                uint8_t endpointId,
                                bool success,
                                const DoorLockClusterUser *user,
                                DoorLockResponseType responseType)
{
    bool result = false;

    icLogDebug(LOG_TAG, "%s", __func__);
    mutexLock(&requestSynchronizersMtx);
    DoorLockRequestSynchronizer *syncr = hashMapGet(requestSynchronizers, &eui64, sizeof(eui64));
    mutexUnlock(&requestSynchronizersMtx);

    if (syncr != NULL)
    {
        mutexLock(&syncr->asyncMtx);
        syncr->responseType = responseType;
        syncr->success = success;
        if (user != NULL)
        {
            memcpy(&syncr->user, user, sizeof(syncr->user));
        }
        else
        {
            memset(&syncr->user, 0, sizeof(syncr->user));
        }
        pthread_cond_broadcast(&syncr->asyncCond);
        mutexUnlock(&syncr->asyncMtx);

        result = true;
    }

    return result;
}

static void clearAllPinCodesResponse(uint64_t eui64, uint8_t endpointId, bool success, const void *ctx)
{
    icLogDebug(LOG_TAG, "%s: %" PRIx64 " success=%s", __func__, eui64, stringValueOfBool(success));

    if (handleAsyncResponse(eui64, endpointId, success, NULL, doorLockResponseTypeClearAllPins) == false)
    {
        icLogWarn(LOG_TAG, "%s: unexpected clear all pin codes response received", __func__);
    }
}

static void setPinCodeResponse(uint64_t eui64, uint8_t endpointId, uint8_t result, const void *ctx)
{
    icLogDebug(LOG_TAG, "%s: %" PRIx64 " result=%" PRIu8, __func__, eui64, result);

    if (handleAsyncResponse(eui64, endpointId, result == 0, NULL, doorLockResponseTypeSetPin) == false)
    {
        icLogWarn(LOG_TAG, "%s: unexpected set pin code response received", __func__);
    }
}

static void keypadProgrammingEventNotification(uint64_t eui64,
                                               uint8_t endpointId,
                                               uint8_t programmingEventCode,
                                               uint16_t userId,
                                               const char *pin,
                                               uint8_t userType,
                                               uint8_t userStatus,
                                               uint32_t localTime,
                                               const char *data,
                                               const void *ctx)
{
    // we do not want programming to occur on the lock itself.  When this occurs, we just save off some details
    // about it in a resource that could be used to trigger a trouble or whatever

    icLogDebug(LOG_TAG, "%s: event=%" PRIu8 ", userId=%" PRIu16, __func__, programmingEventCode, userId);

    AUTO_CLEAN(free_generic__auto) char *uuid = zigbeeSubsystemEui64ToId(eui64);
    AUTO_CLEAN(free_generic__auto) char *epName = stringBuilder("%" PRIu8, endpointId);

    AUTO_CLEAN(cJSON_Delete__auto) cJSON *event = cJSON_CreateObject();

    (void) pin; // not saved due to security

    const char *eventCode = NULL;
    switch (programmingEventCode)
    {
        case 1:
            eventCode = "MasterCodeChanged";
            break;

        case 2:
            eventCode = "PINCodeAdded";
            break;

        case 3:
            eventCode = "PINCodeDeleted";
            break;

        case 4:
            eventCode = "PINCodeChanged";
            break;

        case 5:
            eventCode = "RFIDCodeAdded";
            break;

        case 6:
            eventCode = "RFIDCodeDeleted";
            break;

        default:
            eventCode = "UnknownOrMfgSpecific";
            break;
    }

    cJSON_AddStringToObject(event, "event", eventCode);

    if (userId != 0xFFFF)
    {
        cJSON_AddNumberToObject(event, "userId", userId);
    }

    if (userType != 0xFF)
    {
        cJSON_AddNumberToObject(event, "userType", userType);
    }

    if (userStatus != 0xFF)
    {
        cJSON_AddNumberToObject(event, "userStatus", userStatus);
    }

    cJSON_AddNumberToObject(event, "localTime", localTime);

    if (data != NULL)
    {
        cJSON_AddStringToObject(event, "data", data);
    }

    AUTO_CLEAN(free_generic__auto) char *eventStr = cJSON_Print(event);
    updateResource(uuid, epName, DOORLOCK_PROFILE_RESOURCE_LAST_PROGRAMMING_EVENT, eventStr, NULL);
}

static void autoRelockTimeChanged(uint64_t eui64, uint8_t endpointId, uint32_t autoRelockSeconds, const void *ctx)
{
    icLogDebug(LOG_TAG, "%s: autoRelockSeconds=%" PRIu32, __func__, autoRelockSeconds);

    AUTO_CLEAN(free_generic__auto) char *uuid = zigbeeSubsystemEui64ToId(eui64);
    AUTO_CLEAN(free_generic__auto) char *epName = stringBuilder("%" PRIu8, endpointId);

    AUTO_CLEAN(free_generic__auto) char *str = stringBuilder("%" PRIu32, autoRelockSeconds);

    updateResource(uuid, epName, DOORLOCK_PROFILE_RESOURCE_AUTOLOCK_SECS, str, NULL);
}

static DoorLockRequestSynchronizer *getRequestSynchronizer(uint64_t eui64)
{
    mutexLock(&requestSynchronizersMtx);
    DoorLockRequestSynchronizer *syncr = hashMapGet(requestSynchronizers, &eui64, sizeof(eui64));
    if (syncr == NULL)
    {
        syncr = calloc(1, sizeof(*syncr));
        initTimedWaitCond(&syncr->asyncCond);
        mutexInitWithType(&syncr->asyncMtx, PTHREAD_MUTEX_ERRORCHECK);
        uint64_t *eui64Copy = malloc(sizeof(uint64_t));
        *eui64Copy = eui64;
        if (hashMapPut(requestSynchronizers, eui64Copy, sizeof(uint64_t), syncr) == false)
        {
            requestSynchronizersDestroyFunc(eui64Copy, syncr);
            eui64Copy = NULL;
            syncr = NULL;
        }
    }
    mutexUnlock(&requestSynchronizersMtx);

    return syncr;
}

static void synchronizeDevice(ZigbeeDriverCommon *ctx, icDevice *device, IcDiscoveredDeviceDetails *details)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    uint64_t eui64 = zigbeeSubsystemIdToEui64(device->uuid);

    sbIcLinkedListIterator *it = linkedListIteratorCreate(device->endpoints);
    while (linkedListIteratorHasNext(it) && deviceServiceIsShuttingDown() == false)
    {
        icDeviceEndpoint *endpoint = (icDeviceEndpoint *) linkedListIteratorGetNext(it);
        uint8_t endpointNumber = zigbeeDriverCommonGetEndpointNumber(ctx, endpoint);

        if (icDiscoveredDeviceDetailsEndpointHasCluster(details, endpointNumber, DOORLOCK_CLUSTER_ID, true))
        {
            bool isLocked;
            if (doorLockClusterIsLocked(eui64, endpointNumber, &isLocked))
            {
                updateResource(
                    device->uuid, endpoint->id, DOORLOCK_PROFILE_RESOURCE_LOCKED, stringValueOfBool(isLocked), NULL);
            }
        }
    }
}

static bool findDeviceResource(void *searchVal, void *item)
{
    icDeviceResource *resourceItem = (icDeviceResource *) item;
    return strcmp(searchVal, resourceItem->id) == 0;
}

static const char *mapDeviceIdToProfile(ZigbeeDriverCommon *ctx, uint16_t deviceId)
{
    uint8_t numDeviceIds = sizeof(myDeviceIds) / sizeof(uint16_t);

    for (int i = 0; i < numDeviceIds; ++i)
    {
        if (myDeviceIds[i] == deviceId)
        {
            return DOORLOCK_PROFILE;
        }
    }

    return NULL;
}

#endif // BARTON_CONFIG_ZIGBEE
