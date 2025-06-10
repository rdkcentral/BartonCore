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
 * Created by Christian Leithner on 8/19/2024.
 */

#pragma once

#include "events/barton-core-event.h"
#include <glib-object.h>

G_BEGIN_DECLS

#define B_CORE_DEVICE_DATABASE_FAILURE_EVENT_TYPE (b_core_device_database_failure_event_get_type())

G_DECLARE_FINAL_TYPE(BCoreDeviceDatabaseFailureEvent,
                     b_core_device_database_failure_event,
                     B_CORE,
                     DEVICE_DATABASE_FAILURE_EVENT,
                     BCoreEvent)

typedef enum
{
    B_CORE_DEVICE_DATABASE_FAILURE_TYPE_DEVICE_LOAD_FAILURE =
        1, // Failed to load a device from the device database
} BCoreDeviceDatabaseFailureType;

// This the old school way of creating GEnum types. We should be using G_DEFINE_ENUM_TYPE but that requires glib >=2.74
GType b_core_device_database_failure_type_type(void);
#define B_CORE_DEVICE_DATABASE_FAILURE_TYPE_TYPE (b_core_device_database_failure_type_type())

typedef enum
{
    B_CORE_DEVICE_DATABASE_FAILURE_EVENT_PROP_FAILURE_TYPE = 1,

    B_CORE_DEVICE_DATABASE_FAILURE_EVENT_PROP_DEVICE_ID, // Applicable if
                                                                   // B_CORE_DEVICE_DATABASE_FAILURE_EVENT_PROP_FAILURE_TYPE
                                                                   // ==
                                                                   // B_CORE_DEVICE_DATABASE_FAILURE_TYPE_DEVICE_LOAD_FAILURE

    N_B_CORE_DEVICE_DATABASE_FAILURE_EVENT_PROPERTIES,
} BCoreDeviceDatabaseFailureEventProperty;

static const char *B_CORE_DEVICE_DATABASE_FAILURE_EVENT_PROPERTY_NAMES[] = {
    NULL,
    "failure-type",
    "device-id",
};

/**
 * b_core_device_database_failure_event_new
 *
 * Creates a new device database failure event.
 *
 * Returns: (transfer full): A new device database failure event.
 */
BCoreDeviceDatabaseFailureEvent *b_core_device_database_failure_event_new(void);

G_END_DECLS
