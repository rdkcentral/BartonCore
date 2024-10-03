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

#ifndef ZILKER_ELECTRICALMEASUREMENTCLUSTER_H
#define ZILKER_ELECTRICALMEASUREMENTCLUSTER_H


#ifdef BARTON_CONFIG_ZIGBEE

#include "zigbeeCluster.h"

typedef struct
{
    void (*activePowerChanged)(uint64_t eui64, uint8_t endpointId, int16_t watts, const void *ctx);
} ElectricalMeasurementClusterCallbacks;


ZigbeeCluster *electricalMeasurementClusterCreate(const ElectricalMeasurementClusterCallbacks *callbacks,
                                                  void *callbackContext);

bool electricalMeasurementClusterGetActivePower(uint64_t eui64, uint8_t endpointId, int16_t *watts);
bool electricalMeasurementClusterGetAcPowerDivisor(uint64_t eui64, uint8_t endpointId, uint16_t *divisor);
bool electricalMeasurementClusterGetAcPowerMultiplier(uint64_t eui64, uint8_t endpointId, uint16_t *multiplier);

#endif // BARTON_CONFIG_ZIGBEE

#endif // ZILKER_ELECTRICALMEASUREMENTCLUSTER_H
