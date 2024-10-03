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
// Created by tlea on 9/11/19.
//

#ifndef ZILKER_ZIGBEETELEMETRY_H
#define ZILKER_ZIGBEETELEMETRY_H

#ifdef BARTON_CONFIG_SUPPORT_ZIGBEE_TELEMETRY

/**
 * Initialize the Zigbee Telemetry system, potentially starting network captures.
 */
void zigbeeTelemetryInitialize(void);

/**
 * Shut down the Zigbee Telemetry system, stopping any running captures and releasing resources.
 */
void zigbeeTelemetryShutdown(void);

/**
 * Update a Zigbee telemetry related property which could start, stop, or change a running capture's configuration.
 * @param key
 * @param value
 */
void zigbeeTelemetrySetProperty(const char *key, const char *value);

#endif // BARTON_CONFIG_SUPPORT_ZIGBEE_TELEMETRY

#endif // ZILKER_ZIGBEETELEMETRY_H
