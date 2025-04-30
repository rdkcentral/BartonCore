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

#pragma once
#include "device/icDevice.h"
#include "stdint.h"

void deviceServiceCommFailInit(void);
void deviceServiceCommFailShutdown(void);

/**
 * @brief Set the service wide comm fail timeout in seconds.
 *        Devices that have 'commFailOverrideSeconds' metadata will
 *        override this value automatically.
 *
 * @param commFailTimeoutSecs
 */
void deviceServiceCommFailSetTimeoutSecs(uint32_t commFailTimeoutSecs);

/**
 * @brief Get the service wide comm fail timeout in seconds.
 *
 * @return uint32_t
 */
uint32_t deviceServiceCommFailGetTimeoutSecs(void);

/**
 * @brief Notify a device driver of its default comm-fail timeout, in seconds.
 *        This is included in all '*Set' functions, and is useful for
 *        notifying drivers of intent without actually starting monitoring (e.g., for configuration)
 *
 * @param device
 * @param defaultTimeoutSecs
 */
void deviceServiceCommFailHintDeviceTimeoutSecs(const icDevice *device, uint32_t defaultTimeoutSecs);

/**
 * @brief Set the device comm fail timeout, in seconds. If the device has
 *        'commFailOverrideSeconds,' the value there will be used instead.
 *
 * @param device
 * @param defaultTimeoutSecs
 */
void deviceServiceCommFailSetDeviceTimeoutSecs(const icDevice *device, uint32_t defaultTimeoutSecs);

/**
 * @brief Get the comm fail timeout for a device, in seconds
 *
 * @param deviceUuid
 * @return uint32_t
 */
uint32_t deviceServiceCommFailGetDeviceTimeoutSecs(const icDevice *device);
