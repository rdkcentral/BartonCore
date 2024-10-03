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
 * Created by Thomas Lea on 9/19/16.
 */

#ifndef FLEXCORE_DEVICECOMMUNICATIONWATCHDOG_H
#define FLEXCORE_DEVICECOMMUNICATIONWATCHDOG_H

#include <stdbool.h>
#include <stdint.h>

typedef void (*deviceCommunicationWatchdogCommFailedCallback)(const char *uuid);
typedef void (*deviceCommunicationWatchdogCommRestoredCallback)(const char *uuid);

void deviceCommunicationWatchdogInit(deviceCommunicationWatchdogCommFailedCallback failedCallback,
                                     deviceCommunicationWatchdogCommRestoredCallback restoredCallback);
void deviceCommunicationWatchdogTerm();

/*
 * Adjust the monitor interval.  This is mostly useful for unit tests where the default interval is too long.
 */
void deviceCommunicationWatchdogSetMonitorInterval(uint32_t seconds);

void deviceCommunicationWatchdogSetFastCommfail(bool inFastCommfail);

void deviceCommunicationWatchdogMonitorDevice(const char *uuid, const uint32_t commFailTimeoutSeconds, bool inCommFail);
void deviceCommunicationWatchdogStopMonitoringDevice(const char *uuid);

void deviceCommunicationWatchdogPetDevice(const char *uuid);

/**
 * Wake the watchdog and have it check devices for comm fail.
 */
void deviceCommunicationWatchdogCheckDevices(void);
void deviceCommunicationWatchdogForceDeviceInCommFail(const char *uuid);

/**
 * Get a device's commfail time remaining from the host (normally to be given to xNCP)
 * @param uuid the monitored device id
 * @param commFailDelaySeconds Fixed, nominal seconds until a device is considered in commfail.
 *                             Must be longer than or the same as the touchscreen.sensor.commFail.troubleDelay value; if
 *                             not, this parameter is ignored with a warning.
 *                             FIXME: armed and unarmed delays are different (longer for armed)?! This shouldn't even be
 * a parameter.
 * @return Seconds to count down from until comm failure
 */
int32_t deviceCommunicationWatchdogGetRemainingCommFailTimeoutForLPM(const char *uuid, uint32_t commFailDelaySeconds);

/**
 * Set the remaining seconds from a monitored device in the host (normally from xNCP)
 * @param uuid Device uuid
 * @param commFailTimeRemainingSeconds seconds remaining until comm failure
 */
void deviceCommunicationWatchdogSetTimeRemainingForDeviceFromLPM(const char *uuid,
                                                                 int32_t commFailTimeRemainingSeconds);

/**
 * Check if a device is being monitored
 * @param uuid device uuid
 *
 * @return true if device is being monitored
 */
bool deviceCommunicationWatchdogIsDeviceMonitored(const char *uuid);
#endif // FLEXCORE_DEVICECOMMUNICATIONWATCHDOG_H
