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
// Created by Thomas Lea on 7/29/15.
//

#ifndef FLEXCORE_DEVICEDESCRIPTORS_H
#define FLEXCORE_DEVICEDESCRIPTORS_H

#include <deviceDescriptor.h>
#include <icTypes/icHashMap.h>
#include <icTypes/icLinkedList.h>
#include <libxml/parser.h>

/*
 * Initialize the library and provide the path to where the device descriptor files will be located.
 */
void deviceDescriptorsInit(const char *allowListPath, const char *denyListPath);

/*
 * Release all resources used by the device descriptors library.
 */
void deviceDescriptorsCleanup();

/*
 * Retrieve the matching DeviceDescriptor for the provided input or NULL if a matching one doesnt exist.
 */
DeviceDescriptor *deviceDescriptorsGet(const char *manufacturer,
                                       const char *model,
                                       const char *hardwareVersion,
                                       const char *firmwareVersion);

/*
 * Retrieve the currently configured allowlist path.
 *
 * Caller frees
 */
char *getAllowListPath();

/*
 * Retrieve the currently configured denylist path.
 *
 * Caller frees
 */
char *getDenyListPath();

/*
 * Check whether a given white list is valid/parsable.  This function does NOT require deviceDescriptorsInit to be
 * called
 * @param allowListPath the allow list file to check
 * @return true if valid, false otherwise
 */
bool checkAllowListValid(const char *allowListPath);

/*
 * Check whether a given deny list is valid/parsable.  This function does NOT require deviceDescriptorsInit to be
 * called
 * @param denyListPath the deny list file to check
 * @return true if valid, false otherwise
 */
bool checkDenyListValid(const char *denyListPath);

#endif // FLEXCORE_DEVICEDESCRIPTORS_H
