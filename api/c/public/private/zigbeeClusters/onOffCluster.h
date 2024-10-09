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
// Created by tlea on 2/13/19.
//

#ifndef ZILKER_ONOFFCLUSTER_H
#define ZILKER_ONOFFCLUSTER_H


#ifdef BARTON_CONFIG_ZIGBEE

#include "zigbeeCluster.h"

typedef struct
{
    void (*onOffStateChanged)(uint64_t eui64, uint8_t endpointId, bool isOn, const void *ctx);
} OnOffClusterCallbacks;


ZigbeeCluster *onOffClusterCreate(const OnOffClusterCallbacks *callbacks, void *callbackContext);

/**
 * Set whether or not to set a binding on the on off cluster.  By default we bind the cluster.
 * @param deviceConfigurationContext the configuration context
 * @param bind true to bind or false not to
 */
void onOffClusterSetBindingEnabled(const DeviceConfigurationContext *deviceConfigurationContext, bool bind);
bool onOffClusterSetAttributeReporting(uint64_t eui64, uint8_t endpointId);
bool onOffClusterIsOn(uint64_t eui64, uint8_t endpointId, bool *isOn);
bool onOffClusterSetOn(uint64_t eui64, uint8_t endpointId, bool isOn);

#endif // BARTON_CONFIG_ZIGBEE

#endif // ZILKER_ONOFFCLUSTER_H
