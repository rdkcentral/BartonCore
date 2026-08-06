//------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2026 Comcast Cable Communications Management, LLC
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
 * Shared implementation for the SBMD test harness: the recording call vectors and the
 * C API stubs (updateResource / setMetadata / deviceServiceSetMetadata) that the SBMD
 * result executor links against. Linked into every SBMD test executable.
 */

#include "SbmdDriverTestBase.h"

extern "C" {
#include <cjson/cJSON.h>
}

namespace barton::test
{
    std::vector<UpdateResourceCall> g_updateResourceCalls;
    std::vector<SetMetadataCall> g_setMetadataCalls;
    std::vector<SetPersistentDataCall> g_setPersistentDataCalls;
} // namespace barton::test

extern "C" {
void updateResource(const char *deviceUuid,
                    const char *endpointId,
                    const char *resourceId,
                    const char *newValue,
                    void *metadata)
{
    std::string metaStr;

    if (metadata != nullptr)
    {
        char *printed = cJSON_PrintUnformatted(static_cast<cJSON *>(metadata));

        if (printed != nullptr)
        {
            metaStr = printed;
            free(printed);
        }
    }

    barton::test::g_updateResourceCalls.push_back({deviceUuid ? deviceUuid : "",
                                                   endpointId ? endpointId : "",
                                                   resourceId ? resourceId : "",
                                                   newValue ? newValue : "",
                                                   metaStr});
}

void setMetadata(const char *deviceUuid, const char *endpointId, const char *name, const char *value)
{
    barton::test::g_setMetadataCalls.push_back(
        {deviceUuid ? deviceUuid : "", endpointId ? endpointId : "", name ? name : "", value ? value : ""});
}

bool deviceServiceSetMetadata(const char *uri, const char *value)
{
    barton::test::g_setPersistentDataCalls.push_back({uri ? uri : "", value ? value : ""});

    return true;
}
}
