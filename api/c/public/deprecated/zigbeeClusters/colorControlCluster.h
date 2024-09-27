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

#ifndef ZILKER_COLORCONTROLCLUSTER_H
#define ZILKER_COLORCONTROLCLUSTER_H


#ifdef BARTON_CONFIG_ZIGBEE

#include "zigbeeCluster.h"

typedef struct
{
    void (*currentXChanged)(uint64_t eui64,
                            uint8_t endpointId,
                            double x,
                            const void *ctx);

    void (*currentYChanged)(uint64_t eui64,
                            uint8_t endpointId,
                            double y,
                            const void *ctx);
    void (*currentXYChanged)(uint64_t eui64,
                             uint8_t endpointId,
                             double x,
                             double y,
                             const void *ctx);
} ColorControlClusterCallbacks;


ZigbeeCluster *colorControlClusterCreate(const ColorControlClusterCallbacks *callbacks, void *callbackContext);

bool colorControlClusterGetX(uint64_t eui64, uint8_t endpointId, double *x);
bool colorControlClusterGetY(uint64_t eui64, uint8_t endpointId, double *y);
bool colorControlClusterMoveToColor(uint64_t eui64, uint8_t endpointId, double x, double y);

#endif // BARTON_CONFIG_ZIGBEE

#endif //ZILKER_COLORCONTROLCLUSTER_H
