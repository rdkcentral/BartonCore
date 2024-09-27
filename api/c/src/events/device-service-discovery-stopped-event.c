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
 * Created by Christian Leithner on 7/10/2024.
 */

#include "events/device-service-discovery-stopped-event.h"

#include <glib.h>

struct _BDeviceServiceDiscoveryStoppedEvent
{
    BDeviceServiceEvent parent_instance;

    gchar *device_class;
};

G_DEFINE_TYPE(BDeviceServiceDiscoveryStoppedEvent,
              b_device_service_discovery_stopped_event,
              B_DEVICE_SERVICE_EVENT_TYPE);

static GParamSpec *properties[N_B_DEVICE_SERVICE_DISCOVERY_STOPPED_EVENT_PROPERTIES];

static void b_device_service_discovery_stopped_event_init(BDeviceServiceDiscoveryStoppedEvent *self)
{
    self->device_class = NULL;
}

static void b_device_service_discovery_stopped_event_finalize(GObject *object)
{
    BDeviceServiceDiscoveryStoppedEvent *self = B_DEVICE_SERVICE_DISCOVERY_STOPPED_EVENT(object);

    g_free(self->device_class);

    G_OBJECT_CLASS(b_device_service_discovery_stopped_event_parent_class)->finalize(object);
}

static void b_device_service_discovery_stopped_event_get_property(GObject *object,
                                                                  guint property_id,
                                                                  GValue *value,
                                                                  GParamSpec *pspec)
{
    BDeviceServiceDiscoveryStoppedEvent *self = B_DEVICE_SERVICE_DISCOVERY_STOPPED_EVENT(object);

    switch (property_id)
    {
        case B_DEVICE_SERVICE_DISCOVERY_STOPPED_EVENT_PROP_DEVICE_CLASS:
            g_value_set_string(value, self->device_class);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_device_service_discovery_stopped_event_set_property(GObject *object,
                                                                  guint property_id,
                                                                  const GValue *value,
                                                                  GParamSpec *pspec)
{
    BDeviceServiceDiscoveryStoppedEvent *self = B_DEVICE_SERVICE_DISCOVERY_STOPPED_EVENT(object);

    switch (property_id)
    {
        case B_DEVICE_SERVICE_DISCOVERY_STOPPED_EVENT_PROP_DEVICE_CLASS:
            g_free(self->device_class);
            self->device_class = g_value_dup_string(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_device_service_discovery_stopped_event_class_init(BDeviceServiceDiscoveryStoppedEventClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = b_device_service_discovery_stopped_event_finalize;
    object_class->get_property = b_device_service_discovery_stopped_event_get_property;
    object_class->set_property = b_device_service_discovery_stopped_event_set_property;

    properties[B_DEVICE_SERVICE_DISCOVERY_STOPPED_EVENT_PROP_DEVICE_CLASS] =
        g_param_spec_string(B_DEVICE_SERVICE_DISCOVERY_STOPPED_EVENT_PROPERTY_NAMES
                                [B_DEVICE_SERVICE_DISCOVERY_STOPPED_EVENT_PROP_DEVICE_CLASS],
                            "Device class",
                            "The device class discovery was stopped for",
                            NULL,
                            G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, N_B_DEVICE_SERVICE_DISCOVERY_STOPPED_EVENT_PROPERTIES, properties);
}

BDeviceServiceDiscoveryStoppedEvent *b_device_service_discovery_stopped_event_new(void)
{
    return g_object_new(B_DEVICE_SERVICE_DISCOVERY_STOPPED_EVENT_TYPE, NULL);
}