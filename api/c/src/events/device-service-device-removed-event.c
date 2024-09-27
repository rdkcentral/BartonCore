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
 * Created by Christian Leithner on 7/16/2024.
 */

#include "events/device-service-device-removed-event.h"

struct _BDeviceServiceDeviceRemovedEvent
{
    BDeviceServiceEvent parent_instance;

    gchar *device_uuid;
    gchar *device_class;
};

G_DEFINE_TYPE(BDeviceServiceDeviceRemovedEvent, b_device_service_device_removed_event, B_DEVICE_SERVICE_EVENT_TYPE);

static GParamSpec *properties[N_B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROPERTIES];

static void b_device_service_device_removed_event_init(BDeviceServiceDeviceRemovedEvent *self)
{
    self->device_uuid = NULL;
    self->device_class = NULL;
}

static void b_device_service_device_removed_event_finalize(GObject *object)
{
    BDeviceServiceDeviceRemovedEvent *self = B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT(object);

    g_free(self->device_uuid);
    g_free(self->device_class);

    G_OBJECT_CLASS(b_device_service_device_removed_event_parent_class)->finalize(object);
}

static void
b_device_service_device_removed_event_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    BDeviceServiceDeviceRemovedEvent *self = B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT(object);

    switch (property_id)
    {
        case B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROP_DEVICE_UUID:
            g_value_set_string(value, self->device_uuid);
            break;
        case B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROP_DEVICE_CLASS:
            g_value_set_string(value, self->device_class);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_device_service_device_removed_event_set_property(GObject *object,
                                                               guint property_id,
                                                               const GValue *value,
                                                               GParamSpec *pspec)
{
    BDeviceServiceDeviceRemovedEvent *self = B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT(object);

    switch (property_id)
    {
        case B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROP_DEVICE_UUID:
            g_free(self->device_uuid);
            self->device_uuid = g_value_dup_string(value);
            break;
        case B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROP_DEVICE_CLASS:
            g_free(self->device_class);
            self->device_class = g_value_dup_string(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_device_service_device_removed_event_class_init(BDeviceServiceDeviceRemovedEventClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = b_device_service_device_removed_event_finalize;
    object_class->get_property = b_device_service_device_removed_event_get_property;
    object_class->set_property = b_device_service_device_removed_event_set_property;

    properties[B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROP_DEVICE_UUID] = g_param_spec_string(
        B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROP_DEVICE_UUID],
        "Device UUID",
        "The device ID of the device that was removed",
        NULL,
        G_PARAM_READWRITE);

    properties[B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROP_DEVICE_CLASS] = g_param_spec_string(
        B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROPERTY_NAMES[B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROP_DEVICE_CLASS],
        "Device Class",
        "The device class of the device that was removed",
        NULL,
        G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, N_B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_PROPERTIES, properties);
}

BDeviceServiceDeviceRemovedEvent *b_device_service_device_removed_event_new(void)
{
    return B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT(g_object_new(B_DEVICE_SERVICE_DEVICE_REMOVED_EVENT_TYPE, NULL));
}