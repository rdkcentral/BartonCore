//------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2024 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
//
//------------------------------ tabstop = 4 ----------------------------------

//
// Created by tlea on 5/1/19.
//

#include "zigbeeClusters/helpers/comcastBatterySavingHelper.h"
#include <commonDeviceDefs.h>
#include <deviceDrivers/zigbeeDriverCommon.h>
#include <deviceServicePrivate.h>
#include <icLog/logging.h>
#include <icTypes/sbrm.h>
#include <icUtil/stringUtils.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <subsystems/zigbee/zigbeeIO.h>
#include <zigbeeClusters/comcastBatterySaving.h>
#include <zigbeeClusters/temperatureMeasurementCluster.h>

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
