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
 * Created by Christian Leithner on 6/17/2024.
 */

#include "events/barton-core-device-discovered-event.h"
#include "barton-core-device-found-details.h"

struct _BCoreDeviceDiscoveredEvent
{
    BCoreEvent parent_instance;

    BCoreDeviceFoundDetails *device_found_details;
};

G_DEFINE_TYPE(BCoreDeviceDiscoveredEvent,
              b_core_device_discovered_event,
              B_CORE_DISCOVERY_SESSION_INFO_EVENT_TYPE);

static GParamSpec *properties[N_B_CORE_DEVICE_DISCOVERED_EVENT_PROPERTIES];

static void b_core_device_discovered_event_init(BCoreDeviceDiscoveredEvent *self)
{
    self->device_found_details = NULL;
}

static void b_core_device_discovered_event_finalize(GObject *object)
{
    BCoreDeviceDiscoveredEvent *self = B_CORE_DEVICE_DISCOVERED_EVENT(object);

    g_clear_object(&self->device_found_details);

    G_OBJECT_CLASS(b_core_device_discovered_event_parent_class)->finalize(object);
}

static void b_core_device_discovered_event_get_property(GObject *object,
                                                                  guint property_id,
                                                                  GValue *value,
                                                                  GParamSpec *pspec)
{
    BCoreDeviceDiscoveredEvent *self = B_CORE_DEVICE_DISCOVERED_EVENT(object);

    switch (property_id)
    {
        case B_CORE_DEVICE_DISCOVERED_EVENT_PROP_DEVICE_FOUND_DETAILS:
            g_value_set_object(value, self->device_found_details);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_core_device_discovered_event_set_property(GObject *object,
                                                                  guint property_id,
                                                                  const GValue *value,
                                                                  GParamSpec *pspec)
{
    BCoreDeviceDiscoveredEvent *self = B_CORE_DEVICE_DISCOVERED_EVENT(object);

    switch (property_id)
    {
        case B_CORE_DEVICE_DISCOVERED_EVENT_PROP_DEVICE_FOUND_DETAILS:
            g_clear_object(&self->device_found_details);
            self->device_found_details = g_value_dup_object(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_core_device_discovered_event_class_init(BCoreDeviceDiscoveredEventClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = b_core_device_discovered_event_finalize;
    object_class->get_property = b_core_device_discovered_event_get_property;
    object_class->set_property = b_core_device_discovered_event_set_property;

    /**
     * BCoreDeviceDiscoveredEvent:device-found-details:
     *
     * BCoreDeviceFoundDetails describing what was known about the device when it was discovered
     */
    properties[B_CORE_DEVICE_DISCOVERED_EVENT_PROP_DEVICE_FOUND_DETAILS] = g_param_spec_object(
        B_CORE_DEVICE_DISCOVERED_EVENT_PROPERTY_NAMES
            [B_CORE_DEVICE_DISCOVERED_EVENT_PROP_DEVICE_FOUND_DETAILS],
        "Device Found Details",
        "BCoreDeviceFoundDetails describing what was known about the device when it was discovered",
        B_CORE_DEVICE_FOUND_DETAILS_TYPE,
        G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, N_B_CORE_DEVICE_DISCOVERED_EVENT_PROPERTIES, properties);
}

BCoreDeviceDiscoveredEvent *b_core_device_discovered_event_new(void)
{
    return g_object_new(B_CORE_DEVICE_DISCOVERED_EVENT_TYPE, NULL);
}
