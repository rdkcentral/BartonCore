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
 * Created by Thomas Lea on 3/22/16.
 */

#ifndef FLEXCORE_ZHALEVENTHANDLER_H
#define FLEXCORE_ZHALEVENTHANDLER_H

#include <zhal/zhal.h>

/*
 * Populate the callbacks structure with the handlers in this module.
 *
 * Events will not be handled until zigbeeEventHandlerSystemReady is called.
 */
int zigbeeEventHandlerInit(zhalCallbacks *callbacks);

/*
 * Informs the event handler that the system is ready and it can now start handling events.
 */
int zigbeeEventHandlerSystemReady();

/*
 * Informs the event handler when discovery starts or stops
 */
void zigbeeEventHandlerDiscoveryRunning(bool isRunning);

#endif // FLEXCORE_ZHALEVENTHANDLER_H

