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
 * Created by Christian Leithner on 5/16/2024.
 */

#include "barton-core-metadata.h"
#include <glib.h>

typedef struct
{
    BCoreMetadata *metadata;
} BCoreMetadataTest;

// Setup function called before each test
static void setup(BCoreMetadataTest *test, gconstpointer user_data)
{
    test->metadata = b_core_metadata_new();
}

// Teardown function called after each test
static void teardown(BCoreMetadataTest *test, gconstpointer user_data)
{
    g_clear_object(&test->metadata);
}

// Test case to check if metadata is created successfully
static void test_metadata_creation(BCoreMetadataTest *test, gconstpointer user_data)
{
    g_assert_nonnull(test->metadata);
}

// Test case to check setting and getting properties
static void test_property_access(BCoreMetadataTest *test, gconstpointer user_data)
{
    // Set and get properties and check their values
    g_object_set(
        test->metadata, B_CORE_METADATA_PROPERTY_NAMES[B_CORE_METADATA_PROP_ID], "test-id", NULL);
    gchar *id = NULL;
    g_object_get(
        test->metadata, B_CORE_METADATA_PROPERTY_NAMES[B_CORE_METADATA_PROP_ID], &id, NULL);
    g_assert_cmpstr(id, ==, "test-id");
    g_free(id);

    g_object_set(
        test->metadata, B_CORE_METADATA_PROPERTY_NAMES[B_CORE_METADATA_PROP_URI], "test-uri", NULL);
    gchar *uri = NULL;
    g_object_get(
        test->metadata, B_CORE_METADATA_PROPERTY_NAMES[B_CORE_METADATA_PROP_URI], &uri, NULL);
    g_assert_cmpstr(uri, ==, "test-uri");
    g_free(uri);

    g_object_set(test->metadata,
                 B_CORE_METADATA_PROPERTY_NAMES[B_CORE_METADATA_PROP_ENDPOINT_ID],
                 "test-endpoint-id",
                 NULL);
    gchar *endpoint_id = NULL;
    g_object_get(test->metadata,
                 B_CORE_METADATA_PROPERTY_NAMES[B_CORE_METADATA_PROP_ENDPOINT_ID],
                 &endpoint_id,
                 NULL);
    g_assert_cmpstr(endpoint_id, ==, "test-endpoint-id");
    g_free(endpoint_id);

    g_object_set(test->metadata,
                 B_CORE_METADATA_PROPERTY_NAMES[B_CORE_METADATA_PROP_DEVICE_UUID],
                 "test-device-uuid",
                 NULL);
    gchar *device_uuid = NULL;
    g_object_get(test->metadata,
                 B_CORE_METADATA_PROPERTY_NAMES[B_CORE_METADATA_PROP_DEVICE_UUID],
                 &device_uuid,
                 NULL);
    g_assert_cmpstr(device_uuid, ==, "test-device-uuid");
    g_free(device_uuid);

    g_object_set(test->metadata,
                 B_CORE_METADATA_PROPERTY_NAMES[B_CORE_METADATA_PROP_VALUE],
                 "test-value",
                 NULL);
    gchar *value = NULL;
    g_object_get(
        test->metadata, B_CORE_METADATA_PROPERTY_NAMES[B_CORE_METADATA_PROP_VALUE], &value, NULL);
    g_assert_cmpstr(value, ==, "test-value");
    g_free(value);
}

int main(int argc, char *argv[])
{
    // Initialize GLib testing framework
    g_test_init(&argc, &argv, NULL);

    // Define test suite
    g_test_add("/barton-core-metadata/metadata-creation",
               BCoreMetadataTest,
               NULL,
               setup,
               test_metadata_creation,
               teardown);
    g_test_add("/barton-core-metadata/property-access",
               BCoreMetadataTest,
               NULL,
               setup,
               test_property_access,
               teardown);

    // Run tests
    return g_test_run();
}
