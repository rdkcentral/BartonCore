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
 * Created by Christian Leithner on 8/29/2024.
 */

#include "provider/device-service-property-provider.h"
#include "deviceService.h"
#include "icLog/logging.h"
#include "icTypes/icStringHashMap.h"
#include "inttypes.h"
#include <errno.h>

#define LOG_TAG     "device-service-property-provider"
#define logFmt(fmt) "%s: " fmt, __func__

G_DEFINE_INTERFACE(BDeviceServicePropertyProvider, b_device_service_property_provider, G_TYPE_OBJECT)
G_DEFINE_QUARK(b - device - service - property - provider - error - quark, b_device_service_property_provider_error)

enum DEVICE_SERVICE_PROPERTY_PROVIDER_SIGNALS
{
    SIGNAL_PROPERTY_CHANGED,

    SIGNAL_MAX
};

static guint signals[SIGNAL_MAX];

static gchar *b_device_service_property_provider_get_internal_property(const gchar *property_name, GError **error);
static gboolean b_device_service_property_provider_set_internal_property(BDeviceServicePropertyProvider *self,
                                                                         const gchar *property_name,
                                                                         const gchar *property_value,
                                                                         GError **error);

static void b_device_service_propert_provider_emit_property_changed(BDeviceServicePropertyProvider *self,
                                                                    const gchar *property_name,
                                                                    const gchar *old_value,
                                                                    const gchar *new_value);


static void b_device_service_property_provider_default_init(BDeviceServicePropertyProviderInterface *iface)
{
    iface->emit_property_changed = b_device_service_propert_provider_emit_property_changed;
    iface->get_property = NULL;
    iface->set_property = NULL;

    signals[SIGNAL_PROPERTY_CHANGED] = g_signal_new(B_DEVICE_SERVICE_PROPERTY_PROVIDER_SIGNAL_PROPERTY_CHANGED,
                                                    G_TYPE_FROM_INTERFACE(iface),
                                                    G_SIGNAL_RUN_LAST,
                                                    0,
                                                    NULL,
                                                    NULL,
                                                    NULL,
                                                    G_TYPE_NONE,
                                                    3,
                                                    G_TYPE_STRING,
                                                    G_TYPE_STRING,
                                                    G_TYPE_STRING);
}

gchar *b_device_service_property_provider_get_property(BDeviceServicePropertyProvider *self,
                                                       const gchar *property_name,
                                                       GError **error)
{
    g_return_val_if_fail(B_DEVICE_SERVICE_IS_PROPERTY_PROVIDER(self), NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    if (property_name == NULL)
    {
        g_set_error(error,
                    B_DEVICE_SERVICE_PROPERTY_PROVIDER_ERROR,
                    B_DEVICE_SERVICE_PROPERTY_PROVIDER_ERROR_ARG,
                    "Property name cannot be NULL");
        return NULL;
    }

    BDeviceServicePropertyProviderInterface *iface = B_DEVICE_SERVICE_PROPERTY_PROVIDER_GET_IFACE(self);

    GError *tmp_error = NULL;
    g_autofree gchar *property_value = NULL;
    if (iface->get_property)
    {
        property_value = iface->get_property(self, property_name, &tmp_error);
    }

    if (tmp_error)
    {
        if (tmp_error->code == B_DEVICE_SERVICE_PROPERTY_PROVIDER_ERROR_NOT_FOUND)
        {
            // Try internal property database and propogate that error instead.
            g_clear_error(&tmp_error);
        }
        else
        {
            g_propagate_error(error, g_steal_pointer(&tmp_error));
            return NULL;
        }
    }

    if (property_value == NULL)
    {
        property_value = b_device_service_property_provider_get_internal_property(property_name, &tmp_error);
        if (tmp_error)
        {
            g_propagate_error(error, g_steal_pointer(&tmp_error));
            return NULL;
        }
    }

    return g_steal_pointer(&property_value);
}

/**
 * b_device_service_property_provider_get_internal_property:
 * @property_name: The name of the property to get.
 * @error: A #GError to store the error information.
 *
 * Attempt to get the value of the interal property with the given name.
 *
 * Returns: (nullable) (transfer full): The value of the property or %NULL if an error occurred.
 */
static gchar *b_device_service_property_provider_get_internal_property(const gchar *property_name, GError **error)
{
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    if (property_name == NULL)
    {
        g_set_error(error,
                    B_DEVICE_SERVICE_PROPERTY_PROVIDER_ERROR,
                    B_DEVICE_SERVICE_PROPERTY_PROVIDER_ERROR_ARG,
                    "Property name cannot be NULL");
        return NULL;
    }

    g_autofree gchar *property_value = NULL;
    if (!deviceServiceGetSystemProperty(property_name, &property_value))
    {
        g_set_error(error,
                    B_DEVICE_SERVICE_PROPERTY_PROVIDER_ERROR,
                    B_DEVICE_SERVICE_PROPERTY_PROVIDER_ERROR_NOT_FOUND,
                    "Failed to find property");
        return NULL;
    }

    return g_steal_pointer(&property_value);
}

gchar *b_device_service_property_provider_get_property_as_string(BDeviceServicePropertyProvider *self,
                                                                 const gchar *property_name,
                                                                 const gchar *default_value)
{
    g_return_val_if_fail(B_DEVICE_SERVICE_IS_PROPERTY_PROVIDER(self), NULL);

    g_autoptr(GError) error = NULL;
    g_autofree gchar *property_value = b_device_service_property_provider_get_property(self, property_name, &error);
    if (error)
    {
        icWarn("Error getting property '%s': %s", stringCoalesce(property_name), error->message);
        return g_strdup(default_value);
    }

    return g_steal_pointer(&property_value);
}

gboolean b_device_service_property_provider_get_property_as_bool(BDeviceServicePropertyProvider *self,
                                                                 const gchar *property_name,
                                                                 gboolean default_value)
{
    g_return_val_if_fail(B_DEVICE_SERVICE_IS_PROPERTY_PROVIDER(self), default_value);

    g_autoptr(GError) error = NULL;
    g_autofree gchar *property_value = b_device_service_property_provider_get_property(self, property_name, &error);
    if (error)
    {
        icWarn("Error getting property '%s': %s", stringCoalesce(property_name), error->message);
        return default_value;
    }

    return g_ascii_strcasecmp(property_value, "true") == 0;
}

gint8 b_device_service_property_provider_get_property_as_int8(BDeviceServicePropertyProvider *self,
                                                              const gchar *property_name,
                                                              gint8 default_value)
{
    g_return_val_if_fail(B_DEVICE_SERVICE_IS_PROPERTY_PROVIDER(self), default_value);

    gint64 value = b_device_service_property_provider_get_property_as_int64(self, property_name, default_value);
    if (value < G_MININT8 || value > G_MAXINT8)
    {
        icWarn("Value out of range %" PRIi64, value);
        return default_value;
    }

    return (gint8) value;
}

gint16 b_device_service_property_provider_get_property_as_int16(BDeviceServicePropertyProvider *self,
                                                                const gchar *property_name,
                                                                gint16 default_value)
{
    g_return_val_if_fail(B_DEVICE_SERVICE_IS_PROPERTY_PROVIDER(self), default_value);

    gint64 value = b_device_service_property_provider_get_property_as_int64(self, property_name, default_value);
    if (value < G_MININT16 || value > G_MAXINT16)
    {
        icWarn("Value out of range %" PRIi64, value);
        return default_value;
    }

    return (gint16) value;
}

gint32 b_device_service_property_provider_get_property_as_int32(BDeviceServicePropertyProvider *self,
                                                                const gchar *property_name,
                                                                gint32 default_value)
{
    g_return_val_if_fail(B_DEVICE_SERVICE_IS_PROPERTY_PROVIDER(self), default_value);

    gint64 value = b_device_service_property_provider_get_property_as_int64(self, property_name, default_value);
    if (value < G_MININT32 || value > G_MAXINT32)
    {
        icWarn("Value out of range %" PRIi64, value);
        return default_value;
    }

    return (gint32) value;
}

gint64 b_device_service_property_provider_get_property_as_int64(BDeviceServicePropertyProvider *self,
                                                                const gchar *property_name,
                                                                gint64 default_value)
{
    g_return_val_if_fail(B_DEVICE_SERVICE_IS_PROPERTY_PROVIDER(self), default_value);

    g_autoptr(GError) error = NULL;
    g_autofree gchar *property_value = b_device_service_property_provider_get_property(self, property_name, &error);
    if (error)
    {
        icWarn("Error getting property '%s': %s", stringCoalesce(property_name), error->message);
        return default_value;
    }

    errno = 0;
    gchar *endptr = NULL;
    gint64 retVal = g_ascii_strtoll(property_value, &endptr, 10);
    if (errno > 0 || (retVal == 0 && endptr == property_value))
    {
        icWarn("Error converting property value \"%s\" to int64: %s",
               property_value,
               errno > 0 ? g_strerror(errno) : "Not a number");
        return default_value;
    }

    return retVal;
}

guint8 b_device_service_property_provider_get_property_as_uint8(BDeviceServicePropertyProvider *self,
                                                                const gchar *property_name,
                                                                guint8 default_value)
{
    g_return_val_if_fail(B_DEVICE_SERVICE_IS_PROPERTY_PROVIDER(self), default_value);

    guint64 value = b_device_service_property_provider_get_property_as_uint64(self, property_name, default_value);
    if (value > G_MAXUINT8)
    {
        icWarn("Value out of range %" PRIu64, value);
        return default_value;
    }

    return (guint8) value;
}

guint16 b_device_service_property_provider_get_property_as_uint16(BDeviceServicePropertyProvider *self,
                                                                  const gchar *property_name,
                                                                  guint16 default_value)
{
    g_return_val_if_fail(B_DEVICE_SERVICE_IS_PROPERTY_PROVIDER(self), default_value);

    guint64 value = b_device_service_property_provider_get_property_as_uint64(self, property_name, default_value);
    if (value > G_MAXUINT16)
    {
        icWarn("Value out of range %" PRIu64, value);
        return default_value;
    }

    return (guint16) value;
}

guint32 b_device_service_property_provider_get_property_as_uint32(BDeviceServicePropertyProvider *self,
                                                                  const gchar *property_name,
                                                                  guint32 default_value)
{
    g_return_val_if_fail(B_DEVICE_SERVICE_IS_PROPERTY_PROVIDER(self), default_value);

    guint64 value = b_device_service_property_provider_get_property_as_uint64(self, property_name, default_value);
    if (value > G_MAXUINT32)
    {
        icWarn("Value out of range %" PRIu64, value);
        return default_value;
    }

    return (guint32) value;
}

guint64 b_device_service_property_provider_get_property_as_uint64(BDeviceServicePropertyProvider *self,
                                                                  const gchar *property_name,
                                                                  guint64 default_value)
{
    g_return_val_if_fail(B_DEVICE_SERVICE_IS_PROPERTY_PROVIDER(self), default_value);

    g_autoptr(GError) error = NULL;
    g_autofree gchar *property_value = b_device_service_property_provider_get_property(self, property_name, &error);
    if (error)
    {
        icWarn("Error getting property '%s': %s", stringCoalesce(property_name), error->message);
        return default_value;
    }

    // g_ascii_strtoull readily accepts negative values, so check that by hand.
    if (property_value && property_value[0] == '-')
    {
        icWarn("Value is negative: %s", property_value);
        return default_value;
    }

    errno = 0;
    gchar *endptr = NULL;
    guint64 retVal = g_ascii_strtoull(property_value, &endptr, 0);
    if (errno > 0 || (retVal == 0 && endptr == property_value))
    {
        icWarn("Error converting property value \"%s\" to uint64: %s",
               property_value,
               errno > 0 ? g_strerror(errno) : "Not a number");
        return default_value;
    }

    return retVal;
}

GHashTable *b_device_service_property_provider_get_all_properties(BDeviceServicePropertyProvider *self)
{
    g_return_val_if_fail(B_DEVICE_SERVICE_IS_PROPERTY_PROVIDER(self), NULL);

    GHashTable *properties = NULL;

    BDeviceServicePropertyProviderInterface *iface = B_DEVICE_SERVICE_PROPERTY_PROVIDER_GET_IFACE(self);
    if (iface->get_all_properties)
    {
        properties = iface->get_all_properties(self);
    }
    else
    {
        properties = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    }

    scoped_icStringHashMap *internal_properties = stringHashMapCreate();
    deviceServiceGetAllSystemProperties(internal_properties);
    scoped_icStringHashMapIterator *iter = stringHashMapIteratorCreate(internal_properties);
    char *key;
    char *value;
    while (stringHashMapIteratorHasNext(iter))
    {
        stringHashMapIteratorGetNext(iter, &key, &value);
        if (key && value)
        {
            g_hash_table_insert(properties, g_strdup(key), g_strdup(value));
        }
    }

    return properties;
}

gboolean b_device_service_property_provider_set_property(BDeviceServicePropertyProvider *self,
                                                         const gchar *property_name,
                                                         const gchar *property_value,
                                                         GError **error)
{
    g_return_val_if_fail(B_DEVICE_SERVICE_IS_PROPERTY_PROVIDER(self), FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if (property_name == NULL)
    {
        g_set_error(error,
                    B_DEVICE_SERVICE_PROPERTY_PROVIDER_ERROR,
                    B_DEVICE_SERVICE_PROPERTY_PROVIDER_ERROR_ARG,
                    "Property name cannot be NULL");
        return FALSE;
    }

    BDeviceServicePropertyProviderInterface *iface = B_DEVICE_SERVICE_PROPERTY_PROVIDER_GET_IFACE(self);

    if (iface->set_property)
    {
        return iface->set_property(self, property_name, property_value, error);
    }

    return b_device_service_property_provider_set_internal_property(self, property_name, property_value, error);
}


/**
 * b_device_service_property_provider_set_internal_property:
 * @self: The property provider to set the property on.
 * @property_name: The name of the property to set.
 * @property_value: The value to set the property to.
 * @error: A #GError to store the error information.
 *
 * Attempt to set the value of the interal property with the given name.
 * If successful, emits the "property-changed" signal.
 *
 * Returns: %TRUE if the property was set successfully, %FALSE otherwise.
 */
static gboolean b_device_service_property_provider_set_internal_property(BDeviceServicePropertyProvider *self,
                                                                         const gchar *property_name,
                                                                         const gchar *property_value,
                                                                         GError **error)
{
    g_return_val_if_fail(B_DEVICE_SERVICE_IS_PROPERTY_PROVIDER(self), FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if (property_name == NULL)
    {
        g_set_error(error,
                    B_DEVICE_SERVICE_PROPERTY_PROVIDER_ERROR,
                    B_DEVICE_SERVICE_PROPERTY_PROVIDER_ERROR_ARG,
                    "Property name cannot be NULL");
        return FALSE;
    }

    // Try ` get the old value. If it errors because of anything that isn't "not found", we'll short-circuit
    // and fail the set call.
    g_autoptr(GError) tmp_error = NULL;
    g_autofree gchar *old_value = b_device_service_property_provider_get_property(self, property_name, &tmp_error);
    if (tmp_error)
    {
        if (tmp_error->code != B_DEVICE_SERVICE_PROPERTY_PROVIDER_ERROR_NOT_FOUND)
        {
            icWarn("Failed to get old value of property: %s", tmp_error->message);
            g_propagate_error(error, g_steal_pointer(&tmp_error));
            return FALSE;
        }
    }

    if (!deviceServiceSetSystemProperty(property_name, property_value))
    {
        g_set_error(error,
                    B_DEVICE_SERVICE_PROPERTY_PROVIDER_ERROR,
                    B_DEVICE_SERVICE_PROPERTY_PROVIDER_ERROR_INTERNAL,
                    "Failed to set property");
        return FALSE;
    }
    else
    {
        b_device_service_propert_provider_emit_property_changed(self, property_name, old_value, property_value);
    }

    return TRUE;
}

gboolean b_device_service_property_provider_set_property_string(BDeviceServicePropertyProvider *self,
                                                                const gchar *property_name,
                                                                const gchar *property_value)
{
    g_return_val_if_fail(B_DEVICE_SERVICE_IS_PROPERTY_PROVIDER(self), FALSE);

    g_autoptr(GError) error = NULL;
    if (!b_device_service_property_provider_set_property(self, property_name, property_value, &error))
    {
        icWarn("Error setting property: %s", error->message);
        return FALSE;
    }

    return TRUE;
}

gboolean b_device_service_property_provider_set_property_bool(BDeviceServicePropertyProvider *self,
                                                              const gchar *property_name,
                                                              gboolean property_value)
{
    return b_device_service_property_provider_set_property_string(
        self, property_name, property_value ? "true" : "false");
}

gboolean b_device_service_property_provider_set_property_int8(BDeviceServicePropertyProvider *self,
                                                              const gchar *property_name,
                                                              gint8 property_value)
{
    return b_device_service_property_provider_set_property_int64(self, property_name, property_value);
}

gboolean b_device_service_property_provider_set_property_int16(BDeviceServicePropertyProvider *self,
                                                               const gchar *property_name,
                                                               gint16 property_value)
{
    return b_device_service_property_provider_set_property_int64(self, property_name, property_value);
}

gboolean b_device_service_property_provider_set_property_int32(BDeviceServicePropertyProvider *self,
                                                               const gchar *property_name,
                                                               gint32 property_value)
{
    return b_device_service_property_provider_set_property_int64(self, property_name, property_value);
}

gboolean b_device_service_property_provider_set_property_int64(BDeviceServicePropertyProvider *self,
                                                               const gchar *property_name,
                                                               gint64 property_value)
{
    g_autofree gchar *value = g_strdup_printf("%" PRIi64, property_value);
    return b_device_service_property_provider_set_property_string(self, property_name, value);
}

gboolean b_device_service_property_provider_set_property_uint8(BDeviceServicePropertyProvider *self,
                                                               const gchar *property_name,
                                                               guint8 property_value)
{
    return b_device_service_property_provider_set_property_uint64(self, property_name, property_value);
}

gboolean b_device_service_property_provider_set_property_uint16(BDeviceServicePropertyProvider *self,
                                                                const gchar *property_name,
                                                                guint16 property_value)
{
    return b_device_service_property_provider_set_property_uint64(self, property_name, property_value);
}

gboolean b_device_service_property_provider_set_property_uint32(BDeviceServicePropertyProvider *self,
                                                                const gchar *property_name,
                                                                guint32 property_value)
{
    return b_device_service_property_provider_set_property_uint64(self, property_name, property_value);
}

gboolean b_device_service_property_provider_set_property_uint64(BDeviceServicePropertyProvider *self,
                                                                const gchar *property_name,
                                                                guint64 property_value)
{
    g_autofree gchar *value = g_strdup_printf("%" PRIu64, property_value);
    return b_device_service_property_provider_set_property_string(self, property_name, value);
}

gboolean b_device_service_property_provider_has_property(BDeviceServicePropertyProvider *self,
                                                         const gchar *property_name)
{
    g_return_val_if_fail(B_DEVICE_SERVICE_IS_PROPERTY_PROVIDER(self), FALSE);

    g_autoptr(GError) error = NULL;
    g_autofree gchar *property_value = b_device_service_property_provider_get_property(self, property_name, &error);
    return property_value != NULL && error == NULL;
}

static void b_device_service_propert_provider_emit_property_changed(BDeviceServicePropertyProvider *self,
                                                                    const gchar *property_name,
                                                                    const gchar *old_value,
                                                                    const gchar *new_value)
{
    g_signal_emit(self, signals[SIGNAL_PROPERTY_CHANGED], 0, property_name, old_value, new_value);
}
