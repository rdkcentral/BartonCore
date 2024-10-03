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
// Created by tlea on 3/1/19
//

#ifndef ZILKER_FANCONTROLCLUSTER_H
#define ZILKER_FANCONTROLCLUSTER_H


#ifdef BARTON_CONFIG_ZIGBEE

#include "zigbeeCluster.h"

typedef struct
{
    void (*fanModeChanged)(uint64_t eui64, uint8_t endpointId, uint8_t mode, const void *ctx);
} FanControlClusterCallbacks;


ZigbeeCluster *fanControlClusterCreate(const FanControlClusterCallbacks *callbacks, void *callbackContext);

/**
 * Set whether or not to set a binding on this cluster.  By default we bind the cluster.
 * @param deviceConfigurationContext the configuration context
 * @param bind true to bind or false not to
 */
void fanControlClusterSetBindingEnabled(const DeviceConfigurationContext *deviceConfigurationContext, bool bind);

bool fanControlClusterGetFanMode(const ZigbeeCluster *cluster, uint64_t eui64, uint8_t endpointId, uint8_t *mode);

bool fanControlClusterSetFanMode(const ZigbeeCluster *cluster, uint64_t eui64, uint8_t endpointId, uint8_t mode);

const char *fanControlClusterGetFanModeString(uint8_t fanMode);

#endif // BARTON_CONFIG_ZIGBEE

#endif // ZILKER_FANCONTROLCLUSTER_H
