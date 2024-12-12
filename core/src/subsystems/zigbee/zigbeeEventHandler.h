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
