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

//
// Created by tlea on 12/7/21.
//

#pragma once

extern "C" {
#include "subsystems/thread/threadNetworkInfo.h"
}

#include <stdbool.h>

#define THREAD_SUBSYSTEM_NAME "thread"

/**
 * Retrieve current thread network information.
 *
 * @param info a pointer to an allocated ThreadNetworkInfo struct that will receive the information
 *
 * @return true on success
 */
bool threadSubsystemGetNetworkInfo(ThreadNetworkInfo *info);
