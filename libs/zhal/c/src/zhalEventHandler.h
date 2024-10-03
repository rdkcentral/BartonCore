//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2021 Comcast Corporation
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

/*
 * Created by Thomas Lea on 3/25/21.
 */

#ifndef ZILKER_ZHALEVENTHANDLER_H
#define ZILKER_ZHALEVENTHANDLER_H

#include <cjson/cJSON.h>

/**
 * Initialize the event handler
 */
void zhalEventHandlerInit(void);

/**
 * Terminate the event handler and clean up
 */
void zhalEventHandlerTerm(void);

/**
 * Handle an event
 * @param event
 * @return
 */
int zhalHandleEvent(cJSON *event);

#endif // ZILKER_ZHALEVENTHANDLER_H
