// ------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2025 Comcast Cable Communications Management, LLC
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
// ------------------------------ tabstop = 4 ----------------------------------

//
// Created by Kevin Funderburg on 8/26/2025.
//

#include <stdlib.h>

#include "private/subsystems/zigbee/zigbeeWatchdogDelegate.h"

ZigbeeWatchdogDelegate *createZigbeeWatchdogDelegate(void)
{
    ZigbeeWatchdogDelegate *retVal = calloc(1, sizeof(ZigbeeWatchdogDelegate));
    retVal->initializeWatchdog = NULL;
    retVal->shutdownWatchdog = NULL;
    retVal->updateWatchdogStatus = NULL;
    retVal->petWatchdog = NULL;
    retVal->restartZigbeeCore = NULL;
    retVal->notifyDeviceCommRestored = NULL;
    retVal->getActionInProgress = NULL;

    return retVal;
}

void destroyZigbeeWatchdogDelegate(ZigbeeWatchdogDelegate *delegate)
{
    free(delegate);
}
