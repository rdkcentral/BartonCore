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
// Created by tlea on 2/15/19.
//

#include <errno.h>
#include <icLog/logging.h>
#include <stdlib.h>
#include <string.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>
#include <subsystems/zigbee/zigbeeSubsystem.h>
#include <zigbeeClusters/helpers/comcastBatterySavingHelper.h>

#ifdef BARTON_CONFIG_ZIGBEE

#include "icConcurrent/threadUtils.h"
#include "subsystems/zigbee/zigbeeIO.h"
#include "subsystems/zigbee/zigbeeSubsystem.h"
#include "zigbeeClusters/pollControlCluster.h"

#define LOG_TAG                                         "pollControlCluster"

#define POLL_CONTROL_FAST_POLL_STOP_COMMAND_ID          0x01
#define POLL_CONTROL_SET_LONG_POLL_INTERVAL_COMMAND_ID  0x02
#define POLL_CONTROL_SET_SHORT_POLL_INTERVAL_COMMAND_ID 0x03
#define POLL_CONTROL_FAST_POLL_TIMEOUT_ATTRIBUTE_ID     0x0003
#define POLL_CONTROL_CHECKIN_INTERVAL_ATTRIBUTE_ID      0x0000
#define POLL_CONTROL_CHECKIN_RESPONSE_COMMAND_ID        0x00

#define POLL_CONTROL_CLUSTER_DISABLE_BIND_KEY           "pollConClusterDisableBind"

/* 10s */
#define FAST_POLL_TIMEOUT_QS                            10 * 4

#define CHECKIN_INTERVAL_QS                             ZIGBEE_DEFAULT_CHECKIN_INTERVAL_S * 4

/* 5 min */
#define LONG_POLL_INTERVAL_QS                           5 * 60 * 4

static bool configureCluster(ZigbeeCluster *ctx, const DeviceConfigurationContext *configContext);

static bool handleClusterCommand(ZigbeeCluster *ctx, ReceivedClusterCommand *command);

static void destroyCluster(const ZigbeeCluster *ctx);

typedef struct
{
    const PollControlClusterCallbacks *callbacks;
    const void *callbackContext;
} CallbackInfo;

typedef struct
{
    ZigbeeCluster cluster;

    icLinkedList *callbackList;
} PollControlCluster;

static CallbackInfo *createCallbackInfo(const PollControlClusterCallbacks *callbacks, const void *callbackContext)
{
    if (callbacks == NULL)
    {
        return NULL;
    }

    CallbackInfo *cbInfo = calloc(1, sizeof(CallbackInfo));
    cbInfo->callbacks = callbacks;
    cbInfo->callbackContext = callbackContext;

    return cbInfo;
}

ZigbeeCluster *pollControlClusterCreate(const PollControlClusterCallbacks *callbacks, const void *callbackContext)
{
    PollControlCluster *result = (PollControlCluster *) calloc(1, sizeof(PollControlCluster));

    result->cluster.clusterId = POLL_CONTROL_CLUSTER_ID;
    result->cluster.configureCluster = configureCluster;
    result->cluster.handleClusterCommand = handleClusterCommand;

    result->callbackList = linkedListCreate();

    pollControlClusterAddCallback((ZigbeeCluster *) result, callbacks, callbackContext);

    result->cluster.destroy = destroyCluster;

    return (ZigbeeCluster *) result;
}

bool pollControlClusterAddCallback(ZigbeeCluster *baseCluster,
                                   const PollControlClusterCallbacks *callback,
                                   const void *callbackContext)
{
    if (baseCluster == NULL || callback == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return false;
    }

    PollControlCluster *cluster = (PollControlCluster *) baseCluster;

    CallbackInfo *obj = createCallbackInfo(callback, callbackContext);
    linkedListAppend(cluster->callbackList, obj);

    return true;
}

void pollControlClusterSetBindingEnabled(const DeviceConfigurationContext *deviceConfigurationContext, bool bind)
{
    addBoolConfigurationMetadata(
        deviceConfigurationContext->configurationMetadata, POLL_CONTROL_CLUSTER_DISABLE_BIND_KEY, bind);
}

bool pollControlClusterSendCustomCheckInResponse(uint64_t eui64, uint8_t endpointId)
{
    uint8_t fastPollMsg[3];

    fastPollMsg[0] = 0; // bool: Start fast Polling - 0 off / 1 on

    // 2 bytes of fast poll timeout quarter seconds. If zero, it will use whatever we configured for the fast poll
    // timeout
    fastPollMsg[1] = 0;
    fastPollMsg[2] = 0;

    // Send check-in response command with fast poll set to off
    return zigbeeSubsystemSendCommand(eui64,
                                      endpointId,
                                      POLL_CONTROL_CLUSTER_ID,
                                      true,
                                      POLL_CONTROL_CHECKIN_RESPONSE_COMMAND_ID,
                                      fastPollMsg,
                                      sizeof(fastPollMsg)) == 0;
}

bool pollControlClusterSendCheckInResponse(uint64_t eui64, uint8_t endpointId, bool startFastPoll)
{
    // send checkInResponse of 'startFastPolling' with a timeout of 10*4 quarter seconds
    uint8_t fastPollMsg[3];

    fastPollMsg[0] = (startFastPoll == true ? 1 : 0); // bool to start fast polling

    // 2 bytes of fast poll timeout quarter seconds. If zero, it will use whatever we configured for the fast poll
    //  timeout
    fastPollMsg[1] = 0;
    fastPollMsg[2] = 0;

    return zigbeeSubsystemSendCommand(eui64,
                                      endpointId,
                                      POLL_CONTROL_CLUSTER_ID,
                                      true,
                                      POLL_CONTROL_CHECKIN_RESPONSE_COMMAND_ID,
                                      fastPollMsg,
                                      sizeof(fastPollMsg)) == 0;
}

bool pollControlClusterStopFastPoll(uint64_t eui64, uint8_t endpointId)
{
    return zigbeeSubsystemSendCommand(
               eui64, endpointId, POLL_CONTROL_CLUSTER_ID, true, POLL_CONTROL_FAST_POLL_STOP_COMMAND_ID, NULL, 0) == 0;
}

bool pollControlClusterSetLongPollInterval(uint64_t eui64, uint8_t endpointId, uint32_t newIntervalQs)
{
    bool result;

    // long and short poll intervals are set with a command instead of a write attribute
    uint8_t longPollPayload[4];
    ZigbeeIOContext *zio = NULL;

    errno = 0;
    zio = zigbeeIOInit(longPollPayload, sizeof(longPollPayload), ZIO_WRITE);
    zigbeeIOPutUint32(zio, newIntervalQs);
    zigbeeIODestroy(zio);

    result = zigbeeSubsystemSendCommand(eui64,
                                        endpointId,
                                        POLL_CONTROL_CLUSTER_ID,
                                        true,
                                        POLL_CONTROL_SET_LONG_POLL_INTERVAL_COMMAND_ID,
                                        longPollPayload,
                                        4) == 0;

    return result;
}

static bool setShortPollInterval(uint64_t eui64, uint8_t endpointId, const DeviceConfigurationContext *configContext)
{
    bool result = true;

    // metadata in the device descriptor takes priority over what the device driver may provide.
    //  If no metadata is found in either the device descriptor or from the device driver, dont configure it.
    const char *shortPollMetadata = NULL;

    if (configContext->deviceDescriptor != NULL)
    {
        shortPollMetadata =
            stringHashMapGet(configContext->deviceDescriptor->metadata, SHORT_POLL_INTERVAL_QS_METADATA);
    }

    if (shortPollMetadata == NULL)
    {
        shortPollMetadata = stringHashMapGet(configContext->configurationMetadata, SHORT_POLL_INTERVAL_QS_METADATA);
    }

    if (shortPollMetadata != NULL)
    {
        icLogDebug(LOG_TAG, "%s: using short poll metadata %s", __FUNCTION__, shortPollMetadata);

        uint8_t shortPollPayload[2];
        char *bad = NULL;
        ZigbeeIOContext *zio = NULL;

        errno = 0;
        unsigned long shortPollInterval = strtoul(shortPollMetadata, &bad, 10);

        // 0 is invalid
        if (errno || *bad || shortPollInterval == 0 || shortPollInterval > UINT16_MAX)
        {
            icLogWarn(LOG_TAG, "%s: invalid short poll interval", __FUNCTION__);
            result = false;
        }
        else
        {
            // long and short poll intervals are set with a command instead of a write attribute
            errno = 0;
            zio = zigbeeIOInit(shortPollPayload, sizeof(shortPollPayload), ZIO_WRITE);
            zigbeeIOPutUint16(zio, (uint16_t) shortPollInterval);
            zigbeeIODestroy(zio);

            if (errno != 0 || zigbeeSubsystemSendCommand(eui64,
                                                         endpointId,
                                                         POLL_CONTROL_CLUSTER_ID,
                                                         true,
                                                         POLL_CONTROL_SET_SHORT_POLL_INTERVAL_COMMAND_ID,
                                                         shortPollPayload,
                                                         2) != 0)
            {
                icLogError(LOG_TAG, "%s: failed to set short poll interval", __FUNCTION__);
                result = false;
            }
        }
    }

    return result;
}

static bool setLongPollInterval(uint64_t eui64, uint8_t endpointId, const DeviceConfigurationContext *configContext)
{
    bool result = true;

    // metadata in the device descriptor takes priority over what the device driver may provide.
    //  If no metadata is found in either the device descriptor or from the device driver, dont configure it.
    const char *longPollMetadata = NULL;

    if (configContext->deviceDescriptor != NULL)
    {
        longPollMetadata = stringHashMapGet(configContext->deviceDescriptor->metadata, LONG_POLL_INTERVAL_QS_METADATA);
    }

    if (longPollMetadata == NULL)
    {
        longPollMetadata = stringHashMapGet(configContext->configurationMetadata, LONG_POLL_INTERVAL_QS_METADATA);
    }

    if (longPollMetadata != NULL)
    {
        icLogDebug(LOG_TAG, "%s: using long poll metadata %s", __FUNCTION__, longPollMetadata);

        char *bad = NULL;

        errno = 0;
        unsigned long longPollInterval = strtoul(longPollMetadata, &bad, 10);

        // valid range is 0x4 to 0x6e0000
        if (errno || *bad || longPollInterval < 0x4 || longPollInterval > 0x6e0000)
        {
            icLogWarn(LOG_TAG, "%s: invalid long poll interval", __FUNCTION__);
            result = false;
        }
        else
        {
            result = pollControlClusterSetLongPollInterval(eui64, endpointId, (uint32_t) longPollInterval);
        }
    }

    return result;
}

static bool setFastPollTimeout(uint64_t eui64, uint8_t endpointId, const DeviceConfigurationContext *configContext)
{
    bool result = true;

    // metadata in the device descriptor takes priority over what the device driver may provide.
    //  If no metadata is found in either the device descriptor or from the device driver, dont configure it.
    const char *fastPollTimeoutMetadata = NULL;

    if (configContext->deviceDescriptor != NULL)
    {
        fastPollTimeoutMetadata =
            stringHashMapGet(configContext->deviceDescriptor->metadata, FAST_POLL_TIMEOUT_QS_METADATA);
    }

    if (fastPollTimeoutMetadata == NULL)
    {
        fastPollTimeoutMetadata = stringHashMapGet(configContext->configurationMetadata, FAST_POLL_TIMEOUT_QS_METADATA);
    }

    if (fastPollTimeoutMetadata != NULL)
    {
        icLogDebug(LOG_TAG, "%s: using fast poll timeout metadata %s", __FUNCTION__, fastPollTimeoutMetadata);

        char *bad = NULL;

        errno = 0;
        unsigned long timeout = strtoul(fastPollTimeoutMetadata, &bad, 10);

        // valid range is 0x1 to 0xffff
        if (errno || *bad || timeout == 0 || timeout > UINT16_MAX)
        {
            icLogWarn(LOG_TAG, "%s: invalid fast poll timeout", __FUNCTION__);
            result = false;
        }
        else
        {
            if (zigbeeSubsystemWriteNumber(eui64,
                                           endpointId,
                                           POLL_CONTROL_CLUSTER_ID,
                                           true,
                                           POLL_CONTROL_FAST_POLL_TIMEOUT_ATTRIBUTE_ID,
                                           ZCL_INT16U_ATTRIBUTE_TYPE,
                                           timeout,
                                           2) != 0)
            {
                icLogError(LOG_TAG, "%s: failed to set fast poll timeout", __FUNCTION__);
                result = false;
            }
        }
    }

    return result;
}

static bool setCheckinInterval(uint64_t eui64, uint8_t endpointId, const DeviceConfigurationContext *configContext)
{
    bool result = true;

    // metadata in the device descriptor takes priority over what the device driver may provide.
    //  If no metadata is found in either the device descriptor or from the device driver, dont configure it.
    const char *checkinIntervalMetadata = NULL;

    if (configContext->deviceDescriptor != NULL)
    {
        checkinIntervalMetadata =
            stringHashMapGet(configContext->deviceDescriptor->metadata, CHECK_IN_INTERVAL_QS_METADATA);
    }

    if (checkinIntervalMetadata == NULL)
    {
        checkinIntervalMetadata = stringHashMapGet(configContext->configurationMetadata, CHECK_IN_INTERVAL_QS_METADATA);
    }

    if (checkinIntervalMetadata != NULL)
    {
        icLogDebug(LOG_TAG, "%s: using checkin interval metadata %s", __FUNCTION__, checkinIntervalMetadata);

        char *bad = NULL;

        errno = 0;
        unsigned long interval = strtoul(checkinIntervalMetadata, &bad, 10);

        // valid range is 0x0 to 0x6e0000
        if (errno || *bad || interval > 0x6e0000)
        {
            icLogWarn(LOG_TAG, "%s: invalid checkin interval", __FUNCTION__);
            result = false;
        }
        else
        {
            if (zigbeeSubsystemWriteNumber(eui64,
                                           endpointId,
                                           POLL_CONTROL_CLUSTER_ID,
                                           true,
                                           POLL_CONTROL_CHECKIN_INTERVAL_ATTRIBUTE_ID,
                                           ZCL_INT32U_ATTRIBUTE_TYPE,
                                           interval,
                                           4) != 0)
            {
                icLogError(LOG_TAG, "%s: failed to set checkin interval", __FUNCTION__);
                result = false;
            }
        }
    }

    return result;
}

static bool configureCluster(ZigbeeCluster *ctx, const DeviceConfigurationContext *configContext)
{
    bool result = true;
    uint64_t eui64 = configContext->eui64;
    uint8_t endpointId = configContext->endpointId;

    icLogDebug(LOG_TAG, "%s: eui64=%016" PRIx64, __FUNCTION__, eui64);

    // If the property is set to false we skip, otherwise accept its value or the default of true if nothing was set
    if (getBoolConfigurationMetadata(configContext->configurationMetadata, POLL_CONTROL_CLUSTER_DISABLE_BIND_KEY, true))
    {
        if (zigbeeSubsystemBindingSet(eui64, endpointId, POLL_CONTROL_CLUSTER_ID) != 0)
        {
            icLogError(LOG_TAG, "%s: failed to bind poll control cluster", __FUNCTION__);
            result = false;
        }
    }

    if (result && setShortPollInterval(eui64, endpointId, configContext) == false)
    {
        result = false;
    }

    if (result && setFastPollTimeout(eui64, endpointId, configContext) == false)
    {
        result = false;
    }

    if (result && setCheckinInterval(eui64, endpointId, configContext) == false)
    {
        result = false;
    }

    if (result && setLongPollInterval(eui64, endpointId, configContext) == false)
    {
        result = false;
    }

    return result;
}

static bool handleClusterCommand(ZigbeeCluster *ctx, ReceivedClusterCommand *command)
{
    bool result = false;

    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    PollControlCluster *pollControlCluster = (PollControlCluster *) ctx;

    switch (command->commandId)
    {
        case POLL_CONTROL_CHECKIN_COMMAND_ID:
        {
            ComcastBatterySavingData *batterySavingData = NULL;

            // if this is a comcast enhanced mfg specific checkin message, parse and handle its payload
            if (command->mfgSpecific && command->mfgCode == COMCAST_MFG_ID)
            {
                batterySavingData = comcastBatterySavingDataParse(command->commandData, command->commandDataLen);
            }

            scoped_icLinkedListIterator *iter = linkedListIteratorCreate(pollControlCluster->callbackList);
            while (linkedListIteratorHasNext(iter))
            {
                CallbackInfo *callbackItem = linkedListIteratorGetNext(iter);

                if (callbackItem && callbackItem->callbacks && callbackItem->callbacks->checkin)
                {
                    callbackItem->callbacks->checkin(
                        command->eui64, command->sourceEndpoint, batterySavingData, callbackItem->callbackContext);
                }
            }

            free(batterySavingData);
            result = true;
            break;
        }

        default:
            icLogError(LOG_TAG, "%s: unexpected command id 0x%02x", __FUNCTION__, command->commandId);
            break;
    }

    return result;
}

static void destroyCluster(const ZigbeeCluster *ctx)
{
    PollControlCluster *cluster = (PollControlCluster *) ctx;
    if (cluster)
    {
        linkedListDestroy(cluster->callbackList, NULL);
    }
}

#endif // BARTON_CONFIG_ZIGBEE