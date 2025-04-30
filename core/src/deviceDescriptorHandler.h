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
 * The device descriptor handler is responsible for ensuring that the latest allowlist and denylist (which provide
 * the set of device descriptors) are downloaded and available.
 *
 * Created by Thomas Lea on 10/23/15.
 */

#ifndef FLEXCORE_DEVICEDESCRIPTORHANDLER_H
#define FLEXCORE_DEVICEDESCRIPTORHANDLER_H

#include <stdbool.h>

/*
 * Callback to be called only once after startup, when we have device descriptors and are ready to pair devices
 */
typedef void (*deviceDescriptorsReadyForPairingFunc)(void);

/*
 * Callback for when device descriptors have been updated
 */
typedef void (*deviceDescriptorsUpdatedFunc)();

/*
 * Initialize the device descriptor handler. The readyForPairingCallback will be invoked when
 * we have valid descriptors.  The descriptorsUpdatedCallback will be invoked whenever the
 * white or black list changes.
 */
void deviceServiceDeviceDescriptorsInit(deviceDescriptorsReadyForPairingFunc readyForPairingCallback,
                                        deviceDescriptorsUpdatedFunc descriptorsUpdatedCallback);

/*
 * Cleanup the handler for shutdown.
 */
void deviceServiceDeviceDescriptorsDestroy();

/*
 * Process the provided allowlist URL.  This will download it if required then invoke the readyForDevicesCallback
 * if we have a valid list.
 *
 * @param allowlistUrl - the allowlist url to download from
 */
void deviceDescriptorsUpdateAllowlist(const char *allowlistUrl);

/*
 * Process the provided denylist URL.  This will download it if required.
 *
 * @param denylistUrl - the denylist url to download from
 */
void deviceDescriptorsUpdateDenylist(const char *denylistUrl);

#endif // FLEXCORE_DEVICEDESCRIPTORHANDLER_H
