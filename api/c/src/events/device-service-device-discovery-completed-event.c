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
 * Created by Christian Leithner on 7/8/2024.
 */

#include "events/device-service-device-discovery-completed-event.h"
#include "device-service-device.h"

struct _BDeviceServiceDeviceDiscoveryCompletedEvent
{
    BDeviceServiceDiscoverySessionInfoEvent parent_instance;

    BDeviceServiceDevice *device;
};

G_DEFINE_TYPE(BDeviceServiceDeviceDiscoveryCompletedEvent,
              b_device_service_device_discovery_completed_event,
              B_DEVICE_SERVICE_DISCOVERY_SESSION_INFO_EVENT_TYPE);

static GParamSpec *properties[N_B_DEVICE_SERVICE_DEVICE_DISCOVERY_COMPLETED_EVENT_PROPERTIES];

static void b_device_service_device_discovery_completed_event_init(BDeviceServiceDeviceDiscoveryCompletedEvent *self)
{
    self->device = NULL;
}

static void b_device_service_device_discovery_completed_event_finalize(GObject *object)
{
    BDeviceServiceDeviceDiscoveryCompletedEvent *self = B_DEVICE_SERVICE_DEVICE_DISCOVERY_COMPLETED_EVENT(object);

    g_clear_object(&self->device);

    G_OBJECT_CLASS(b_device_service_device_discovery_completed_event_parent_class)->finalize(object);
}

static void b_device_service_device_discovery_completed_event_get_property(GObject *object,
                                                                           guint property_id,
                                                                           GValue *value,
                                                                           GParamSpec *pspec)
{
    BDeviceServiceDeviceDiscoveryCompletedEvent *self = B_DEVICE_SERVICE_DEVICE_DISCOVERY_COMPLETED_EVENT(object);

    switch (property_id)
    {
        case B_DEVICE_SERVICE_DEVICE_DISCOVERY_COMPLETED_EVENT_PROP_DEVICE:
            g_value_set_object(value, self->device);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_device_service_device_discovery_completed_event_set_property(GObject *object,
                                                                           guint property_id,
                                                                           const GValue *value,
                                                                           GParamSpec *pspec)
{
    BDeviceServiceDeviceDiscoveryCompletedEvent *self = B_DEVICE_SERVICE_DEVICE_DISCOVERY_COMPLETED_EVENT(object);

    switch (property_id)
    {
        case B_DEVICE_SERVICE_DEVICE_DISCOVERY_COMPLETED_EVENT_PROP_DEVICE:
            g_clear_object(&self->device);
            self->device = g_value_dup_object(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
b_device_service_device_discovery_completed_event_class_init(BDeviceServiceDeviceDiscoveryCompletedEventClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = b_device_service_device_discovery_completed_event_finalize;
    object_class->get_property = b_device_service_device_discovery_completed_event_get_property;
    object_class->set_property = b_device_service_device_discovery_completed_event_set_property;

    /**
     * BDeviceServiceDeviceDiscoveryCompletedEvent:device: (type BDeviceServiceDevice)
     */
    properties[B_DEVICE_SERVICE_DEVICE_DISCOVERY_COMPLETED_EVENT_PROP_DEVICE] =
        g_param_spec_object(B_DEVICE_SERVICE_DEVICE_DISCOVERY_COMPLETED_EVENT_PROPERTY_NAMES
                                [B_DEVICE_SERVICE_DEVICE_DISCOVERY_COMPLETED_EVENT_PROP_DEVICE],
                            "Device",
                            "The device that was discovered",
                            B_DEVICE_SERVICE_DEVICE_TYPE,
                            G_PARAM_READWRITE);

    g_object_class_install_properties(
        object_class, N_B_DEVICE_SERVICE_DEVICE_DISCOVERY_COMPLETED_EVENT_PROPERTIES, properties);
}

BDeviceServiceDeviceDiscoveryCompletedEvent *b_device_service_device_discovery_completed_event_new(void)
{
    return g_object_new(B_DEVICE_SERVICE_DEVICE_DISCOVERY_COMPLETED_EVENT_TYPE, NULL);
}
