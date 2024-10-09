//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2021 Comcast Corporation
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
// Created by tlea on 11/8/2021
//

#ifndef ZILKER_REMOTECLICLUSTER_H
#define ZILKER_REMOTECLICLUSTER_H


#ifdef BARTON_CONFIG_ZIGBEE

#include "zigbeeCluster.h"

typedef struct
{
    void (*handleCliCommandResponse)(uint64_t eui64, uint8_t endpointId, const char *response, const void *ctx);
} RemoteCliClusterCallbacks;

ZigbeeCluster *remoteCliClusterCreate(const RemoteCliClusterCallbacks *callbacks, void *callbackContext);

bool remoteCliClusterEnable(uint64_t eui64, uint8_t endpointId, uint8_t enable, uint16_t longPollInterval);

bool remoteCliClusterSendCommand(uint64_t eui64, uint8_t endpointId, const char *command);

#endif // BARTON_CONFIG_ZIGBEE

#endif // ZILKER_REMOTECLICLUSTER_H
