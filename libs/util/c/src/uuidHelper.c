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
// Created by mkoch201 on 8/6/20.
//

#include <stdlib.h>
#include <uuid/uuid.h>

/**
 * Generate a new 128 bit uuid and return it in string form (xxxxxxxx-xxxx-Mxxx-Nxxx-xxxxxxxxxxxx)
 * @return the uuid string, caller must free
 */
char *generateUUIDString(void)
{
    uuid_t uuid = {'\0'};
    uuid_generate(uuid);

    // Example (36 characters plus null): CBDE8461-824E-4ADD-87D5-5FD6024685A5
    char *buffer = (char *) calloc(1, 37);
    uuid_unparse_lower(uuid, buffer);

    return buffer;
}
