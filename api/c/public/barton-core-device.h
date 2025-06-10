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
 * Created by Thomas Lea on 5/10/2024.
 */

#pragma once

#include <gio/gio.h>
#include <glib-object.h>

G_BEGIN_DECLS

/*
Properties:
  uuid: gchararray
  deviceClass: gchararray
  deviceClassVersion: guint
  uri: gchararray
  managingDeviceDriver: gchararray
  devices: GList of BCoreDevice
  resources: GList of BCoreResource
  metadata: GList of BCoreMetadata
*/

#define B_CORE_DEVICE_TYPE b_core_device_get_type()
G_DECLARE_FINAL_TYPE(BCoreDevice, b_core_device, B_CORE, DEVICE, GObject)

typedef enum
{
    B_CORE_DEVICE_PROP_UUID = 1,
    B_CORE_DEVICE_PROP_DEVICE_CLASS,
    B_CORE_DEVICE_PROP_DEVICE_CLASS_VERSION,
    B_CORE_DEVICE_PROP_URI,
    B_CORE_DEVICE_PROP_MANAGING_DEVICE_DRIVER,
    B_CORE_DEVICE_PROP_ENDPOINTS,
    B_CORE_DEVICE_PROP_RESOURCES,
    B_CORE_DEVICE_PROP_METADATA,
    N_B_CORE_DEVICE_PROPERTIES
} BCoreDeviceProperty;

static const char *B_CORE_DEVICE_PROPERTY_NAMES[] = {NULL,
                                                               "uuid",
                                                               "device-class",
                                                               "device-class-version",
                                                               "uri",
                                                               "managing-device-driver",
                                                               "endpoints",
                                                               "resources",
                                                               "metadata"};


/**
 * b_core_device_new
 *
 * @brief
 *
 * Returns: (transfer full): BCoreDevice*
 */
BCoreDevice *b_core_device_new(void);

G_END_DECLS
