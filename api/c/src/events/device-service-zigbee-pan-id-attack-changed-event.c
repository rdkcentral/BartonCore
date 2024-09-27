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
 * Created by ss411 on 08/08/2024.
 */

#include "events/device-service-zigbee-pan-id-attack-changed-event.h"

struct _BDeviceServiceZigbeePanIdAttackChangedEvent
{
    BDeviceServiceEvent parent_instance;

    gboolean attack_detected;
};

G_DEFINE_TYPE(BDeviceServiceZigbeePanIdAttackChangedEvent,
              b_device_service_zigbee_pan_id_attack_changed_event,
              B_DEVICE_SERVICE_EVENT_TYPE)

static GParamSpec *properties[N_B_DEVICE_SERVICE_ZIGBEE_PAN_ID_ATTACK_CHANGED_EVENT_PROPERTIES];

static void b_device_service_zigbee_pan_id_attack_changed_event_init(BDeviceServiceZigbeePanIdAttackChangedEvent *self)
{
    self->attack_detected = false;
}

static void b_device_service_zigbee_pan_id_attack_changed_event_finalize(GObject *object)
{
    G_OBJECT_CLASS(b_device_service_zigbee_pan_id_attack_changed_event_parent_class)->finalize(object);
}

static void b_device_service_zigbee_pan_id_attack_changed_event_get_property(GObject *object,
                                                                             guint property_id,
                                                                             GValue *value,
                                                                             GParamSpec *pspec)
{
    BDeviceServiceZigbeePanIdAttackChangedEvent *self = B_DEVICE_SERVICE_ZIGBEE_PAN_ID_ATTACK_CHANGED_EVENT(object);

    switch (property_id)
    {
        case B_DEVICE_SERVICE_ZIGBEE_PAN_ID_ATTACK_CHANGED_EVENT_PROP_ATTACK_DETECTED:
            g_value_set_boolean(value, self->attack_detected);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_device_service_zigbee_pan_id_attack_changed_event_set_property(GObject *object,
                                                                             guint property_id,
                                                                             const GValue *value,
                                                                             GParamSpec *pspec)
{
    BDeviceServiceZigbeePanIdAttackChangedEvent *self = B_DEVICE_SERVICE_ZIGBEE_PAN_ID_ATTACK_CHANGED_EVENT(object);

    switch (property_id)
    {
        case B_DEVICE_SERVICE_ZIGBEE_PAN_ID_ATTACK_CHANGED_EVENT_PROP_ATTACK_DETECTED:
            self->attack_detected = g_value_get_boolean(value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
b_device_service_zigbee_pan_id_attack_changed_event_class_init(BDeviceServiceZigbeePanIdAttackChangedEventClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = b_device_service_zigbee_pan_id_attack_changed_event_finalize;
    object_class->get_property = b_device_service_zigbee_pan_id_attack_changed_event_get_property;
    object_class->set_property = b_device_service_zigbee_pan_id_attack_changed_event_set_property;

    properties[B_DEVICE_SERVICE_ZIGBEE_PAN_ID_ATTACK_CHANGED_EVENT_PROP_ATTACK_DETECTED] =
        g_param_spec_boolean(B_DEVICE_SERVICE_ZIGBEE_PAN_ID_ATTACK_CHANGED_EVENT_PROPERTY_NAMES
                                 [B_DEVICE_SERVICE_ZIGBEE_PAN_ID_ATTACK_CHANGED_EVENT_PROP_ATTACK_DETECTED],
                             "attack-detected",
                             "If true, we have detected zigbee pan Id attack and false if attack "
                             "had occurred and has subsided",
                             FALSE,
                             G_PARAM_READWRITE);

    g_object_class_install_properties(
        object_class, N_B_DEVICE_SERVICE_ZIGBEE_PAN_ID_ATTACK_CHANGED_EVENT_PROPERTIES, properties);
}

BDeviceServiceZigbeePanIdAttackChangedEvent *b_device_service_zigbee_pan_id_attack_changed_event_new(void)
{
    return g_object_new(B_DEVICE_SERVICE_ZIGBEE_PAN_ID_ATTACK_CHANGED_EVENT_TYPE, NULL);
}