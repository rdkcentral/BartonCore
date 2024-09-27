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

#ifndef ZILKER_REMOTECELLMODEMCLUSTER_H
#define ZILKER_REMOTECELLMODEMCLUSTER_H

#include "zigbeeCluster.h"

#ifdef BARTON_CONFIG_ZIGBEE

typedef struct {
    void (*onOffStateChanged)(uint64_t eui64, uint8_t endpointId, bool isOn);
} RemoteCellModemClusterCallbacks;

ZigbeeCluster *remoteCellModemClusterCreate(const RemoteCellModemClusterCallbacks *callbacks, void *callbackContext, uint16_t manufacturerId);

/**
 * Determines whether or not the remote cell modem is powered on or not.
 * @param ctx
 * @param eui64
 * @param endpointId
 * @param result
 * @return
 */
bool remoteCellModemClusterIsPoweredOn(ZigbeeCluster *ctx, uint64_t eui64, uint8_t endpointId, bool *result);

/**
 * Powers the remote cell modem on.
 * @param ctx
 * @param eui64
 * @param endpointId
 * @return
 */
bool remoteCellModemClusterPowerOn(ZigbeeCluster *ctx, uint64_t eui64, uint8_t endpointId);

/**
 * Powers the remote cell modem off.
 * @param ctx
 * @param eui64
 * @param endpointId
 * @return
 */
bool remoteCellModemClusterPowerOff(ZigbeeCluster *ctx, uint64_t eui64, uint8_t endpointId);

/**
 * Forces the remote cell modem to perform an emergency reset.
 * @param ctx
 * @param eui64
 * @param endpointId
 * @return
 */
bool remoteCellModemClusterEmergencyReset(ZigbeeCluster *ctx, uint64_t eui64, uint8_t endpointId);

#endif // BARTON_CONFIG_ZIGBEE
#endif // ZILKER_REMOTECELLMODEMCLUSTER_H
