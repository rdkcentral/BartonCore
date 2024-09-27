//------------------------------ tabstop = 4 ----------------------------------
// 
// Copyright (C) 2018 Comcast
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
// Comcast retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------
//
// Created by mkoch201 on 3/18/19.
//

#ifndef ZILKER_BRIDGECLUSTER_H
#define ZILKER_BRIDGECLUSTER_H


#ifdef BARTON_CONFIG_ZIGBEE

#include "zigbeeCluster.h"


typedef struct
{
    void (*refreshRequested)(uint64_t eui64,
                             uint8_t endpointId,
                             const void *ctx);
    void (*refreshCompleted)(uint64_t eui64,
                             uint8_t endpointId,
                             const void *ctx);
    void (*bridgeTamperStatusChanged)(uint64_t eui64,
                                      uint8_t endpointId,
                                      const void *ctx,
                                      bool isTampered);
} BridgeClusterCallbacks;

ZigbeeCluster *bridgeClusterCreate(const BridgeClusterCallbacks *callbacks, const void *callbackContext);

bool bridgeClusterRefresh(uint64_t eui64, uint8_t endpointId);

bool bridgeClusterStartConfiguration(uint64_t eui64, uint8_t endpointId);

bool bridgeClusterStopConfiguration(uint64_t eui64, uint8_t endpointId);

bool bridgeClusterReset(uint64_t eui64, uint8_t endpointId);

bool bridgeClusterGetTamperStatus(const ZigbeeCluster *ctx,
                                  uint64_t eui64,
                                  uint8_t endpointId,
                                  bool *result);

#endif // BARTON_CONFIG_ZIGBEE

#endif //ZILKER_BRIDGECLUSTER_H
