//------------------------------ tabstop = 4 ----------------------------------
// 
// Copyright (C) 2019 Comcast
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
// Created by mkoch201 on 3/25/19.
//

#ifndef ZILKER_IASWDCLUSTER_H
#define ZILKER_IASWDCLUSTER_H


#ifdef BARTON_CONFIG_ZIGBEE

#include "zigbeeCluster.h"

typedef struct
{
} IASWDClusterCallbacks;

typedef enum {
    IASWD_WARNING_MODE_STOP = 0,
    IASWD_WARNING_MODE_BURGLAR,
    IASWD_WARNING_MODE_FIRE,
    IASWD_WARNING_MODE_EMERGENCY,
    IASWD_WARNING_MODE_POLICE_PANIC,
    IASWD_WARNING_MODE_FIRE_PANIC,
    IASWD_WARNING_MODE_EMERGENCY_PANIC,
    IASWD_WARNING_MODE_CO
} IASWDWarningMode;

typedef enum {
    IASWD_SIREN_LEVEL_LOW = 0,
    IASWD_SIREN_LEVEL_MEDIUM,
    IASWD_SIREN_LEVEL_HIGH,
    IASWD_SIREN_LEVEL_MAXIMUM
} IASWDSirenLevel;

typedef enum {
    IASWD_STROBE_LEVEL_LOW = 0,
    IASWD_STROBE_LEVEL_MEDIUM,
    IASWD_STROBE_LEVEL_HIGH,
    IASWD_STROBE_LEVEL_MAXIMUM
} IASWDStrobeLevel;

ZigbeeCluster *iasWDClusterCreate(const IASWDClusterCallbacks *callbacks, void *callbackContext);

bool iasWDClusterStartWarning(uint64_t eui64,
                              uint8_t endpointId,
                              IASWDWarningMode warningMode,
                              IASWDSirenLevel sirenLevel,
                              bool enableStrobe,
                              uint16_t warningDuration,
                              uint8_t strobeDutyCycle,
                              IASWDStrobeLevel strobeLevel);


#endif // BARTON_CONFIG_ZIGBEE

#endif //ZILKER_IASWDCLUSTER_H
