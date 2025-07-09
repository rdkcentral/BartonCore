// ------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2025 Comcast Cable Communications Management, LLC
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
// ------------------------------ tabstop = 4 ----------------------------------

//
// Created by Raiyan Chowdhury on 3/27/2025.
//

#include "MockPropertyProvider.h"
#include "provider/barton-core-default-property-provider.h"
#include "provider/barton-core-property-provider.h"
#include "icTypes/icStringHashMap.h"

static GHashTable *properties; // hash table to simulate property storage

static BCorePropertyProvider *mockPropertyProvider = NULL;

BCorePropertyProvider *__wrap_deviceServiceConfigurationGetPropertyProvider()
{
    if (mockPropertyProvider == NULL)
    {
        mockPropertyProvider = (BCorePropertyProvider *) b_core_default_property_provider_new();
        g_assert(G_IS_OBJECT(mockPropertyProvider));
    }

    // Need to increment ref count because the unit under test uses g_autoptr, and we need this object to persist for
    // the duration of the test
    g_object_ref(mockPropertyProvider);
    return mockPropertyProvider;
}

bool __wrap_deviceServiceGetSystemProperty(const char *name, char **value)
{
    if (properties == NULL)
    {
        return false;
    }

    char *propertyValue = (char *) g_hash_table_lookup(properties, name);

    if (propertyValue == NULL)
    {
        return false;
    }

    *value = g_strdup(propertyValue);
    return true;
}

bool __wrap_deviceServiceSetSystemProperty(const char *name, const char *value)
{
    if (properties == NULL)
    {
        return false;
    }

    if (value == NULL)
    {
        g_hash_table_remove(properties, name);
    }
    else
    {
        g_hash_table_replace(properties, g_strdup(name), g_strdup(value));
    }

    return true;
}

bool __wrap_deviceServiceGetAllSystemProperties(icStringHashMap *map)
{
    return false;
}

void MockPropertyProviderInit()
{
    properties = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
}

void MockPropertyProviderReset()
{
    g_hash_table_destroy(properties);
    properties = NULL;

    if (mockPropertyProvider != NULL)
    {
        g_clear_object(&mockPropertyProvider);
    }
}

