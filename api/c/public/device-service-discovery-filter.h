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
 * Created by Christian Leithner on 6/5/2024.
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define B_DEVICE_SERVICE_DISCOVERY_FILTER_TYPE b_device_service_discovery_filter_get_type()
G_DECLARE_FINAL_TYPE(BDeviceServiceDiscoveryFilter,
                     b_device_service_discovery_filter,
                     B_DEVICE_SERVICE,
                     DISCOVERY_FILTER,
                     GObject)

typedef enum
{
    B_DEVICE_SERVICE_DISCOVERY_FILTER_PROP_URI = 1, // Regex string describing a URI of a resource
    B_DEVICE_SERVICE_DISCOVERY_FILTER_PROP_VALUE,   // Regex string describing a resource value
    N_B_DEVICE_SERVICE_DISCOVERY_FILTER_PROPERTIES
} BDeviceServiceDiscoveryFilterProperty;

static const char *B_DEVICE_SERVICE_DISCOVERY_FILTER_PROPERTY_NAMES[] = {NULL, "uri", "value"};

/**
 * b_device_service_discovery_filter_new
 *
 * @brief
 *
 * Returns: (transfer full): BDeviceServiceDiscoveryFilter*
 */
BDeviceServiceDiscoveryFilter *b_device_service_discovery_filter_new(void);

G_END_DECLS
