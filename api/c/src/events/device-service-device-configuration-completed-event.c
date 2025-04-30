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
 * Created by Christian Leithner on 7/1/2024.
 */

#include "events/device-service-device-configuration-completed-event.h"

struct _BDeviceServiceDeviceConfigurationCompletedEvent
{
    BDeviceServiceDeviceConfigurationEvent parent_instance;

    // No properties. Be sure to add unit testing if you add properties!
};

G_DEFINE_TYPE(BDeviceServiceDeviceConfigurationCompletedEvent,
              b_device_service_device_configuration_completed_event,
              B_DEVICE_SERVICE_DEVICE_CONFIGURATION_EVENT_TYPE)

static void
b_device_service_device_configuration_completed_event_init(BDeviceServiceDeviceConfigurationCompletedEvent *self)
{
    // No properties
}

static void b_device_service_device_configuration_completed_event_finalize(GObject *object)
{
    G_OBJECT_CLASS(b_device_service_device_configuration_completed_event_parent_class)->finalize(object);
}

static void b_device_service_device_configuration_completed_event_class_init(
    BDeviceServiceDeviceConfigurationCompletedEventClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = b_device_service_device_configuration_completed_event_finalize;
}

BDeviceServiceDeviceConfigurationCompletedEvent *b_device_service_device_configuration_completed_event_new(void)
{
    return g_object_new(B_DEVICE_SERVICE_DEVICE_CONFIGURATION_COMPLETED_EVENT_TYPE, NULL);
}
