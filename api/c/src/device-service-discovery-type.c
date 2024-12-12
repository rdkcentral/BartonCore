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

#include "device-service-discovery-type.h"

GType b_device_service_discovery_type_get_type(void)
{
    static gsize g_define_type_id__volatile = 0;

    // Initializes the BDeviceServiceDiscoveryType enum GType only once.
    if (g_once_init_enter(&g_define_type_id__volatile))
    {
        static const GEnumValue values[] = {
            {     B_DEVICE_SERVICE_DISCOVERY_TYPE_NONE,      "B_DEVICE_SERVICE_DISCOVERY_TYPE_NONE",      "none"},
            {B_DEVICE_SERVICE_DISCOVERY_TYPE_DISCOVERY, "B_DEVICE_SERVICE_DISCOVERY_TYPE_DISCOVERY", "discovery"},
            { B_DEVICE_SERVICE_DISCOVERY_TYPE_RECOVERY,  "B_DEVICE_SERVICE_DISCOVERY_TYPE_RECOVERY",  "recovery"},
            {                                        0,                                        NULL,        NULL}
        };

        GType g_define_type_id = g_enum_register_static(g_intern_static_string("BDeviceServiceDiscoveryType"), values);
        g_once_init_leave(&g_define_type_id__volatile, g_define_type_id);
    }

    return g_define_type_id__volatile;
}
