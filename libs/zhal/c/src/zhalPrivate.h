//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2015 iControl Networks, Inc.
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
// iControl Networks retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------
/*
 * Created by Thomas Lea on 3/15/16.
 */

#ifndef FLEXCORE_ZHALPRIVATE_H
#define FLEXCORE_ZHALPRIVATE_H

#define LOG_TAG "zhalImpl"

#include <zhal/zhal.h>

cJSON *zhalSendRequest(uint64_t targetEui64, cJSON *requestJson, int timeoutSecs);

zhalCallbacks *getCallbacks(void);

void *getCallbackContext(void);

bool zhalIsInitialized(void);

#endif // FLEXCORE_ZHALPRIVATE_H
