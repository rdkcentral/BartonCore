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


#include <stdlib.h>
#include "subsystems/zigbee/zigbeeCommonIds.h"
#include <icLog/logging.h>
#include <subsystems/zigbee/zigbeeSubsystem.h>
#include <memory.h>
#include <subsystems/zigbee/zigbeeIO.h>
#include <pthread.h>
#include <icConcurrent/threadUtils.h>
#include <icConcurrent/timedWait.h>

#ifdef BARTON_CONFIG_ZIGBEE

#include "zigbeeClusters/alarmsCluster.h"

#define LOG_TAG "alarmsCluster"

#define ALARMS_CLUSTER_DISABLE_BIND_KEY "alarmsClusterDisableBind"

#define ALARMS_GET_TIMEOUT_SECS 15

static bool configureCluster(ZigbeeCluster *ctx, const DeviceConfigurationContext *configContext);

static bool handleClusterCommand(ZigbeeCluster *ctx, ReceivedClusterCommand *command);

static void destroyCluster(const ZigbeeCluster *ctx);

static void tempAlarmTableContextFreeFunc(void *key, void *value);

// one of these is created and stored in the tempAlarmTableContexts map for each device that is actively retrieving
// the alarm table
typedef struct
{
    icLinkedList *tempAlarmTable; //used while fetching the alarm table.  Holds ZigbeeAlarmTableEntrys
    pthread_mutex_t tempAlarmTableMtx;
    pthread_cond_t tempAlarmTableCond;
} TempAlarmTableContext;

typedef struct
{
    ZigbeeCluster cluster;

    const AlarmsClusterCallbacks *callbacks;
    icHashMap *tempAlarmTableContexts; //maps eui64 to TempAlarmTableContext
    pthread_mutex_t tempAlarmTableContextMtx;

    const void *callbackContext;
} AlarmsCluster;

ZigbeeCluster *alarmsClusterCreate(const AlarmsClusterCallbacks *callbacks, const void *callbackContext)
{
    AlarmsCluster *result = (AlarmsCluster *) calloc(1, sizeof(AlarmsCluster));

    result->cluster.clusterId = ALARMS_CLUSTER_ID;

    result->cluster.configureCluster = configureCluster;
    result->cluster.handleClusterCommand = handleClusterCommand;
    result->cluster.destroy = destroyCluster;
    // Want to configure first, so that we bind and get any alarms that happen right after we
    // configure any alarm masks
    result->cluster.priority = CLUSTER_PRIORITY_HIGHEST;

    pthread_mutex_init(&result->tempAlarmTableContextMtx, NULL);
    result->tempAlarmTableContexts = hashMapCreate();

    result->callbacks = callbacks;
    result->callbackContext = callbackContext;

    return (ZigbeeCluster *) result;
}

static void destroyCluster(const ZigbeeCluster *ctx)
{
    AlarmsCluster *alarmsCluster = (AlarmsCluster*)ctx;
    if (alarmsCluster != NULL)
    {
        hashMapDestroy(alarmsCluster->tempAlarmTableContexts, tempAlarmTableContextFreeFunc);
        pthread_mutex_destroy(&alarmsCluster->tempAlarmTableContextMtx);
    }
}

static void tempAlarmTableContextFreeFunc(void *key, void *value)
{
    free(key);

    TempAlarmTableContext  *ctx = (TempAlarmTableContext*)value;
    pthread_mutex_destroy(&ctx->tempAlarmTableMtx);
    pthread_cond_destroy(&ctx->tempAlarmTableCond);
    linkedListDestroy(ctx->tempAlarmTable, NULL);

    free(ctx);
}

void alarmsClusterSetBindingEnabled(const DeviceConfigurationContext *deviceConfigurationContext, bool bind)
{
    addBoolConfigurationMetadata(deviceConfigurationContext->configurationMetadata,
                                 ALARMS_CLUSTER_DISABLE_BIND_KEY,
                                 bind);
}

icLinkedList *alarmsClusterGetAlarms(const ZigbeeCluster *cluster, uint64_t eui64, uint8_t endpointId)
{
    icLinkedList *result = NULL;

    AlarmsCluster *alarmsCluster = (AlarmsCluster*)cluster;

    // lock the overall table so we can check to see if we have an entry and add one if needed
    mutexLock(&alarmsCluster->tempAlarmTableContextMtx);

    TempAlarmTableContext *tempAlarmTableContext = hashMapGet(alarmsCluster->tempAlarmTableContexts, &eui64, sizeof(eui64));

    if (tempAlarmTableContext == NULL)
    {
        tempAlarmTableContext = calloc(1, sizeof(*tempAlarmTableContext));
        tempAlarmTableContext->tempAlarmTable = linkedListCreate();
        pthread_mutex_init(&tempAlarmTableContext->tempAlarmTableMtx, NULL);
        initTimedWaitCond(&tempAlarmTableContext->tempAlarmTableCond);

        // lock the entry before we put into the table
        mutexLock(&tempAlarmTableContext->tempAlarmTableMtx);

        uint64_t *eui64Copy = malloc(sizeof(uint64_t));
        *eui64Copy = eui64;
        hashMapPut(alarmsCluster->tempAlarmTableContexts, eui64Copy, sizeof(*eui64Copy), tempAlarmTableContext);

        // before we wait for the operation to complete we release the overall lock since the entry has been added
        mutexUnlock(&alarmsCluster->tempAlarmTableContextMtx);

        // now send the command that gets the processing going.  The responses come in one at a time until done
        if (zigbeeSubsystemSendCommand(eui64,
                                       endpointId,
                                       ALARMS_CLUSTER_ID,
                                       true,
                                       ALARMS_GET_ALARM_COMMAND_ID,
                                       NULL,
                                       0) == 0)
        {
            incrementalCondTimedWait(&tempAlarmTableContext->tempAlarmTableCond,
                                     &tempAlarmTableContext->tempAlarmTableMtx,
                                     ALARMS_GET_TIMEOUT_SECS);
        }
        else
        {
            icLogWarn(LOG_TAG, "%s: failed to initiate alarm retrieval", __func__);
        }

        // remove the entry from the table since we are done (success or timeout)
        hashMapDelete(alarmsCluster->tempAlarmTableContexts, &eui64, sizeof(eui64), standardDoNotFreeHashMapFunc);

        // safe to unlock since its out of the table (nobody else could have it)
        mutexUnlock(&tempAlarmTableContext->tempAlarmTableMtx);

        // move memory ownership to the caller
        result = tempAlarmTableContext->tempAlarmTable;
        tempAlarmTableContext->tempAlarmTable = NULL;

        // finally destroy our context entry
        tempAlarmTableContextFreeFunc(eui64Copy, tempAlarmTableContext);
    }
    else
    {
        icLogWarn(LOG_TAG, "%s: another get alarms for %016" PRIx64 " is already in progress", __func__, eui64);
        mutexUnlock(&alarmsCluster->tempAlarmTableContextMtx);
    }


    return result;
}

static bool configureCluster(ZigbeeCluster *ctx, const DeviceConfigurationContext *configContext)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    //If the property is set to false we skip, otherwise accept its value or the default of true if nothing was set
    if (getBoolConfigurationMetadata(configContext->configurationMetadata, ALARMS_CLUSTER_DISABLE_BIND_KEY, true))
    {
        if (zigbeeSubsystemBindingSet(configContext->eui64, configContext->endpointId, ALARMS_CLUSTER_ID) != 0)
        {
            icLogError(LOG_TAG, "%s: failed to bind alarms cluster", __FUNCTION__);
            return false;
        }
    }

    return true;
}

static bool parseAlarmTableEntry(uint8_t *commandData, size_t commandDataLen, ZigbeeAlarmTableEntry *entry)
{
    bool result = false;

    //validate.  if there is only 1 byte that means error response.  otherwise there must be 8 bytes.
    if (commandData == NULL || commandDataLen == 0 || entry == NULL || commandDataLen == 1 || commandDataLen != 8)
    {
        icLogWarn(LOG_TAG, "%s: invalid args", __func__);
        return false;
    }

    sbZigbeeIOContext *zio = zigbeeIOInit(commandData, commandDataLen, ZIO_READ);
    if (zio != NULL)
    {
    }

    return result;
}

/**
 * Take an alarm response, find the associated running context, add this alarm to its list and continue retrieving
 * alarms till done.  Once done, signal the waiting context.
 * @param ctx
 * @param command
 */
static void handleAlarmResponse(ZigbeeCluster *ctx, ReceivedClusterCommand *command)
{
    AlarmsCluster *alarmsCluster = (AlarmsCluster*)ctx;

    // lock the overall table so we can check to see if we have an entry and add one if needed
    mutexLock(&alarmsCluster->tempAlarmTableContextMtx);

    TempAlarmTableContext *tempAlarmTableContext = hashMapGet(alarmsCluster->tempAlarmTableContexts,
                                                              &command->eui64,
                                                              sizeof(command->eui64));

    if (tempAlarmTableContext == NULL)
    {
        icLogWarn(LOG_TAG, "%s: got alarm response but there was no waiting context found", __func__);
    }
    else
    {
        mutexLock(&tempAlarmTableContext->tempAlarmTableMtx);

        sbZigbeeIOContext *zio = zigbeeIOInit(command->commandData, command->commandDataLen, ZIO_READ);

        //first check the status byte (byte 0).  If it is success (0) then we can process the rest, otherwise we are done
        uint8_t status = zigbeeIOGetUint8(zio);
        if (status == 0)
        {
            if (command->commandDataLen != 8) //the payload of an alarm record should be 8 bytes
            {
                icLogDebug(LOG_TAG, "%s: unexpected payload length %"PRIu16, __func__, command->commandDataLen);
            }
            else
            {
                ZigbeeAlarmTableEntry *entry = (ZigbeeAlarmTableEntry *) calloc(1, sizeof(ZigbeeAlarmTableEntry));

                entry->alarmCode = zigbeeIOGetUint8(zio);
                entry->clusterId = zigbeeIOGetUint16(zio);
                entry->timeStamp = zigbeeIOGetUint32(zio);

                //zigbee epoch is 1/1/2000 00:00 GMT.  To convert to POSIX time (ISO 8601), add 946684800 (the difference between the two epochs)
                entry->localTimeStamp = entry->timeStamp + 946684800;

                icLogDebug(LOG_TAG, "%s: got alarm:  code=0x%02x, clusterId=0x%04x, timeStamp=%"
                        PRIu32, __func__, entry->alarmCode, entry->clusterId, entry->timeStamp);

                linkedListAppend(tempAlarmTableContext->tempAlarmTable, entry);

                //Trigger the next read
                zigbeeSubsystemSendCommand(command->eui64,
                                           command->sourceEndpoint,
                                           ALARMS_CLUSTER_ID,
                                           true,
                                           ALARMS_GET_ALARM_COMMAND_ID,
                                           NULL,
                                           0);
            }
        }
        else
        {
            icLogDebug(LOG_TAG, "%s: done retrieving alarms", __func__);
            pthread_cond_broadcast(&tempAlarmTableContext->tempAlarmTableCond);
        }

        mutexUnlock(&tempAlarmTableContext->tempAlarmTableMtx);
    }

    mutexUnlock(&alarmsCluster->tempAlarmTableContextMtx);
}

static bool handleClusterCommand(ZigbeeCluster *ctx, ReceivedClusterCommand *command)
{
    bool result = false;

    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    AlarmsCluster *alarmsCluster = (AlarmsCluster *) ctx;

    switch (command->commandId)
    {
        case ALARMS_ALARM_COMMAND_ID:
        {
            if (alarmsCluster->callbacks->alarmReceived != NULL)
            {
                ZigbeeAlarmTableEntry entry;
                memset(&entry, 0, sizeof(ZigbeeAlarmTableEntry));

                entry.alarmCode = command->commandData[0];
                entry.clusterId = command->commandData[1] + (command->commandData[2] << 8);

                alarmsCluster->callbacks->alarmReceived(command->eui64,
                                                        command->sourceEndpoint,
                                                        &entry,
                                                        alarmsCluster->callbackContext);
            }
            result = true;
            break;
        }

        case ALARMS_CLEAR_ALARM_COMMAND_ID:
        {
            if (alarmsCluster->callbacks->alarmCleared != NULL)
            {
                ZigbeeAlarmTableEntry entry;
                memset(&entry, 0, sizeof(ZigbeeAlarmTableEntry));

                entry.alarmCode = command->commandData[0];
                entry.clusterId = command->commandData[1] + (command->commandData[2] << 8);

                alarmsCluster->callbacks->alarmCleared(command->eui64,
                                                       command->sourceEndpoint,
                                                       &entry,
                                                       alarmsCluster->callbackContext);
            }
            result = true;
            break;
        }

        case ALARMS_GET_ALARM_RESPONSE_COMMAND_ID:
        {
            handleAlarmResponse(ctx, command);
            break;
        }

        default:
            icLogError(LOG_TAG, "%s: unexpected command id 0x%02x", __FUNCTION__, command->commandId);
            break;

    }

    return result;
}

#endif //BARTON_CONFIG_ZIGBEE