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
// Created by Raiyan Chowdhury on 3/28/2025.
//

#include "device-service-mock-property-provider.h"
#include "glib.h"

struct _BDeviceServiceMockPropertyProvider
{
    GObject parent_instance;
    GHashTable *properties; // hash table to simulate property storage
};

static void b_device_service_mock_property_provider_interface_init(BDeviceServicePropertyProviderInterface *iface);

G_DEFINE_TYPE_WITH_CODE(BDeviceServiceMockPropertyProvider,
                         b_device_service_mock_property_provider,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE(B_DEVICE_SERVICE_PROPERTY_PROVIDER_TYPE,
                            b_device_service_mock_property_provider_interface_init))

static void b_device_service_mock_property_provider_init(BDeviceServiceMockPropertyProvider *self)
{
    self->properties = g_hash_table_new(g_str_hash, g_str_equal);
}

static void b_device_service_mock_property_provider_finalize(GObject *object)
{
    BDeviceServiceMockPropertyProvider *self = B_DEVICE_SERVICE_MOCK_PROPERTY_PROVIDER(object);

    g_hash_table_destroy(self->properties);
    self->properties = NULL;

    G_OBJECT_CLASS(b_device_service_mock_property_provider_parent_class)->finalize(object);
}

static void b_device_service_mock_property_provider_class_init(BDeviceServiceMockPropertyProviderClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->finalize = b_device_service_mock_property_provider_finalize;
}

static gchar *mock_get_property(BDeviceServicePropertyProvider *self,
                                     const gchar *property_name,
                                     GError **error)
{
    BDeviceServiceMockPropertyProvider *mock_self = B_DEVICE_SERVICE_MOCK_PROPERTY_PROVIDER(self);
    g_return_val_if_fail(mock_self->properties != NULL, NULL);

    gchar *value = g_hash_table_lookup(mock_self->properties, property_name);
    if (value == NULL)
    {
        g_set_error(error,
                    B_DEVICE_SERVICE_PROPERTY_PROVIDER_ERROR,
                    B_DEVICE_SERVICE_PROPERTY_PROVIDER_ERROR_NOT_FOUND,
                    "Property '%s' not found",
                    property_name);
        return NULL;
    }

    return g_strdup(value);
}

static gboolean mock_set_property(BDeviceServicePropertyProvider *self,
                                     const gchar *property_name,
                                     const gchar *property_value,
                                     GError **error)
{
    BDeviceServiceMockPropertyProvider *mock_self = B_DEVICE_SERVICE_MOCK_PROPERTY_PROVIDER(self);
    g_return_val_if_fail(mock_self->properties != NULL, FALSE);

    if (property_value == NULL)
    {
        g_hash_table_remove(mock_self->properties, property_name);
    }
    else
    {
        g_hash_table_replace(mock_self->properties,
                             g_strdup(property_name),
                             g_strdup(property_value));
    }

    return TRUE;
}

BDeviceServiceMockPropertyProvider *b_device_service_mock_property_provider_new(void)
{
    return g_object_new(B_DEVICE_SERVICE_MOCK_PROPERTY_PROVIDER_TYPE, NULL);
}

static void b_device_service_mock_property_provider_interface_init(BDeviceServicePropertyProviderInterface *iface)
{
    iface->get_property = mock_get_property;
    iface->set_property = mock_set_property;
}
