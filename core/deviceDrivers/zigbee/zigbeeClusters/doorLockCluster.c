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

#include <stdlib.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>
#include <icLog/logging.h>
#include <memory.h>
#include <subsystems/zigbee/zigbeeAttributeTypes.h>
#include <subsystems/zigbee/zigbeeSubsystem.h>
#include <stdio.h>
#include <ctype.h>
#include <commonDeviceDefs.h>
#include <subsystems/zigbee/zigbeeIO.h>
#include <sys/param.h>

#ifdef BARTON_CONFIG_ZIGBEE

#include "zigbeeClusters/doorLockCluster.h"

#define LOG_TAG "doorLockCluster"

// Alarm codes
#define BOLT_JAMMED 0x00
#define LOCK_RESET_TO_FACTORY_DEFAULTS 0x01
#define BATTERY_REPLACEMENT 0x02
#define RF_MODULE_POWER_CYCLED 0x03
#define TAMPER_ALARM_WRONG_CODE_ENTRY_LIMIT 0x04
#define TAMPER_ALARM_FRONT_ESCUTCHEON_REMOVED 0x05
#define DOOR_FORCED_OPEN_WHILE_LOCKED 0x06

static bool configureCluster(ZigbeeCluster *ctx, const DeviceConfigurationContext *configContext);

static bool handleClusterCommand(ZigbeeCluster *ctx, ReceivedClusterCommand *command);

static bool handleAttributeReport(ZigbeeCluster *ctx, ReceivedAttributeReport* report);

static bool handleAlarm(ZigbeeCluster *ctx, uint64_t eui64, uint8_t endpointId, const ZigbeeAlarmTableEntry *alarmTableEntry);

typedef struct
{
    ZigbeeCluster cluster;

    const DoorLockClusterCallbacks *callbacks;
    const void *callbackContext;
} DoorLockCluster;

ZigbeeCluster *doorLockClusterCreate(const DoorLockClusterCallbacks *callbacks, void *callbackContext)
{
    DoorLockCluster *result = (DoorLockCluster *) calloc(1, sizeof(DoorLockCluster));

    result->cluster.clusterId = DOORLOCK_CLUSTER_ID;
    result->cluster.configureCluster = configureCluster;
    result->cluster.handleClusterCommand = handleClusterCommand;
    result->cluster.handleAttributeReport = handleAttributeReport;
    result->cluster.handleAlarm = handleAlarm;

    result->callbacks = callbacks;
    result->callbackContext = callbackContext;

    return (ZigbeeCluster *) result;
}

bool doorLockClusterIsLocked(uint64_t eui64, uint8_t endpointId, bool *isLocked)
{
    bool result = false;

    uint64_t val = 0;

    if (zigbeeSubsystemReadNumber(eui64,
                                  endpointId,
                                  DOORLOCK_CLUSTER_ID,
                                  true,
                                  DOORLOCK_LOCK_STATE_ATTRIBUTE_ID,
                                  &val) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to read lock state attribute value", __FUNCTION__);
    }
    else
    {
        *isLocked = val == 0x01; // 0x00 == not fully locked, 0x01 == locked, 0x02 == unlocked
        result = true;
    }

    return result;
}

bool doorLockClusterSetLocked(uint64_t eui64, uint8_t endpointId, bool isLocked)
{
    // the payload is an octet string for the PIN.  The first byte of an octet string is the length, so we use 0
    // since we dont send one.
    uint8_t payload = 0;

    return (zigbeeSubsystemSendCommand(eui64,
                                       endpointId,
                                       DOORLOCK_CLUSTER_ID,
                                       true,
                                       (uint8_t) (isLocked ? DOORLOCK_LOCK_DOOR_COMMAND_ID
                                                           : DOORLOCK_UNLOCK_DOOR_COMMAND_ID),
                                       &payload,
                                       1) == 0);
}

bool doorLockClusterGetInvalidLockoutTimeSecs(uint64_t eui64, uint8_t endpointId, uint8_t* lockoutTimeSecs)
{
    bool result = false;

    uint64_t val = 0;
    if (zigbeeSubsystemReadNumber(eui64,
                                  endpointId,
                                  DOORLOCK_CLUSTER_ID,
                                  true,
                                  DOORLOCK_USER_CODE_TEMPORARY_DISABLE_TIME,
                                  &val) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to read user code temporary disable time attribute value", __FUNCTION__);
    }
    else
    {
        *lockoutTimeSecs = val & 0xff;
        result = true;
    }

    return result;
}

bool doorLockClusterGetMaxPinCodeLength(uint64_t eui64, uint8_t endpointId, uint8_t *length)
{
    bool result = false;

    uint64_t val = 0;
    if (zigbeeSubsystemReadNumber(eui64,
                                  endpointId,
                                  DOORLOCK_CLUSTER_ID,
                                  true,
                                  DOORLOCK_MAX_PIN_CODE_LENGTH_ATTRIBUTE_ID,
                                  &val) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to read max pin code length attribute value", __FUNCTION__);
    }
    else
    {
        *length = val & 0xff;
        result = true;
    }

    return result;
}

bool doorLockClusterGetMinPinCodeLength(uint64_t eui64, uint8_t endpointId, uint8_t *length)
{
    bool result = false;

    uint64_t val = 0;
    if (zigbeeSubsystemReadNumber(eui64,
                                  endpointId,
                                  DOORLOCK_CLUSTER_ID,
                                  true,
                                  DOORLOCK_MIN_PIN_CODE_LENGTH_ATTRIBUTE_ID,
                                  &val) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to read min pin code length attribute value", __FUNCTION__);
    }
    else
    {
        *length = val & 0xff;
        result = true;
    }

    return result;
}

bool doorLockClusterGetMaxPinCodeUsers(uint64_t eui64, uint8_t endpointId, uint16_t *numUsers)
{
    bool result = false;

    uint64_t val = 0;
    if (zigbeeSubsystemReadNumber(eui64,
                                  endpointId,
                                  DOORLOCK_CLUSTER_ID,
                                  true,
                                  DOORLOCK_NUM_PIN_USERS_SUPPORTED_ATTRIBUTE_ID,
                                  &val) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to read num pin users supported attribute value", __FUNCTION__);
    }
    else
    {
        *numUsers = val & 0xffff;
        result = true;
    }

    return result;
}

bool doorLockClusterGetAutoRelockTime(uint64_t eui64, uint8_t endpointId, uint32_t *autoRelockSeconds)
{
    bool result = false;

    uint64_t val = 0;
    if (zigbeeSubsystemReadNumber(eui64,
                                  endpointId,
                                  DOORLOCK_CLUSTER_ID,
                                  true,
                                  DOORLOCK_AUTO_RELOCK_TIME_ATTRIBUTE_ID,
                                  &val) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to read auto relock time attribute value", __FUNCTION__);
    }
    else
    {
        *autoRelockSeconds = val & 0xffffffff;
        result = true;
    }

    return result;
}

bool doorLockClusterSetAutoRelockTime(uint64_t eui64, uint8_t endpointId, uint32_t autoRelockSeconds)
{
    bool result = false;

    if (zigbeeSubsystemWriteNumber(eui64,
                                   endpointId,
                                   DOORLOCK_CLUSTER_ID,
                                   true,
                                   DOORLOCK_AUTO_RELOCK_TIME_ATTRIBUTE_ID,
                                   ZCL_INT32U_ATTRIBUTE_TYPE,
                                   autoRelockSeconds,
                                   sizeof(autoRelockSeconds)) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to write auto relock time attribute value", __FUNCTION__);
    }
    else
    {
        result = true;
    }

    return result;
}

bool doorLockClusterClearAllPinCodes(uint64_t eui64, uint8_t endpointId)
{
    bool result = false;

    if (zigbeeSubsystemSendCommand(eui64,
                                   endpointId,
                                   DOORLOCK_CLUSTER_ID,
                                   true,
                                   DOORLOCK_CLEAR_ALL_PIN_CODES_COMMAND_ID,
                                   NULL,
                                   0) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to send clear all pin codes command", __func__);
    }
    else
    {
        result = true;
    }

    return result;
}

bool doorLockClusterGetPinCode(uint64_t eui64, uint8_t endpointId, uint16_t userId)
{
    bool result = false;

    uint8_t payload[2];
    sbZigbeeIOContext *zio = zigbeeIOInit(payload, 2, ZIO_WRITE);
    zigbeeIOPutUint16(zio, userId);

    if (zigbeeSubsystemSendCommand(eui64,
                                   endpointId,
                                   DOORLOCK_CLUSTER_ID,
                                   true,
                                   DOORLOCK_GET_PIN_CODE_COMMAND_ID,
                                   payload,
                                   2) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to send get pin code command", __func__);
    }
    else
    {
        result = true;
    }

    return result;
}

bool doorLockClusterClearPinCode(uint64_t eui64, uint8_t endpointId, uint16_t userId)
{
    bool result = false;

    uint8_t payload[2];
    sbZigbeeIOContext *zio = zigbeeIOInit(payload, 2, ZIO_WRITE);
    zigbeeIOPutUint16(zio, userId);

    if (zigbeeSubsystemSendCommand(eui64,
                                   endpointId,
                                   DOORLOCK_CLUSTER_ID,
                                   true,
                                   DOORLOCK_CLEAR_PIN_CODE_COMMAND_ID,
                                   payload,
                                   2) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to send clear pin code command", __func__);
    }
    else
    {
        result = true;
    }

    return result;
}

bool doorLockClusterSetPinCode(uint64_t eui64,
                               uint8_t endpointId,
                               const DoorLockClusterUser *user)
{
    bool result = false;

    if (user == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __func__);
        return false;
    }

    // user id + user status + user type + 1 byte length prefixed octet string
    size_t pinLen = strlen(user->pin);
    size_t payloadLen = 4 + pinLen + 1;
    AUTO_CLEAN(free_generic__auto) uint8_t *payload = calloc(payloadLen, 1);
    sbZigbeeIOContext *zio = zigbeeIOInit(payload, payloadLen, ZIO_WRITE);

    zigbeeIOPutUint16(zio, user->userId);
    zigbeeIOPutUint8(zio, user->userStatus);
    zigbeeIOPutUint8(zio, user->userType);
    zigbeeIOPutUint8(zio, pinLen);

    bool validPin = true;
    for (size_t i = 0; i < pinLen; i++)
    {
        if (isdigit(user->pin[i]) == 0)
        {
            icLogError(LOG_TAG, "%s: invalid pin digit 0x%02x", __func__, user->pin[i]);
            validPin = false;
            break;
        }

        zigbeeIOPutUint8(zio, user->pin[i]);
    }

    if (validPin == true)
    {
        if (zigbeeSubsystemSendCommand(eui64,
                                       endpointId,
                                       DOORLOCK_CLUSTER_ID,
                                       true,
                                       DOORLOCK_SET_PIN_CODE_COMMAND_ID,
                                       payload,
                                       payloadLen) != 0)
        {
            icLogError(LOG_TAG, "%s: failed to send set pin code command", __func__);
        }
        else
        {
            result = true;
        }
    }

    return result;
}

static bool configureCluster(ZigbeeCluster *ctx, const DeviceConfigurationContext *configContext)
{
    bool result = true;

    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    zhalAttributeReportingConfig doorLockConfigs[3]; // the first entry is mandatory, but we could have up to two others.
    uint8_t numConfigs = 1;
    memset(&doorLockConfigs[0], 0, sizeof(zhalAttributeReportingConfig));
    doorLockConfigs[0].attributeInfo.id = DOORLOCK_LOCK_STATE_ATTRIBUTE_ID;
    doorLockConfigs[0].attributeInfo.type = ZCL_ENUM8_ATTRIBUTE_TYPE;
    doorLockConfigs[0].minInterval = 1;
    doorLockConfigs[0].maxInterval = 1620; //every 27 minutes at least.  we need this for comm fail, but only 1 attr.
    doorLockConfigs[0].reportableChange = 1;

    if (icDiscoveredDeviceDetailsGetAttributeEndpoint(configContext->discoveredDeviceDetails,
                                                      DOORLOCK_CLUSTER_ID,
                                                      DOORLOCK_AUTO_RELOCK_TIME_ATTRIBUTE_ID,
                                                      NULL) == true)
    {
        doorLockConfigs[1].attributeInfo.id = DOORLOCK_AUTO_RELOCK_TIME_ATTRIBUTE_ID;
        doorLockConfigs[1].attributeInfo.type = ZCL_INT32U_ATTRIBUTE_TYPE;
        doorLockConfigs[1].minInterval = 1;
        doorLockConfigs[1].maxInterval = REPORTING_INTERVAL_MAX;
        doorLockConfigs[1].reportableChange = 1;

        ++numConfigs;
    }

    if (icDiscoveredDeviceDetailsGetAttributeEndpoint(configContext->discoveredDeviceDetails,
                                                      DOORLOCK_CLUSTER_ID,
                                                      DOORLOCK_ENABLE_LOCAL_PROGRAMMING_ATTRIBUTE_ID,
                                                      NULL) == true)
    {
        doorLockConfigs[2].attributeInfo.id = DOORLOCK_ENABLE_LOCAL_PROGRAMMING_ATTRIBUTE_ID;
        doorLockConfigs[2].attributeInfo.type = ZCL_BOOLEAN_ATTRIBUTE_TYPE;
        doorLockConfigs[2].minInterval = 1;
        doorLockConfigs[2].maxInterval = REPORTING_INTERVAL_MAX;
        doorLockConfigs[2].reportableChange = 1;

        ++numConfigs;
    }

    if (zigbeeSubsystemBindingSet(configContext->eui64, configContext->endpointId, DOORLOCK_CLUSTER_ID) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to bind", __FUNCTION__);
        result = false;
    }
    else if (zigbeeSubsystemAttributesSetReporting(configContext->eui64,
                                                   configContext->endpointId,
                                                   DOORLOCK_CLUSTER_ID,
                                                   doorLockConfigs,
                                                   numConfigs) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to set reporting", __FUNCTION__);
        result = false;
    }

    if (icDiscoveredDeviceDetailsGetAttributeEndpoint(configContext->discoveredDeviceDetails,
                                                      DOORLOCK_CLUSTER_ID,
                                                      DOORLOCK_KEYPAD_PROGRAMMING_EVENT_MASK_ATTRIBUTE_ID,
                                                      NULL) == true)
    {
        if (zigbeeSubsystemWriteNumber(configContext->eui64,
                                       configContext->endpointId,
                                       DOORLOCK_CLUSTER_ID,
                                       true,
                                       DOORLOCK_KEYPAD_PROGRAMMING_EVENT_MASK_ATTRIBUTE_ID,
                                       ZCL_BITMAP16_ATTRIBUTE_TYPE,
                                       0xFFFF,
                                       2) != 0)
        {
            icLogError(LOG_TAG, "%s: failed to set keypad programming event mask", __FUNCTION__);
            result = false;
        }
    }

    uint16_t alarmMask = 1 << BOLT_JAMMED |
            1 << LOCK_RESET_TO_FACTORY_DEFAULTS |
            1 << BATTERY_REPLACEMENT |
            1 << RF_MODULE_POWER_CYCLED |
            1 << TAMPER_ALARM_WRONG_CODE_ENTRY_LIMIT |
            1 << TAMPER_ALARM_FRONT_ESCUTCHEON_REMOVED |
            1 << DOOR_FORCED_OPEN_WHILE_LOCKED;
    if (zigbeeSubsystemWriteNumber(configContext->eui64,
                                   configContext->endpointId,
                                   DOORLOCK_CLUSTER_ID,
                                   true,
                                   DOORLOCK_ALARM_MASK_ATTRIBUTE_ID,
                                   ZCL_BITMAP16_ATTRIBUTE_TYPE,
                                   alarmMask,
                                   sizeof(alarmMask)) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to set alarm mask", __FUNCTION__);
        result = false;
    }

    return result;
}

const char *getSourceString(uint8_t source, uint8_t code)
{
    const char *result = NULL;

    // our representation of the source of the event comes from either the source or the code.  If we are using
    // the code handle that first, otherwise fall back to source

    switch (code)
    {
        case 0xa: //auto lock
            result = DOORLOCK_PROFILE_LOCKED_SOURCE_AUTO;
            break;

        case 0xb: //schedule lock
        case 0xc: //schedule unlock
            result = DOORLOCK_PROFILE_LOCKED_SOURCE_SCHEDULE;
            break;

        default:
            //we will use the source
            break;
    }

    if (result == NULL)
    {
        switch (source)
        {
            //TODO when we get to the point where we have all json defined in schemas and structs generated, fixme
            case 0x00:
                result = DOORLOCK_PROFILE_LOCKED_SOURCE_KEYPAD;
                break;

            case 0x01:
                result = DOORLOCK_PROFILE_LOCKED_SOURCE_RF;
                break;

            case 0x02:
                result = DOORLOCK_PROFILE_LOCKED_SOURCE_MANUAL;
                break;

            case 0x03:
                result = DOORLOCK_PROFILE_LOCKED_SOURCE_RFID;
                break;

            default:
                result = DOORLOCK_PROFILE_LOCKED_SOURCE_UNKNOWN;
                break;
        }
    }

    return result;
}

static bool handleOperationEventNotification(DoorLockCluster *cluster, ReceivedClusterCommand *command)
{
    bool handled = false;
    bool isLocked = false;

    // ensure we have minimum sane command payload length
    if (command->commandDataLen < 9)
    {
        return false;
    }

    sbZigbeeIOContext *zio = zigbeeIOInit(command->commandData, command->commandDataLen, ZIO_READ);

    uint8_t eventSource = zigbeeIOGetUint8(zio);
    uint8_t eventCode = zigbeeIOGetUint8(zio);

    const char *source = getSourceString(eventSource, eventCode);

    switch (eventCode)
    {
        case 0x1: //lock
        case 0x7: //one touch lock
        case 0x8: //key lock
        case 0xa: //auto lock
        case 0xb: //schedule lock
        case 0xd: //manual lock
            isLocked = true;
            handled = true;
            break;

        case 0x2: //unlock
        case 0x9: //key unlock
        case 0xc: //schedule unlock
        case 0xe: //manual unlock
            isLocked = false;
            handled = true;
            break;

        default:
            icLogWarn(LOG_TAG, "%s: ignoring operation code %d", __FUNCTION__, command->commandData[1]);
            break;
    }

    if (handled)
    {
        uint16_t userId = zigbeeIOGetUint16(zio);

        if (cluster->callbacks->lockedStateChanged != NULL)
        {
            cluster->callbacks->lockedStateChanged(command->eui64, command->sourceEndpoint, isLocked, source, userId,
                                                   cluster->callbackContext);
        }
    }

    return handled;
}

static char *pinToString(ZigbeeIOContext *zio, size_t pinLength)
{
    char *result = calloc(pinLength + 1, 1);

    bool validPin = true;
    for(int i = 0; i < pinLength; i++)
    {
        uint8_t pinByte = zigbeeIOGetUint8(zio);

        if (isdigit(pinByte) == 0 && pinByte != 0) //0 is allowed for invalid/empty slots
        {
            icLogError(LOG_TAG, "%s: invalid pin digit %"PRIu8, __func__, pinByte);
            validPin = false;
            break;
        }
        else
        {
            result[i] = (char)pinByte;
        }
    }

    if (validPin == false)
    {
        free(result);
        result = NULL;
    }

    return result;
}

static bool handleProgrammingEventNotification(DoorLockCluster *cluster, ReceivedClusterCommand *command)
{
    bool handled = false;

    icLogDebug(LOG_TAG, "%s", __func__);

    // ensure we have minimum sane command payload length
    if (command->commandDataLen < 12)
    {
        return false;
    }

    sbZigbeeIOContext *zio = zigbeeIOInit(command->commandData, command->commandDataLen, ZIO_READ);
    uint8_t programmingEventSource = zigbeeIOGetUint8(zio);

    if (programmingEventSource == 0x00) // we only care about programming at the keypad
    {
        uint8_t programEventCode = zigbeeIOGetUint8(zio);
        uint16_t userId = zigbeeIOGetUint16(zio);
        uint8_t pinLength = zigbeeIOGetUint8(zio);
        AUTO_CLEAN(free_generic__auto) char *pin = pinToString(zio, pinLength);

        if (pin != NULL)
        {
            uint8_t userType = zigbeeIOGetUint8(zio);
            uint8_t userStatus = zigbeeIOGetUint8(zio);
            uint32_t localTime = zigbeeIOGetUint32(zio);
            AUTO_CLEAN(free_generic__auto) char *data = zigbeeIOGetString(zio); //this can be NULL which is ok

            if (cluster->callbacks->keypadProgrammingEventNotification != NULL)
            {
                cluster->callbacks->keypadProgrammingEventNotification(command->eui64,
                                                                       command->sourceEndpoint,
                                                                       programEventCode,
                                                                       userId,
                                                                       pin,
                                                                       userType,
                                                                       userStatus,
                                                                       localTime,
                                                                       data,
                                                                       cluster->callbackContext);
            }
        }

        handled = true;
    }

    return handled;
}

static bool handleSetPinCodeResponse(DoorLockCluster *cluster, ReceivedClusterCommand *command)
{
    // ensure we have minimum sane command payload length
    if (command->commandDataLen != 1)
    {
        return false;
    }

    if (cluster->callbacks->setPinCodeResponse != NULL)
    {
        cluster->callbacks->setPinCodeResponse(command->eui64,
                                               command->sourceEndpoint,
                                               command->commandData[0],
                                               cluster->callbackContext);
    }

    return true;
}

static bool handleGetPinCodeResponse(DoorLockCluster *cluster, ReceivedClusterCommand *command)
{
    bool handled = false;

    // ensure we have minimum sane command payload length
    if (command->commandDataLen < 5)
    {
        return false;
    }

    sbZigbeeIOContext *zio = zigbeeIOInit(command->commandData, command->commandDataLen, ZIO_READ);
    DoorLockClusterUser user;
    memset(&user, 0, sizeof(user));

    user.userId = zigbeeIOGetUint16(zio);
    user.userStatus = zigbeeIOGetUint8(zio);
    user.userType = zigbeeIOGetUint8(zio);

    uint8_t pinLength = zigbeeIOGetUint8(zio);
    AUTO_CLEAN(free_generic__auto) char *pin = pinToString(zio, pinLength);

    if (pin != NULL && cluster->callbacks->getPinCodeResponse != NULL)
    {
        strncpy(user.pin, pin, MIN(pinLength, sizeof(user.pin) - 1));
        cluster->callbacks->getPinCodeResponse(command->eui64,
                                               command->sourceEndpoint,
                                               &user,
                                               cluster->callbackContext);
    }

    return handled;
}

static bool handleClearPinCodeResponse(DoorLockCluster *cluster, ReceivedClusterCommand *command)
{
    // ensure we have minimum sane command payload length
    if (command->commandDataLen != 1)
    {
        return false;
    }

    if (cluster->callbacks->clearPinCodeResponse != NULL)
    {
        cluster->callbacks->clearPinCodeResponse(command->eui64,
                                                 command->sourceEndpoint,
                                                 command->commandData[0] == 0,
                                                 cluster->callbackContext);
    }

    return true;
}

static bool handleClearAllPinCodesResponse(DoorLockCluster *cluster, ReceivedClusterCommand *command)
{
    // ensure we have minimum sane command payload length
    if (command->commandDataLen != 1)
    {
        return false;
    }

    if (cluster->callbacks->clearAllPinCodesResponse != NULL)
    {
        cluster->callbacks->clearAllPinCodesResponse(command->eui64,
                                                     command->sourceEndpoint,
                                                     command->commandData[0] == 0,
                                                     cluster->callbackContext);
    }

    return true;
}

static bool handleClusterCommand(ZigbeeCluster *ctx, ReceivedClusterCommand *command)
{
    bool handled = false;

    icLogDebug(LOG_TAG, "%s: clusterId 0x%04x, commandId 0x%02x", __FUNCTION__, command->clusterId, command->commandId);

    if (command->clusterId != DOORLOCK_CLUSTER_ID ||
         command->mfgSpecific == true ||
         command->fromServer == false)
    {
        return false;
    }

    DoorLockCluster *cluster = (DoorLockCluster *) ctx;

    switch(command->commandId)
    {
        case DOORLOCK_SET_PIN_CODE_RESPONSE_COMMAND_ID:
            handled = handleSetPinCodeResponse(cluster, command);
            break;

        case DOORLOCK_GET_PIN_CODE_RESPONSE_COMMAND_ID:
            handled = handleGetPinCodeResponse(cluster, command);
            break;

        case DOORLOCK_CLEAR_PIN_CODE_RESPONSE_COMMAND_ID:
            handled = handleClearPinCodeResponse(cluster, command);
            break;

        case DOORLOCK_CLEAR_ALL_PIN_CODES_RESPONSE_COMMAND_ID:
            handled = handleClearAllPinCodesResponse(cluster, command);
            break;

        case DOORLOCK_OPERATION_EVENT_NOTIFICATION_COMMAND_ID:
            handled = handleOperationEventNotification(cluster, command);
            break;

        case DOORLOCK_PROGRAMMING_EVENT_NOTIFICATION_COMMAND_ID:
            handled = handleProgrammingEventNotification(cluster, command);
            break;
    }

    return handled;
}

static bool handleAttributeReport(ZigbeeCluster *ctx, ReceivedAttributeReport* report)
{
    bool handled = false;

    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    // there has to be more than the attribute id and type
    if (report->clusterId != DOORLOCK_CLUSTER_ID || report->reportDataLen <= 3)
    {
        icLogError(LOG_TAG, "%s: invalid report data", __FUNCTION__);
        return false;
    }

    DoorLockCluster *cluster = (DoorLockCluster *) ctx;

    sbZigbeeIOContext *zio = zigbeeIOInit(report->reportData, report->reportDataLen, ZIO_READ);

    uint16_t attributeId = zigbeeIOGetUint16(zio);
    uint8_t attributeType = zigbeeIOGetUint8(zio);

    switch (attributeId)
    {
        case DOORLOCK_AUTO_RELOCK_TIME_ATTRIBUTE_ID:
        {
            uint32_t autoRelockTime = zigbeeIOGetUint32(zio);
            if (cluster->callbacks->autoRelockTimeChanged != NULL)
            {
                cluster->callbacks->autoRelockTimeChanged(report->eui64,
                                                          report->sourceEndpoint,
                                                          autoRelockTime,
                                                          cluster->callbackContext);
            }
            break;
        }

        case DOORLOCK_LOCK_STATE_ATTRIBUTE_ID:
        {
            //silently ignore this.  we use it for comm fail prevention.  actual lock state handling
            // is done through the operation event notification
            uint8_t lockState = zigbeeIOGetUint8(zio); //read the byte so zio library doesnt complain
            (void)lockState; // unused
            break;
        }

        default:
            icLogWarn(LOG_TAG, "%s: unexpected attribute id 0x%04x", __func__, attributeId);
            break;
    }

    return handled;
}

static bool handleAlarm(ZigbeeCluster *ctx, uint64_t eui64, uint8_t endpointId, const ZigbeeAlarmTableEntry *alarmTableEntry)
{
    bool result = true;

    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    DoorLockCluster *cluster = (DoorLockCluster *) ctx;

    switch (alarmTableEntry->alarmCode)
    {
        case BOLT_JAMMED: // bolt jammed
            if (cluster->callbacks->jammedStateChanged != NULL)
            {
                cluster->callbacks->jammedStateChanged(eui64, endpointId, true, cluster->callbackContext);
            }
            break;

        case LOCK_RESET_TO_FACTORY_DEFAULTS: // Lock Reset to Factory Defaults
            icLogWarn(LOG_TAG, "%s: Lock reset to factory defaults", __FUNCTION__);
            break;

        case BATTERY_REPLACEMENT: // Battery replacement
            icLogWarn(LOG_TAG, "%s: battery replaced", __FUNCTION__);
            break;

        case RF_MODULE_POWER_CYCLED: // RF Module Power cycled
            icLogWarn(LOG_TAG, "%s: RF module power cycled", __FUNCTION__);
            break;

        case TAMPER_ALARM_WRONG_CODE_ENTRY_LIMIT: // Tamper Alarm - wrong code entry limit
            if (cluster->callbacks->invalidCodeEntryLimitChanged != NULL)
            {
                cluster->callbacks->invalidCodeEntryLimitChanged(eui64, endpointId, true, cluster->callbackContext);
            }
            break;

        case TAMPER_ALARM_FRONT_ESCUTCHEON_REMOVED: // Tamper Alarm - front escutcheon removed from main
            icLogWarn(LOG_TAG, "%s: Escutcheon removed!", __FUNCTION__);
            if (cluster->callbacks->tamperedStateChanged != NULL)
            {
                cluster->callbacks->tamperedStateChanged(eui64, endpointId, true, cluster->callbackContext);
            }
            break;

        case DOOR_FORCED_OPEN_WHILE_LOCKED: // Forced Door Open under Door Locked Condition
            icLogWarn(LOG_TAG, "%s: Door forced open while locked!", __FUNCTION__);
            if (cluster->callbacks->tamperedStateChanged != NULL)
            {
                cluster->callbacks->tamperedStateChanged(eui64, endpointId, true, cluster->callbackContext);
            }
            break;

        default:
            icLogWarn(LOG_TAG,
                      "%s: Unsupported door lock cluster alarm code 0x%02x",
                      __FUNCTION__,
                      alarmTableEntry->alarmCode);
            result = false;
            break;
    }

    return result;
}

#endif //BARTON_CONFIG_ZIGBEE