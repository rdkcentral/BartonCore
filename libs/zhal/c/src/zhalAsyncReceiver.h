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

#ifndef FLEXCORE_ZHALASYNCRECEIVER_H
#define FLEXCORE_ZHALASYNCRECEIVER_H

#include <cjson/cJSON.h>

// These handlers should return non-zero values to indicate that they will handle the payload argument's lifecycle
typedef int (*zhalIpcResponseHandler)(cJSON *response);
typedef int (*zhalEventHandler)(cJSON *event);

int zhalAsyncReceiverStart(const char *host, zhalIpcResponseHandler ipcHandler, zhalEventHandler eventHandler);
int zhalAsyncReceiverStop();

#endif // FLEXCORE_ZHALASYNCRECEIVER_H
