//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2024 Comcast
//
// All rights reserved.
//
// This software is protected by copyright laws of the United States
// and of foreign countries. This material may also be protected by
// patent laws of the United States and of foreign countries.
//
// This software is furnished under a license agreement and/or a
// nondisclosure agreement and may only be used or copied in accordance
// with the terms of those agreements.
//
// The mere transfer of this software does not imply any licenses of trade
// secrets, proprietary technology, copyrights, patents, trademarks, or
// any other form of intellectual property whatsoever.
//
// Comcast retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------

/*
 * Created by Christian Leithner on 8/29/2024.
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define B_DEVICE_SERVICE_PROPERTY_PROVIDER_TYPE (b_device_service_property_provider_get_type())
G_DECLARE_INTERFACE(BDeviceServicePropertyProvider,
                    b_device_service_property_provider,
                    B_DEVICE_SERVICE,
                    PROPERTY_PROVIDER,
                    GObject);

#define B_DEVICE_SERVICE_PROPERTY_PROVIDER_ERROR (b_device_service_property_provider_error_quark())

typedef enum
{
    B_DEVICE_SERVICE_PROPERTY_PROVIDER_ERROR_GENERIC,   // A catch-all error for clients to use.
    B_DEVICE_SERVICE_PROPERTY_PROVIDER_ERROR_ARG,       // An argument was invalid.
    B_DEVICE_SERVICE_PROPERTY_PROVIDER_ERROR_NOT_FOUND, // The requested property was not found.
    B_DEVICE_SERVICE_PROPERTY_PROVIDER_ERROR_INTERNAL,  // An error occurred with internal properties.
} BDeviceServicePropertyProviderError;

GQuark b_device_service_property_provider_error_quark(void);

struct _BDeviceServicePropertyProviderInterface
{
    GTypeInterface parent_iface;

    // Signals
    void (*emit_property_changed)(BDeviceServicePropertyProvider *self,
                                  const gchar *property_name,
                                  const gchar *old_value,
                                  const gchar *new_value);

    gchar *(*get_property)(BDeviceServicePropertyProvider *self, const gchar *property_name, GError **error);
    GHashTable *(*get_all_properties)(BDeviceServicePropertyProvider *self);
    gboolean (*set_property)(BDeviceServicePropertyProvider *self,
                             const gchar *property_name,
                             const gchar *property_value,
                             GError **error);
};


#define B_DEVICE_SERVICE_PROPERTY_PROVIDER_SIGNAL_PROPERTY_CHANGED "property-changed"
// signal handler args: gchar *property_name, gchar *old_property_value, gchar *new_property_value


/**
 * b_device_service_property_provider_get_property:
 * @self: The #BDeviceServicePropertyProvider instance.
 * @property_name: The name of the property to get.
 * @error: A #GError to store the error information.
 *
 * Attempt to get the value of the property with the given name. Will first call an implementation of the get_property
 * interface method. If it is unable to locate the property, it will then check internal device service properties.
 *
 * Returns: (nullable) (transfer full): The value of the property or %NULL if an error occurred.
 */
gchar *b_device_service_property_provider_get_property(BDeviceServicePropertyProvider *self,
                                                       const gchar *property_name,
                                                       GError **error);

/**
 * b_device_service_property_provider_get_property_as_string:
 * @self: The #BDeviceServicePropertyProvider instance.
 * @property_name: The name of the property to get.
 * @default_value: The default value to return if an error is encountered.
 *
 * Returns: (nullable) (transfer full): The value of the property as a string or the default value if an error occurred.
 */
gchar *b_device_service_property_provider_get_property_as_string(BDeviceServicePropertyProvider *self,
                                                                 const gchar *property_name,
                                                                 const gchar *default_value);

/**
 * b_device_service_property_provider_get_property_as_bool:
 * @self: The #BDeviceServicePropertyProvider instance.
 * @property_name: The name of the property to get.
 * @default_value: The default value to return if an error is encountered.
 *
 * Returns: The #TRUE if the value of the property is "true", #FALSE otherwise, or the default value if an error
 * occurred.
 */
gboolean b_device_service_property_provider_get_property_as_bool(BDeviceServicePropertyProvider *self,
                                                                 const gchar *property_name,
                                                                 gboolean default_value);

/**
 * b_device_service_property_provider_get_property_as_int8:
 * @self: The #BDeviceServicePropertyProvider instance.
 * @property_name: The name of the property to get.
 * @default_value: The default value to return if an error is encountered.
 *
 * Returns: The value of the property as an int8 or the default value if an error occurred.
 */
gint8 b_device_service_property_provider_get_property_as_int8(BDeviceServicePropertyProvider *self,
                                                              const gchar *property_name,
                                                              gint8 default_value);

/**
 * b_device_service_property_provider_get_property_as_int16:
 * @self: The #BDeviceServicePropertyProvider instance.
 * @property_name: The name of the property to get.
 * @default_value: The default value to return if an error is encountered.
 *
 * Returns: The value of the property as an int16 or the default value if an error occurred.
 */
gint16 b_device_service_property_provider_get_property_as_int16(BDeviceServicePropertyProvider *self,
                                                                const gchar *property_name,
                                                                gint16 default_value);

/**
 * b_device_service_property_provider_get_property_as_int32:
 * @self: The #BDeviceServicePropertyProvider instance.
 * @property_name: The name of the property to get.
 * @default_value: The default value to return if an error is encountered.
 *
 * Returns: The value of the property as an int32 or the default value if an error occurred.
 */
gint32 b_device_service_property_provider_get_property_as_int32(BDeviceServicePropertyProvider *self,
                                                                const gchar *property_name,
                                                                gint32 default_value);

/**
 * b_device_service_property_provider_get_property_as_int64:
 * @self: The #BDeviceServicePropertyProvider instance.
 * @property_name: The name of the property to get.
 * @default_value: The default value to return if an error is encountered.
 *
 * Returns: The value of the property as an int64 or the default value if an error occurred.
 */
gint64 b_device_service_property_provider_get_property_as_int64(BDeviceServicePropertyProvider *self,
                                                                const gchar *property_name,
                                                                gint64 default_value);

/**
 * b_device_service_property_provider_get_property_as_uint8:
 * @self: The #BDeviceServicePropertyProvider instance.
 * @property_name: The name of the property to get.
 * @default_value: The default value to return if an error is encountered.
 *
 * Returns: The value of the property as a uint8 or the default value if an error occurred.
 */
guint8 b_device_service_property_provider_get_property_as_uint8(BDeviceServicePropertyProvider *self,
                                                                const gchar *property_name,
                                                                guint8 default_value);

/**
 * b_device_service_property_provider_get_property_as_uint16:
 * @self: The #BDeviceServicePropertyProvider instance.
 * @property_name: The name of the property to get.
 * @default_value: The default value to return if an error is encountered.
 *
 * Returns: The value of the property as a uint16 or the default value if an error occurred.
 */
guint16 b_device_service_property_provider_get_property_as_uint16(BDeviceServicePropertyProvider *self,
                                                                  const gchar *property_name,
                                                                  guint16 default_value);

/**
 * b_device_service_property_provider_get_property_as_uint32:
 * @self: The #BDeviceServicePropertyProvider instance.
 * @property_name: The name of the property to get.
 * @default_value: The default value to return if an error is encountered.
 *
 * Returns: The value of the property as a uint32 or the default value if an error occurred.
 */
guint32 b_device_service_property_provider_get_property_as_uint32(BDeviceServicePropertyProvider *self,
                                                                  const gchar *property_name,
                                                                  guint32 default_value);

/**
 * b_device_service_property_provider_get_property_as_uint64:
 * @self: The #BDeviceServicePropertyProvider instance.
 * @property_name: The name of the property to get.
 * @default_value: The default value to return if an error is encountered.
 *
 * Returns: The value of the property as a uint64 or the default value if an error occurred.
 */
guint64 b_device_service_property_provider_get_property_as_uint64(BDeviceServicePropertyProvider *self,
                                                                  const gchar *property_name,
                                                                  guint64 default_value);

/**
 * b_device_service_property_provider_get_all_properties:
 * @self: The #BDeviceServicePropertyProvider instance.
 *
 * Get all known properties. This does include internal properties.
 *
 * Returns: (transfer full): A #GHashTable containing all properties.
 */
GHashTable *b_device_service_property_provider_get_all_properties(BDeviceServicePropertyProvider *self);

/**
 * b_device_service_property_provider_set_property:
 * @self: The #BDeviceServicePropertyProvider instance.
 * @property_name: The name of the property to set.
 * @property_value: The value to set the property to.
 * @error: A #GError to store the error information.
 *
 * Attempt to set the value of the property with the given name. Will first call an implementation of the set_property
 * interface method. If one does not exist, it will then attempt to set the property in internal device service
 * properties.
 *
 * If the property is successfully set IN INTERNAL PROPERTIES, a "property-changed" signal will be emitted.
 * It is the responsibility of implementations to emit this signal (#emit_property_changed) when a property is set in
 * the client implementation. This enables clients to support external property sources/services where a property change
 * may be caused by a different source than the device service GObject interface.
 *
 * @see B_DEVICE_SERVICE_PROPERTY_PROVIDER_SIGNAL_PROPERTY_CHANGED
 * @see emit_property_changed
 *
 * Returns: %TRUE if the property was set successfully, %FALSE otherwise.
 */
gboolean b_device_service_property_provider_set_property(BDeviceServicePropertyProvider *self,
                                                         const gchar *property_name,
                                                         const gchar *property_value,
                                                         GError **error);

/**
 * b_device_service_property_provider_set_property_as_string:
 * @self: The #BDeviceServicePropertyProvider instance.
 * @property_name: The name of the property to set.
 * @property_value: The value to set the property to.
 *
 * Calls b_device_service_property_provider_set_property and does not
 * propagate errors.
 * Simple convenience function if you do not care about error handling.
 *
 * Returns: %TRUE if the property was set successfully, %FALSE otherwise.
 */
gboolean b_device_service_property_provider_set_property_string(BDeviceServicePropertyProvider *self,
                                                                const gchar *property_name,
                                                                const gchar *property_value);

/**
 * b_device_service_property_provider_set_property_bool:
 * @self: The #BDeviceServicePropertyProvider instance.
 * @property_name: The name of the property to set.
 * @property_value: The value to set the property to.
 *
 * Calls b_device_service_property_provider_set_property and does not
 * propagate errors.
 *
 * Returns: %TRUE if the property was set successfully, %FALSE otherwise.
 */
gboolean b_device_service_property_provider_set_property_bool(BDeviceServicePropertyProvider *self,
                                                              const gchar *property_name,
                                                              gboolean property_value);

/**
 * b_device_service_property_provider_set_property_int8:
 * @self: The #BDeviceServicePropertyProvider instance.
 * @property_name: The name of the property to set.
 * @property_value: The value to set the property to.
 *
 * Calls b_device_service_property_provider_set_property and does not
 * propagate errors.
 *
 * Returns: %TRUE if the property was set successfully, %FALSE otherwise.
 */
gboolean b_device_service_property_provider_set_property_int8(BDeviceServicePropertyProvider *self,
                                                              const gchar *property_name,
                                                              gint8 property_value);

/**
 * b_device_service_property_provider_set_property_int16:
 * @self: The #BDeviceServicePropertyProvider instance.
 * @property_name: The name of the property to set.
 * @property_value: The value to set the property to.
 *
 * Calls b_device_service_property_provider_set_property and does not
 * propagate errors.
 *
 * Returns: %TRUE if the property was set successfully, %FALSE otherwise.
 */
gboolean b_device_service_property_provider_set_property_int16(BDeviceServicePropertyProvider *self,
                                                               const gchar *property_name,
                                                               gint16 property_value);

/**
 * b_device_service_property_provider_set_property_int32:
 * @self: The #BDeviceServicePropertyProvider instance.
 * @property_name: The name of the property to set.
 * @property_value: The value to set the property to.
 *
 * Calls b_device_service_property_provider_set_property and does not
 * propagate errors.
 *
 * Returns: %TRUE if the property was set successfully, %FALSE otherwise.
 */
gboolean b_device_service_property_provider_set_property_int32(BDeviceServicePropertyProvider *self,
                                                               const gchar *property_name,
                                                               gint32 property_value);

/**
 * b_device_service_property_provider_set_property_int64:
 * @self: The #BDeviceServicePropertyProvider instance.
 * @property_name: The name of the property to set.
 * @property_value: The value to set the property to.
 *
 * Calls b_device_service_property_provider_set_property and does not
 * propagate errors.
 *
 * Returns: %TRUE if the property was set successfully, %FALSE otherwise.
 */
gboolean b_device_service_property_provider_set_property_int64(BDeviceServicePropertyProvider *self,
                                                               const gchar *property_name,
                                                               gint64 property_value);

/**
 * b_device_service_property_provider_set_property_uint8:
 * @self: The #BDeviceServicePropertyProvider instance.
 * @property_name: The name of the property to set.
 * @property_value: The value to set the property to.
 *
 * Calls b_device_service_property_provider_set_property and does not
 * propagate errors.
 *
 * Returns: %TRUE if the property was set successfully, %FALSE otherwise.
 */
gboolean b_device_service_property_provider_set_property_uint8(BDeviceServicePropertyProvider *self,
                                                               const gchar *property_name,
                                                               guint8 property_value);

/**
 * b_device_service_property_provider_set_property_uint16:
 * @self: The #BDeviceServicePropertyProvider instance.
 * @property_name: The name of the property to set.
 * @property_value: The value to set the property to.
 *
 * Calls b_device_service_property_provider_set_property and does not
 * propagate errors.
 *
 * Returns: %TRUE if the property was set successfully, %FALSE otherwise.
 */
gboolean b_device_service_property_provider_set_property_uint16(BDeviceServicePropertyProvider *self,
                                                                const gchar *property_name,
                                                                guint16 property_value);

/**
 * b_device_service_property_provider_set_property_uint32:
 * @self: The #BDeviceServicePropertyProvider instance.
 * @property_name: The name of the property to set.
 * @property_value: The value to set the property to.
 *
 * Calls b_device_service_property_provider_set_property and does not
 * propagate errors.
 *
 * Returns: %TRUE if the property was set successfully, %FALSE otherwise.
 */
gboolean b_device_service_property_provider_set_property_uint32(BDeviceServicePropertyProvider *self,
                                                                const gchar *property_name,
                                                                guint32 property_value);

/**
 * b_device_service_property_provider_set_property_uint64:
 * @self: The #BDeviceServicePropertyProvider instance.
 * @property_name: The name of the property to set.
 * @property_value: The value to set the property to.
 *
 * Calls b_device_service_property_provider_set_property and does not
 * propagate errors.
 *
 * Returns: %TRUE if the property was set successfully, %FALSE otherwise.
 */
gboolean b_device_service_property_provider_set_property_uint64(BDeviceServicePropertyProvider *self,
                                                                const gchar *property_name,
                                                                guint64 property_value);

/**
 * b_device_service_property_provider_has_property:
 * @self: The #BDeviceServicePropertyProvider instance.
 * @property_name: The name of the property to check for.
 *
 * Calls b_device_service_property_provider_set_property and does not
 * propagate errors.
 *
 * Returns: %TRUE if the property exists, %FALSE otherwise.
 */
gboolean b_device_service_property_provider_has_property(BDeviceServicePropertyProvider *self,
                                                         const gchar *property_name);

G_END_DECLS