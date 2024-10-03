//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2024 Comcast Corporation
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
// Comcast Corporation retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------

/*
 * Created by Ganesh Induri on 07/26/2024.
 */

#include "events/device-service-zigbee-channel-changed-event.h"

struct _BDeviceServiceZigbeeChannelChangedEvent
{
    BDeviceServiceEvent parent_instance;

    gboolean channel_changed;
    guint8 current_channel;
    guint8 targeted_channel;
};

G_DEFINE_TYPE(BDeviceServiceZigbeeChannelChangedEvent,
              b_device_service_zigbee_channel_changed_event,
              B_DEVICE_SERVICE_EVENT_TYPE)

static GParamSpec *properties[N_B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROPERTIES];

static void b_device_service_zigbee_channel_changed_event_init(BDeviceServiceZigbeeChannelChangedEvent *self)
{
    self->channel_changed = false;
    self->current_channel = 0;
    self->targeted_channel = 0;
}

static void b_device_service_zigbee_channel_changed_event_finalize(GObject *object)
{

    G_OBJECT_CLASS(b_device_service_zigbee_channel_changed_event_parent_class)->finalize(object);
}

static void b_device_service_zigbee_channel_changed_event_get_property(GObject *object,
                                                                       guint property_id,
                                                                       GValue *value,
                                                                       GParamSpec *pspec)
{
    BDeviceServiceZigbeeChannelChangedEvent *self = B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT(object);

    switch (property_id)
    {
        case B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_CHANNEL_CHANGED:
            g_value_set_boolean(value, self->channel_changed);
            break;
        case B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_CURRENT_CHANNEL:
            g_value_set_uint(value, self->current_channel);
            break;
        case B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_TARGETED_CHANNEL:
            g_value_set_uint(value, self->targeted_channel);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_device_service_zigbee_channel_changed_event_set_property(GObject *object,
                                                                       guint property_id,
                                                                       const GValue *value,
                                                                       GParamSpec *pspec)
{
    BDeviceServiceZigbeeChannelChangedEvent *self = B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT(object);

    switch (property_id)
    {
        case B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_CHANNEL_CHANGED:
            self->channel_changed = g_value_get_boolean(value);
            break;
        case B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_CURRENT_CHANNEL:
            self->current_channel = g_value_get_uint(value);
            break;
        case B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_TARGETED_CHANNEL:
            self->targeted_channel = g_value_get_uint(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
b_device_service_zigbee_channel_changed_event_class_init(BDeviceServiceZigbeeChannelChangedEventClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = b_device_service_zigbee_channel_changed_event_finalize;
    object_class->get_property = b_device_service_zigbee_channel_changed_event_get_property;
    object_class->set_property = b_device_service_zigbee_channel_changed_event_set_property;

    properties[B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_CHANNEL_CHANGED] =
        g_param_spec_boolean(B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROPERTY_NAMES
                                 [B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_CHANNEL_CHANGED],
                             "channel-changed",
                             "Indicates if the channel actually changed (true) or failed (false)",
                             FALSE,
                             G_PARAM_READWRITE);

    properties[B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_CURRENT_CHANNEL] =
        g_param_spec_uint(B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROPERTY_NAMES
                              [B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_CURRENT_CHANNEL],
                          "current-channel",
                          "The current Zigbee channel",
                          0,
                          G_MAXUINT8,
                          0,
                          G_PARAM_READWRITE);

    properties[B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_TARGETED_CHANNEL] =
        g_param_spec_uint(B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROPERTY_NAMES
                              [B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROP_TARGETED_CHANNEL],
                          "targeted-channel",
                          "Indicates targeted Zigbee channel",
                          0,
                          G_MAXUINT8,
                          0,
                          G_PARAM_READWRITE);

    g_object_class_install_properties(
        object_class, N_B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_PROPERTIES, properties);
}

BDeviceServiceZigbeeChannelChangedEvent *b_device_service_zigbee_channel_changed_event_new(void)
{
    return g_object_new(B_DEVICE_SERVICE_ZIGBEE_CHANNEL_CHANGED_EVENT_TYPE, NULL);
}
