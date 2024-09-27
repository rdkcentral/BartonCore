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
 * Created by Christian Leithner on 8/19/2024.
 */

#include "events/device-service-device-database-failure-event.h"

struct _BDeviceServiceDeviceDatabaseFailureEvent
{
    BDeviceServiceEvent parent_instance;

    BDeviceServiceDeviceDatabaseFailureType failure_type;
    gchar *device_id;
};

G_DEFINE_TYPE(BDeviceServiceDeviceDatabaseFailureEvent,
              b_device_service_device_database_failure_event,
              B_DEVICE_SERVICE_EVENT_TYPE);

static GParamSpec *properties[N_B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROPERTIES];

static void b_device_service_device_database_failure_event_init(BDeviceServiceDeviceDatabaseFailureEvent *self)
{
    self->failure_type = B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_TYPE_DEVICE_LOAD_FAILURE;
    self->device_id = NULL;
}

static void b_device_service_device_database_failure_event_finalize(GObject *object)
{
    BDeviceServiceDeviceDatabaseFailureEvent *self = B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT(object);

    g_clear_pointer(&self->device_id, g_free);

    G_OBJECT_CLASS(b_device_service_device_database_failure_event_parent_class)->finalize(object);
}

static void b_device_service_device_database_failure_event_get_property(GObject *object,
                                                                        guint property_id,
                                                                        GValue *value,
                                                                        GParamSpec *pspec)
{
    BDeviceServiceDeviceDatabaseFailureEvent *self = B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT(object);

    switch (property_id)
    {
        case B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROP_FAILURE_TYPE:
            g_value_set_enum(value, self->failure_type);
            break;
        case B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROP_DEVICE_ID:
            g_value_set_string(value, self->device_id);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_device_service_device_database_failure_event_set_property(GObject *object,
                                                                        guint property_id,
                                                                        const GValue *value,
                                                                        GParamSpec *pspec)
{
    BDeviceServiceDeviceDatabaseFailureEvent *self = B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT(object);

    switch (property_id)
    {
        case B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROP_FAILURE_TYPE:
            self->failure_type = g_value_get_enum(value);
            break;
        case B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROP_DEVICE_ID:
            g_clear_pointer(&self->device_id, g_free);
            self->device_id = g_value_dup_string(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
b_device_service_device_database_failure_event_class_init(BDeviceServiceDeviceDatabaseFailureEventClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = b_device_service_device_database_failure_event_finalize;
    object_class->get_property = b_device_service_device_database_failure_event_get_property;
    object_class->set_property = b_device_service_device_database_failure_event_set_property;

    properties[B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROP_FAILURE_TYPE] =
        g_param_spec_enum(B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROPERTY_NAMES
                              [B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROP_FAILURE_TYPE],
                          "Failure Type",
                          "The type of failure that occurred",
                          B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_TYPE_TYPE,
                          B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_TYPE_DEVICE_LOAD_FAILURE,
                          G_PARAM_READWRITE);

    properties[B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROP_DEVICE_ID] =
        g_param_spec_string(B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROPERTY_NAMES
                                [B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROP_DEVICE_ID],
                            "Device ID",
                            "The ID of the device that failed to load",
                            NULL,
                            G_PARAM_READWRITE);

    g_object_class_install_properties(
        object_class, N_B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_PROPERTIES, properties);
}

BDeviceServiceDeviceDatabaseFailureEvent *b_device_service_device_database_failure_event_new(void)
{
    return g_object_new(B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_EVENT_TYPE, NULL);
}

GType b_device_service_device_database_failure_type_type(void)
{
    static GType type = 0;

    if (type == 0)
    {
        static const GEnumValue values[] = {
            {B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_TYPE_DEVICE_LOAD_FAILURE,
             "B_DEVICE_SERVICE_DEVICE_DATABASE_FAILURE_TYPE_DEVICE_LOAD_FAILURE", "device-load-failure"    },
            {                                                                0, NULL,                  NULL},
        };

        type = g_enum_register_static("BDeviceServiceDeviceDatabaseFailureType", values);
    }

    return type;
}