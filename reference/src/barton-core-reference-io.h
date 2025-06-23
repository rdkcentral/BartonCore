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
 * Created by Micah Koch on 11/08/24.
 */

#pragma once

#include <stdbool.h>

typedef bool (*processLineCallback)(const char *line, void *userData);

/**
 * Blocks waiting on input from the user and manages the prompt and other async output.  In order to keep the prompt
 * from getting mangled by output users should use emitOutput/emitError to print to the terminal.
 */
void barton_core_reference_io_process(bool useLinenoise, processLineCallback callback, void *userData);

/**
 * Emit some output to the terminal.  This will be displayed in a way that doesn't mangle the prompt.
 */
__attribute__((format(__printf__, 1, 2))) void emitOutput(const char *format, ...);

/**
 * Emit an error message to the terminal.  This will be displayed in a way that doesn't mangle the prompt.
 */
__attribute__((format(__printf__, 1, 2))) void emitError(const char *format, ...);
