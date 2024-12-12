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
// Created by Thomas Lea on 7/31/15.
//

#ifndef FLEXCORE_PARSER_H
#define FLEXCORE_PARSER_H

#include <icTypes/icLinkedList.h>
#include <icTypes/icStringHashMap.h>

/*
 * Parse the device descriptor list (aka allowlist) and any optional denylist at the provided paths
 * and return a list of device descriptors that are not explicitly denylisted.
 */
icLinkedList *parseDeviceDescriptors(const char *allowlistPath, const char *denylistPath);

/*
 * Allows just parsing a denylist, can be used for validating a denylist file is valid
 * @param denylistPath the path to the denylist
 * @return the denylisted uuids
 */
icStringHashMap *getDenylistedUuids(const char *denylistPath);

#endif // FLEXCORE_PARSER_H
