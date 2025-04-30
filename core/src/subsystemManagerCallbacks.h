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
// Created by mkoch201 on 12/5/18.
//

#pragma once

/**
 * Callback for notifying subsystem manager that the subsystem
 * is up and ready for normal operations.
 */
typedef void (*subsystemInitializedFunc)(const char *subsystemName);

/**
 * Callback for notifying subsystem manager that the subsystem
 * is no longer ready.
 */
typedef void (*subsystemDeInitializedFunc)(const char *subsystemName);
