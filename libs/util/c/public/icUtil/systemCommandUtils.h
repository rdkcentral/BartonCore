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

/*-----------------------------------------------
 * systemCommandUtils.h
 *
 * Utilities for system command
 *
 * Author: eInfochips
 *-----------------------------------------------*/

#pragma once

#define executeSystemCommand(a, ...) execSystemCommand(a, ##__VA_ARGS__, NULL)

/**
 * Run a shell command (i.e., system) and return the exit status code
 * @param commandStr Any valid bourne shell command
 * @note Avoid using this if any arguments are provided by externally
 *       controlled sources (filenames, URLs, user input, etc.)
 * @return exit status code
 */
int execShellCommand(const char *commandStr);

/**
 * Fork and run a command with arguments and return the exit status code
 * @param program The program to run
 * @param ... Arguments (if any)
 * @return the exit status code
 */
int execSystemCommand(const char *program, ...);

/**
 * Fork and run a command
 * @param path path to executable
 * @param argv arguments (c strings) with NULL terminator
 * @return program exit status code, or -1 on pre-invocation error
 */
int __executeSystemCommand(const char *path, char *const argv[]);
