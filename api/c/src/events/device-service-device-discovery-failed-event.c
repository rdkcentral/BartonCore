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
 * Created by Christian Leithner on 6/13/2024.
 */

#include "events/device-service-device-discovery-failed-event.h"
#include "device-service-device-found-details.h"
#include "events/device-service-discovery-session-info-event.h"

struct _BDeviceServiceDeviceDiscoveryFailedEvent
{
    BDeviceServiceEvent parent_instance;

    BDeviceServiceDeviceFoundDetails *device_found_details;
};

G_DEFINE_TYPE(BDeviceServiceDeviceDiscoveryFailedEvent,
              b_device_service_device_discovery_failed_event,
              B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_TYPE);

static GParamSpec *properties[N_B_DEVICE_SERVICE_DEVICE_DISCOVERY_FAILED_EVENT_PROPERTIES];

static void b_device_service_device_discovery_failed_event_init(BDeviceServiceDeviceDiscoveryFailedEvent *self)
{
    self->device_found_details = NULL;
}

static void b_device_service_device_discovery_failed_event_finalize(GObject *object)
{
    BDeviceServiceDeviceDiscoveryFailedEvent *self = B_DEVICE_SERVICE_DEVICE_DISCOVERY_FAILED_EVENT(object);

    g_clear_object(&self->device_found_details);

    G_OBJECT_CLASS(b_device_service_device_discovery_failed_event_parent_class)->finalize(object);
}

static void b_device_service_device_discovery_failed_event_get_property(GObject *object,
                                                                        guint property_id,
                                                                        GValue *value,
                                                                        GParamSpec *pspec)
{
    BDeviceServiceDeviceDiscoveryFailedEvent *self = B_DEVICE_SERVICE_DEVICE_DISCOVERY_FAILED_EVENT(object);

    switch (property_id)
    {
        case B_DEVICE_SERVICE_DEVICE_DISCOVERY_FAILED_EVENT_PROP_DEVICE_FOUND_DETAILS:
            g_value_set_object(value, self->device_found_details);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_device_service_device_discovery_failed_event_set_property(GObject *object,
                                                                        guint property_id,
                                                                        const GValue *value,
                                                                        GParamSpec *pspec)
{
    BDeviceServiceDeviceDiscoveryFailedEvent *self = B_DEVICE_SERVICE_DEVICE_DISCOVERY_FAILED_EVENT(object);

    switch (property_id)
    {
        case B_DEVICE_SERVICE_DEVICE_DISCOVERY_FAILED_EVENT_PROP_DEVICE_FOUND_DETAILS:
            g_clear_object(&self->device_found_details);
            self->device_found_details = g_value_dup_object(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
b_device_service_device_discovery_failed_event_class_init(BDeviceServiceDeviceDiscoveryFailedEventClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->get_property = b_device_service_device_discovery_failed_event_get_property;
    object_class->set_property = b_device_service_device_discovery_failed_event_set_property;
    object_class->finalize = b_device_service_device_discovery_failed_event_finalize;

    /**
     * BDeviceServiceDeviceDiscoveryFailedEvent:device-found-details:
     *
     * BDeviceServiceDeviceFoundDetails describing what was known about the device when discovery failed
     */
    properties[B_DEVICE_SERVICE_DEVICE_DISCOVERY_FAILED_EVENT_PROP_DEVICE_FOUND_DETAILS] = g_param_spec_object(
        B_DEVICE_SERVICE_DEVICE_DISCOVERY_FAILED_EVENT_PROPERTY_NAMES
            [B_DEVICE_SERVICE_DEVICE_DISCOVERY_FAILED_EVENT_PROP_DEVICE_FOUND_DETAILS],
        "Device found details",
        "BDeviceServiceDeviceFoundDetails describing what was known about the device when discovery failed",
        B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_TYPE,
        G_PARAM_READWRITE);

    g_object_class_install_properties(
        object_class, N_B_DEVICE_SERVICE_DEVICE_DISCOVERY_FAILED_EVENT_PROPERTIES, properties);
}

BDeviceServiceDeviceDiscoveryFailedEvent *b_device_service_device_discovery_failed_event_new(void)
{
    return B_DEVICE_SERVICE_DEVICE_DISCOVERY_FAILED_EVENT(
        g_object_new(B_DEVICE_SERVICE_DEVICE_DISCOVERY_FAILED_EVENT_TYPE, NULL));
}