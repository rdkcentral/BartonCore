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
 * Created by Thomas Lea on 6/6/2024.
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define B_CORE_STATUS_TYPE (b_core_status_get_type())
G_DECLARE_FINAL_TYPE(BCoreStatus, b_core_status, B_CORE, STATUS, GObject)

typedef enum
{
    B_CORE_STATUS_PROP_DEVICE_CLASSES = 1,
    B_CORE_STATUS_PROP_DISCOVERY_TYPE,
    B_CORE_STATUS_PROP_SEARCHING_DEVICE_CLASSES,
    B_CORE_STATUS_PROP_DISCOVERY_SECONDS,
    B_CORE_STATUS_PROP_READY_FOR_OPERATION,
    B_CORE_STATUS_PROP_READY_FOR_PAIRING,
    B_CORE_STATUS_PROP_SUBSYSTEMS,
    B_CORE_STATUS_PROP_JSON,
    N_B_CORE_STATUS_PROPERTIES
} BCoreStatusProperty;

static const char *B_CORE_STATUS_PROPERTY_NAMES[] = {NULL,
                                                               "device-classes",
                                                               "discovery-type",
                                                               "searching-device-classes",
                                                               "discovery-seconds",
                                                               "ready-for-operation",
                                                               "ready-for-pairing",
                                                               "subsystems",
                                                               "json"};
/**
 * b_core_status_new
 *
 * @brief
 *
 * Returns: (transfer full): BCoreStatus*
 */
BCoreStatus *b_core_status_new(void);

G_END_DECLS
