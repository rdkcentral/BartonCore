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
 * Created by Thomas Lea on 3/11/21.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define MATTER_SUBSYSTEM_NAME    "matter"
#define MATTER_SUBSYSTEM_VERSION (1U)


#include <stdint.h>

#include "subsystemManagerCallbacks.h"
#include <device-driver/device-driver.h>

/*
typedef struct
{
    void *device;
    uint8_t endpoint;
} MatterDeviceInfo;
 */

/**
 * Locate the commissionable device as specified in the provided setup payload and commission it.
 *
 * @param setupPayload the decoded QR code setup payload
 * @param timeoutSeconds the maximum number of seconds to allow for commissioning to complete
 * @return true if the device is successfully commissioned
 */
bool matterSubsystemCommissionDevice(const char *setupPayload, uint16_t timeoutSeconds);

/**
 * Locate the device identified by nodeId and pair it
 *
 * @param nodeId
 * @param timeoutSeconds the maximum number of seconds to allow for pairing to complete
 * @return true if the device is successfully commissioned
 */
bool matterSubsystemPairDevice(uint64_t nodeId, uint16_t timeoutSeconds);

/*
 * Open a commissioning window locally or for a specific device. When successful, the generated setup code and
 * QR code are returned.  The caller is responsible for freeing the setupCode and qrCode.
 *
 * @param nodeId - the nodeId of the device to open the commissioning window for, or NULL for local
 * @param timeoutSeconds - the number of seconds to perform discovery before automatically stopping or 0 for default
 * @param setupCode - receives the setup code if successful (caller frees)
 * @param qrCode - receives the QR code if successful (caller frees)
 * @returns true if commissioning window is opened
 */
bool matterSubsystemOpenCommissioningWindow(const char *nodeId, uint16_t timeoutSeconds, char **setupCode, char **qrCode);

/**
 * Clear the AccessRestrictionList for certification testing.
 *
 * @return true on success
 */
bool matterSubsystemClearAccessRestrictionList(void);

/*
MatterDeviceInfo *matterSubsystemGetDeviceInfo(const char *uuid);
 */

#ifdef __cplusplus
}
#endif
