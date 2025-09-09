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
 * Created by Thomas Lea on 3/15/16.
 */

#ifndef FLEXCORE_ZHALASYNCRECEIVER_H
#define FLEXCORE_ZHALASYNCRECEIVER_H

#include <cjson/cJSON.h>

// These handlers should return non-zero values to indicate that they will handle the payload argument's lifecycle
typedef int (*zhalIpcResponseHandler)(cJSON *response);
typedef int (*zhalEventHandler)(cJSON *event);

int zhalAsyncReceiverStart(const char *host, zhalIpcResponseHandler ipcHandler, zhalEventHandler eventHandler);
int zhalAsyncReceiverStop();

#endif // FLEXCORE_ZHALASYNCRECEIVER_H
