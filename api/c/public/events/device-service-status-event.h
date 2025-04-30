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
 * Created by Thomas Lea on 6/10/2024.
 */

#pragma once

#include "events/device-service-event.h"
#include <glib-object.h>

G_BEGIN_DECLS

#define B_DEVICE_SERVICE_STATUS_EVENT_TYPE (b_device_service_status_event_get_type())
G_DECLARE_FINAL_TYPE(BDeviceServiceStatusEvent,
                     b_device_service_status_event,
                     B_DEVICE_SERVICE,
                     STATUS_EVENT,
                     BDeviceServiceEvent)

typedef enum
{
    B_DEVICE_SERVICE_STATUS_CHANGED_REASON_INVALID = 0,
    B_DEVICE_SERVICE_STATUS_CHANGED_REASON_READY_FOR_PAIRING,
    B_DEVICE_SERVICE_STATUS_CHANGED_REASON_READY_FOR_DEVICE_OPERATION,
    B_DEVICE_SERVICE_STATUS_CHANGED_REASON_SUBSYSTEM_STATUS
} BDeviceServiceStatusChangedReason;

// This the old school way of creating GEnum types. We should be using G_DEFINE_ENUM_TYPE but that requires glib >=2.74
GType b_device_service_status_changed_reason_get_type(void);
#define B_DEVICE_SERVICE_STATUS_CHANGED_REASON_TYPE (b_device_service_status_changed_reason_get_type())

typedef enum
{
    B_DEVICE_SERVICE_STATUS_EVENT_PROP_STATUS = 1, // BDeviceServiceStatus object
    B_DEVICE_SERVICE_STATUS_EVENT_PROP_REASON,     // BDeviceServiceStatusChangedReason enum
    N_B_DEVICE_SERVICE_STATUS_EVENT_PROPERTIES
} BDeviceServiceStatusEventProperty;

static const char *B_DEVICE_SERVICE_STATUS_EVENT_PROPERTY_NAMES[] = {NULL, "status", "reason"};

/**
 * b_device_service_status_event_new
 *
 * @brief
 *
 * Returns: (transfer full): BDeviceServiceStatusEvent*
 */
BDeviceServiceStatusEvent *b_device_service_status_event_new(void);

G_END_DECLS
