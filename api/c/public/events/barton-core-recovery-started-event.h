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
 * Created by Christian Leithner on 7/11/2024.
 */

#pragma once

#include "events/barton-core-event.h"
#include <glib-object.h>

G_BEGIN_DECLS

#define B_CORE_RECOVERY_STARTED_EVENT_TYPE (b_core_recovery_started_event_get_type())
G_DECLARE_FINAL_TYPE(BCoreRecoveryStartedEvent,
                     b_core_recovery_started_event,
                     B_CORE,
                     RECOVERY_STARTED_EVENT,
                     BCoreEvent)

typedef enum
{
    B_CORE_RECOVERY_STARTED_EVENT_PROP_DEVICE_CLASSES =
        1, // GList of strings describing which device classes recovery was started for
    B_CORE_RECOVERY_STARTED_EVENT_PROP_TIMEOUT, // Timeout for recovery in seconds
    N_B_CORE_RECOVERY_STARTED_EVENT_PROPERTIES
} BCoreRecoveryStartedEventProperty;

static const char *B_CORE_RECOVERY_STARTED_EVENT_PROPERTY_NAMES[] = {NULL, "device-classes", "timeout"};

/**
 * b_core_recovery_started_event_new
 *
 * @brief
 *
 * Returns: (transfer full): BCoreRecoveryStartedEvent*
 */
BCoreRecoveryStartedEvent *b_core_recovery_started_event_new(void);

G_END_DECLS
