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
 * Created by Thomas Lea on 6/10/2024.
 */

#include "events/device-service-status-event.h"
#include "device-service-utils.h"
#include "device-service-status.h"
#include "events/device-service-event.h"

#include <glib.h>

struct _BDeviceServiceStatusEvent
{
    BDeviceServiceEvent parent_instance;

    BDeviceServiceStatus *status;
    BDeviceServiceStatusChangedReason reason;
};

G_DEFINE_TYPE(BDeviceServiceStatusEvent, b_device_service_status_event, B_DEVICE_SERVICE_EVENT_TYPE);

static GParamSpec *properties[N_B_DEVICE_SERVICE_STATUS_EVENT_PROPERTIES];

static void b_device_service_status_event_init(BDeviceServiceStatusEvent *self)
{
    self->status = NULL;
    self->reason = B_DEVICE_SERVICE_STATUS_CHANGED_REASON_INVALID;
}

static void b_device_service_status_event_finalize(GObject *object)
{
    BDeviceServiceStatusEvent *self = B_DEVICE_SERVICE_STATUS_EVENT(object);

    g_clear_object(&self->status);

    G_OBJECT_CLASS(b_device_service_status_event_parent_class)->finalize(object);
}

static void b_device_service_status_event_get_property(GObject *object,
                                                     guint property_id,
                                                     GValue *value,
                                                     GParamSpec *pspec)
{
    BDeviceServiceStatusEvent *self = B_DEVICE_SERVICE_STATUS_EVENT(object);

    switch (property_id)
    {
        case B_DEVICE_SERVICE_STATUS_EVENT_PROP_STATUS:
            g_value_set_object(value, self->status);
            break;
        case B_DEVICE_SERVICE_STATUS_EVENT_PROP_REASON:
            g_value_set_enum(value, self->reason);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_device_service_status_event_set_property(GObject *object,
                                                     guint property_id,
                                                     const GValue *value,
                                                     GParamSpec *pspec)
{
    BDeviceServiceStatusEvent *self = B_DEVICE_SERVICE_STATUS_EVENT(object);

    switch (property_id)
    {
        case B_DEVICE_SERVICE_STATUS_EVENT_PROP_STATUS:
            if (self->status) g_object_unref(self->status);
            self->status = g_value_dup_object(value);
            break;
        case B_DEVICE_SERVICE_STATUS_EVENT_PROP_REASON:
            self->reason = g_value_get_enum(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_device_service_status_event_class_init(BDeviceServiceStatusEventClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = (GObjectFinalizeFunc) b_device_service_status_event_finalize;

    object_class->get_property = b_device_service_status_event_get_property;
    object_class->set_property = b_device_service_status_event_set_property;

    /**
     * DeviceServiceStatusEvent:status: (type DeviceServiceStatus)
     */
    properties[B_DEVICE_SERVICE_STATUS_EVENT_PROP_STATUS] =
        g_param_spec_object(B_DEVICE_SERVICE_STATUS_EVENT_PROPERTY_NAMES
                                 [B_DEVICE_SERVICE_STATUS_EVENT_PROP_STATUS],
                             "Device Service Status",
                             "The overall status of Device Service and its subsystems",
                             B_DEVICE_SERVICE_STATUS_TYPE,
                             G_PARAM_READWRITE);

    properties[B_DEVICE_SERVICE_STATUS_EVENT_PROP_REASON] =
        g_param_spec_enum(B_DEVICE_SERVICE_STATUS_EVENT_PROPERTY_NAMES
                              [B_DEVICE_SERVICE_STATUS_EVENT_PROP_REASON],
                          "Reason",
                          "The reason for the status change",
                          B_DEVICE_SERVICE_STATUS_CHANGED_REASON_TYPE,
                          B_DEVICE_SERVICE_STATUS_CHANGED_REASON_INVALID,
                          G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, N_B_DEVICE_SERVICE_STATUS_EVENT_PROPERTIES, properties);
}

BDeviceServiceStatusEvent *b_device_service_status_event_new(void)
{
    return B_DEVICE_SERVICE_STATUS_EVENT(g_object_new(B_DEVICE_SERVICE_STATUS_EVENT_TYPE, NULL));
}

GType b_device_service_status_changed_reason_get_type(void)
{
    static gsize g_define_type_id__volatile = 0;

    // Initializes the BDeviceServiceStatusChangedReason enum GType only once.
    if (g_once_init_enter(&g_define_type_id__volatile))
    {
        static const GEnumValue values[] = {
            {B_DEVICE_SERVICE_STATUS_CHANGED_REASON_INVALID, "B_DEVICE_SERVICE_STATUS_CHANGED_REASON_INVALID",  "invalid"},
            {B_DEVICE_SERVICE_STATUS_CHANGED_REASON_READY_FOR_PAIRING, "B_DEVICE_SERVICE_STATUS_CHANGED_REASON_READY_FOR_PAIRING",  "ready-for-pairing"},
            {B_DEVICE_SERVICE_STATUS_CHANGED_REASON_READY_FOR_DEVICE_OPERATION, "B_DEVICE_SERVICE_STATUS_CHANGED_REASON_READY_FOR_DEVICE_OPERATION",  "ready-for-device-operation"},
            {B_DEVICE_SERVICE_STATUS_CHANGED_REASON_SUBSYSTEM_STATUS, "B_DEVICE_SERVICE_STATUS_CHANGED_REASON_SUBSYSTEM_STATUS",  "subsystem-status"},
            {0,NULL,NULL}
        };

        GType g_define_type_id =
            g_enum_register_static(g_intern_static_string("BDeviceServiceStatusChangedReason"), values);
        g_once_init_leave(&g_define_type_id__volatile, g_define_type_id);
    }

    return g_define_type_id__volatile;
}
