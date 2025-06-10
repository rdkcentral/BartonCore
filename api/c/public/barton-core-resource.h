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
 * Created by Thomas Lea on 5/10/2024.
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define B_CORE_RESOURCE_TYPE (b_core_resource_get_type())
G_DECLARE_FINAL_TYPE(BCoreResource, b_core_resource, B_CORE, RESOURCE, GObject)

typedef enum
{
    B_CORE_RESOURCE_PROP_ID = 1,
    B_CORE_RESOURCE_PROP_URI,
    B_CORE_RESOURCE_PROP_ENDPOINT_ID,
    B_CORE_RESOURCE_PROP_DEVICE_UUID,
    B_CORE_RESOURCE_PROP_VALUE,
    B_CORE_RESOURCE_PROP_TYPE,
    B_CORE_RESOURCE_PROP_MODE,
    B_CORE_RESOURCE_PROP_CACHING_POLICY,
    B_CORE_RESOURCE_PROP_DATE_OF_LAST_SYNC_MILLIS,
    N_B_CORE_RESOURCE_PROPERTIES
} BCoreResourceProperty;

static const char *B_CORE_RESOURCE_PROPERTY_NAMES[] = {NULL,
                                                                 "id",
                                                                 "uri",
                                                                 "endpoint-id",
                                                                 "device-uuid",
                                                                 "value",
                                                                 "type",
                                                                 "mode",
                                                                 "caching-policy",
                                                                 "date-of-last-sync-millis"};

typedef enum
{
    B_CORE_RESOURCE_CACHING_POLICY_NEVER =
        1, // Never cache this attribute and always call the device driver to retrieve the value
    B_CORE_RESOURCE_CACHING_POLICY_ALWAYS // Always cache this attribute and never call the device driver to
                                                    // retrieve the value
} BCoreResourceCachingPolicy;

// This the old school way of creating GEnum types. We should be using G_DEFINE_ENUM_TYPE but that requires glib >=2.74
GType b_core_resource_caching_policy_get_type(void);
#define B_CORE_RESOURCE_CACHING_POLICY_TYPE (b_core_resource_caching_policy_get_type())


/**
 * b_core_resource_new
 *
 * @brief
 *
 * Returns: (transfer full): BCoreResource*
 */
BCoreResource *b_core_resource_new(void);

G_END_DECLS
