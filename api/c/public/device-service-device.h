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
  devices: GList of BDeviceServiceDevice
  resources: GList of BDeviceServiceResource
  metadata: GList of BDeviceServiceMetadata
*/

#define B_DEVICE_SERVICE_DEVICE_TYPE b_device_service_device_get_type()
G_DECLARE_FINAL_TYPE(BDeviceServiceDevice, b_device_service_device, B_DEVICE_SERVICE, DEVICE, GObject)

typedef enum
{
    B_DEVICE_SERVICE_DEVICE_PROP_UUID = 1,
    B_DEVICE_SERVICE_DEVICE_PROP_DEVICE_CLASS,
    B_DEVICE_SERVICE_DEVICE_PROP_DEVICE_CLASS_VERSION,
    B_DEVICE_SERVICE_DEVICE_PROP_URI,
    B_DEVICE_SERVICE_DEVICE_PROP_MANAGING_DEVICE_DRIVER,
    B_DEVICE_SERVICE_DEVICE_PROP_ENDPOINTS,
    B_DEVICE_SERVICE_DEVICE_PROP_RESOURCES,
    B_DEVICE_SERVICE_DEVICE_PROP_METADATA,
    N_B_DEVICE_SERVICE_DEVICE_PROPERTIES
} BDeviceServiceDeviceProperty;

static const char *B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[] = {NULL,
                                                               "uuid",
                                                               "device-class",
                                                               "device-class-version",
                                                               "uri",
                                                               "managing-device-driver",
                                                               "endpoints",
                                                               "resources",
                                                               "metadata"};


/**
 * b_device_service_device_new
 *
 * @brief
 *
 * Returns: (transfer full): BDeviceServiceDevice*
 */
BDeviceServiceDevice *b_device_service_device_new(void);

G_END_DECLS
