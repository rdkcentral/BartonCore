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
