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
// Created by tlea on 5/1/19.
//

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <zigbeeClusters/comcastBatterySaving.h>
#include <zigbeeClusters/temperatureMeasurementCluster.h>
#include <stddef.h>
#include <icLog/logging.h>
#include <subsystems/zigbee/zigbeeIO.h>
#include <icTypes/sbrm.h>
#include <commonDeviceDefs.h>
#include <deviceDrivers/zigbeeDriverCommon.h>
#include <deviceServicePrivate.h>
#include <icUtil/stringUtils.h>
#include "zigbeeClusters/helpers/comcastBatterySavingHelper.h"

#define LOG_TAG "ComcastBatterySavingHelper"

ComcastBatterySavingData *comcastBatterySavingDataParse(uint8_t *buffer, uint16_t bufferLen)
{
    ComcastBatterySavingData *result = NULL;

    if (buffer == NULL || bufferLen < sizeof(ComcastBatterySavingData))
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
    }
    else
    {
        result = calloc(1, sizeof(*result));

        sbZigbeeIOContext *zio = zigbeeIOInit(buffer, bufferLen, ZIO_READ);

        result->battVoltage = zigbeeIOGetUint16(zio);
        result->optionalData.data = zigbeeIOGetUint16(zio);
        result->temp = zigbeeIOGetInt16(zio);
        result->rssi = zigbeeIOGetInt8(zio);
        result->lqi = zigbeeIOGetUint8(zio);
        result->retries = zigbeeIOGetUint32(zio);
        result->rejoins = zigbeeIOGetUint32(zio);
    }

    return result;
}