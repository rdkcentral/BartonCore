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
// Created by tlea on 7/9/19.
//

#ifndef ZILKER_ZIGBEEHEALTHCHECK_H
#define ZILKER_ZIGBEEHEALTHCHECK_H

#include <stdbool.h>

/**
 * Start monitoring the zigbee network for health.  It is safe to call this multiple times, such as when a
 * related property changes.
 */
void zigbeeHealthCheckStart();

/**
 * Stop monitoring the zigbee network for health
 */
void zigbeeHealthCheckStop();

/**
 * Set the state of the network health (as reported by zhal)
 *
 * @param problemExists
 */
void zigbeeHealthCheckSetProblem(bool problemExists);

#endif // ZILKER_ZIGBEEHEALTHCHECK_H
