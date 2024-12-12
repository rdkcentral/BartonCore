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

typedef enum
{
    B_DEVICE_SERVICE_DISCOVERY_TYPE_NONE = 1,
    B_DEVICE_SERVICE_DISCOVERY_TYPE_DISCOVERY,
    B_DEVICE_SERVICE_DISCOVERY_TYPE_RECOVERY
} BDeviceServiceDiscoveryType;

// This the old school way of creating GEnum types. We should be using G_DEFINE_ENUM_TYPE but that requires glib >=2.74
GType b_device_service_discovery_type_get_type(void);
#define B_DEVICE_SERVICE_DISCOVERY_TYPE_TYPE (b_device_service_discovery_type_get_type())

G_END_DECLS
