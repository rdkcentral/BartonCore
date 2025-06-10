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

#include "events/barton-core-device-configuration-failed-event.h"

struct _BCoreDeviceConfigurationFailedEvent
{
    BCoreDeviceConfigurationEvent parent_instance;

    // No properties. Be sure to add unit testing if you add properties!
};

G_DEFINE_TYPE(BCoreDeviceConfigurationFailedEvent,
              b_core_device_configuration_failed_event,
              B_CORE_DEVICE_CONFIGURATION_EVENT_TYPE)

static void b_core_device_configuration_failed_event_init(BCoreDeviceConfigurationFailedEvent *self)
{
    // No properties
}

static void b_core_device_configuration_failed_event_finalize(GObject *object)
{
    G_OBJECT_CLASS(b_core_device_configuration_failed_event_parent_class)->finalize(object);
}

static void
b_core_device_configuration_failed_event_class_init(BCoreDeviceConfigurationFailedEventClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = b_core_device_configuration_failed_event_finalize;
}

BCoreDeviceConfigurationFailedEvent *b_core_device_configuration_failed_event_new(void)
{
    return g_object_new(B_CORE_DEVICE_CONFIGURATION_FAILED_EVENT_TYPE, NULL);
}
