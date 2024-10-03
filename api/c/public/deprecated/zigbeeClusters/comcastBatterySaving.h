//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2018 Comcast Corporation
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
// Created by tlea on 4/30/19.
//

/**
 * To save battery, we have extended some cluster commands (on IAS Zone and Poll Control) with additional
 * data to prevent the need to query the device after receiving some commands.
 */

#ifndef ZILKER_COMCASTBATTERYSAVING_H
#define ZILKER_COMCASTBATTERYSAVING_H

#include <inttypes.h>

#define OPTIONAL_SENSOR_DATA_TYPE_METADATA         "comcast.battSave.extra"
#define OPTIONAL_SENSOR_DATA_TYPE_MAGNETIC         "magStr"
#define OPTIONAL_SENSOR_DATA_TYPE_BATTERY_CAP_USED "battCapUsed"

typedef union
{
    uint16_t data;
    uint16_t battUsedMilliAmpHr;
    uint16_t magStrMicroTesla;
} OptionalSensorData;

typedef struct
{
    uint16_t battVoltage;
    OptionalSensorData optionalData;
    int16_t temp;
    int8_t rssi;
    uint8_t lqi;
    uint32_t retries;
    uint32_t rejoins;
} ComcastBatterySavingData;

#endif // ZILKER_COMCASTBATTERYSAVING_H
