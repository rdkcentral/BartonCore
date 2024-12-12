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

#include "resourceContainer.h"
#include <icLog/logging.h>
#include <string.h>

#define LOG_TAG "resourceContainer"

int findEnumForLabel(const char *needle, const char *const haystack[], size_t haystackLength)
{
    int out = INVALID_ENUM;

    if (needle == NULL || haystack == NULL)
    {
        return out;
    }

    for (int i = 0; i < haystackLength; i++)
    {
        if (strcmp(haystack[i], needle) == 0)
        {
            out = i;
            break;
        }
    }

    if (out == INVALID_ENUM)
    {
        icLogWarn(LOG_TAG, "No valid enum value found for %s", needle);
    }

    return out;
}
