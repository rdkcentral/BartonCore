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
 * The device driver manager handles the registration and interactions with the
 * various device drivers, each of which is responsible for understanding how to
 * interact with various device classes.
 *
 * Created by Thomas Lea on 7/28/15.
 */

#ifndef FLEXCORE_DEVICEDRIVERMANAGER_H
#define FLEXCORE_DEVICEDRIVERMANAGER_H

#include <device-driver/device-driver.h>
#include <stdbool.h>

bool deviceDriverManagerInitialize();
bool deviceDriverManagerStartDeviceDrivers();
bool deviceDriverManagerShutdown();
DeviceDriver *deviceDriverManagerGetDeviceDriver(const char *driverName);
icLinkedList *deviceDriverManagerGetDeviceDriversByDeviceClass(const char *deviceClass);
icLinkedList *deviceDriverManagerGetDeviceDrivers();
icLinkedList *deviceDriverManagerGetDeviceDriversBySubsystem(const char *subsystem);

#endif // FLEXCORE_DEVICEDRIVERMANAGER_H
