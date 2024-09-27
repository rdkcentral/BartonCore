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

#ifndef ZILKER_COMCASTBATTERYSAVINGHELPER_H
#define ZILKER_COMCASTBATTERYSAVINGHELPER_H

#include <deviceDrivers/zigbeeDriverCommon.h>
#include "zigbeeClusters/comcastBatterySaving.h"

/**
 * Parse the provided buffer into a ComcastBatterySavingData structure
 *
 * @param buffer  the buffer to parse
 * @param bufferLen  the length of the buffer to parse
 * @return a pointer to the parsed data on success or NULL on failure
 */
ComcastBatterySavingData *comcastBatterySavingDataParse(uint8_t *buffer, uint16_t bufferLen);

#endif //ZILKER_COMCASTBATTERYSAVINGHELPER_H
