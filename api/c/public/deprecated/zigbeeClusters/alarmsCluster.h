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

#ifndef ZILKER_ALARMSCLUSTER_H
#define ZILKER_ALARMSCLUSTER_H


#ifdef BARTON_CONFIG_ZIGBEE

#include "zigbeeCluster.h"


typedef struct
{
    void (*alarmReceived)(uint64_t eui64, uint8_t endpointId, const ZigbeeAlarmTableEntry *entry, const void *ctx);

    void (*alarmCleared)(uint64_t eui64, uint8_t endpointId, const ZigbeeAlarmTableEntry *entry, const void *ctx);
} AlarmsClusterCallbacks;

ZigbeeCluster *alarmsClusterCreate(const AlarmsClusterCallbacks *callbacks, const void *callbackContext);

/**
 * Retrieve the current entries in the alarm table.  Per spec, this will also clear the alarm table.
 *
 * @param cluster
 * @param eui64
 * @param endpointId
 * @return  a linked list of ZigbeeAlarmTableEntrys or NULL on failure
 */
icLinkedList *alarmsClusterGetAlarms(const ZigbeeCluster *cluster, uint64_t eui64, uint8_t endpointId);

/**
 * Set whether or not to set a binding on this cluster.  By default we bind the cluster.
 * @param deviceConfigurationContext the configuration context
 * @param bind true to bind or false not to
 */
void alarmsClusterSetBindingEnabled(const DeviceConfigurationContext *deviceConfigurationContext, bool bind);

#endif // BARTON_CONFIG_ZIGBEE

#endif // ZILKER_ALARMSCLUSTER_H
