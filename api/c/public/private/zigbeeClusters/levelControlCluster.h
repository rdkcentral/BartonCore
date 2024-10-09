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
// Created by tlea on 2/19/19.
//

#ifndef ZILKER_LEVELCONTROLCLUSTER_H
#define ZILKER_LEVELCONTROLCLUSTER_H


#ifdef BARTON_CONFIG_ZIGBEE

#include "zigbeeCluster.h"

typedef struct
{
    void (*levelChanged)(uint64_t eui64, uint8_t endpointId, uint8_t level, const void *ctx);
} LevelControlClusterCallbacks;


ZigbeeCluster *levelControlClusterCreate(const LevelControlClusterCallbacks *callbacks, void *callbackContext);

bool levelControlClusterGetLevel(uint64_t eui64, uint8_t endpointId, uint8_t *level);

bool levelControlClusterSetLevel(uint64_t eui64, uint8_t endpointId, uint8_t level);

void levelControlClusterSetBindingEnabled(const DeviceConfigurationContext *deviceConfigurationContext, bool bind);

bool levelControlClusterSetAttributeReporting(uint64_t eui64, uint8_t endpointId);

// caller frees
char *levelControlClusterGetLevelString(uint8_t level);

uint8_t levelControlClusterGetLevelFromString(const char *level);

#endif // BARTON_CONFIG_ZIGBEE

#endif // ZILKER_LEVELCONTROLCLUSTER_H
