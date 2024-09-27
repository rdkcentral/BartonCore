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
// Created by tlea on 2/18/19.
//

#ifndef ZILKER_DIAGNOSTICSCLUSTER_H
#define ZILKER_DIAGNOSTICSCLUSTER_H

#include "zigbeeCluster.h"

typedef struct
{
    void (*lastMessageRssiLqiUpdated)(void *ctx, uint64_t eui64, uint8_t endpointId, int8_t rssi, uint8_t lqi);
} DiagnosticsClusterCallbacks;

ZigbeeCluster *diagnosticsClusterCreate(const DiagnosticsClusterCallbacks *callbacks, void *callbackContext);

bool diagnosticsClusterGetLastMessageLqi(uint64_t eui64, uint8_t endpointId, uint8_t *lqi);
bool diagnosticsClusterGetLastMessageRssi(uint64_t eui64, uint8_t endpointId, int8_t *rssi);

#endif //ZILKER_DIAGNOSTICSCLUSTER_H
