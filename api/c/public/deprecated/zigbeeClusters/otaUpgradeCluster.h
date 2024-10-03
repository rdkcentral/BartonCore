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
// Created by tlea on 9/23/19.
//

#ifndef ZILKER_OTAUPGRADECLUSTER_H
#define ZILKER_OTAUPGRADECLUSTER_H


#ifdef BARTON_CONFIG_ZIGBEE

#include "zigbeeCluster.h"

ZigbeeCluster *otaUpgradeClusterCreate(void);

/**
 * Notify a device that uses the OTA Upgrade cluster that we have a new firmware image for them.
 * @param eui64
 * @param endpointId
 *
 * @return true on success
 */
bool otaUpgradeClusterImageNotify(uint64_t eui64, uint8_t endpointId);

#endif // BARTON_CONFIG_ZIGBEE

#endif // ZILKER_OTAUPGRADECLUSTER_H
