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
 * Created by Christian Leithner on 6/10/2024.
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define B_CORE_DEVICE_FOUND_DETAILS_TYPE b_core_device_found_details_get_type()
G_DECLARE_FINAL_TYPE(BCoreDeviceFoundDetails,
                     b_core_device_found_details,
                     B_CORE,
                     DEVICE_FOUND_DETAILS,
                     GObject)

// Note: Device found details do not convey full GObject information like BCoreEndpoint
// or BCoreMetadata. This is because these details are built up early in the discovery
// process prior to the full device model being internalized.

typedef enum
{
    B_CORE_DEVICE_FOUND_DETAILS_PROP_UUID = 1,
    B_CORE_DEVICE_FOUND_DETAILS_PROP_MANUFACTURER,
    B_CORE_DEVICE_FOUND_DETAILS_PROP_MODEL,
    B_CORE_DEVICE_FOUND_DETAILS_PROP_HARDWARE_VERSION,
    B_CORE_DEVICE_FOUND_DETAILS_PROP_FIRMWARE_VERSION,
    B_CORE_DEVICE_FOUND_DETAILS_PROP_DEVICE_CLASS,
    B_CORE_DEVICE_FOUND_DETAILS_PROP_METADATA, // GHashTable of string:string metadata key/value pairs
    B_CORE_DEVICE_FOUND_DETAILS_PROP_ENDPOINT_PROFILES, // GHashTable of string:string endpointId to endpoint
                                                                  // profile key/value pairs
    N_B_CORE_DEVICE_FOUND_DETAILS_PROPERTIES
} BCoreDeviceFoundDetailsProperty;

static const char *B_CORE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[] = {NULL,
                                                                             "uuid",
                                                                             "manufacturer",
                                                                             "model",
                                                                             "hardware-version",
                                                                             "firmware-version",
                                                                             "device-class",
                                                                             "metadata",
                                                                             "endpoint-profiles"};

/**
 * b_core_device_found_details_new
 *
 * Returns: (transfer full): BCoreDeviceFoundDetails*
 */
BCoreDeviceFoundDetails *b_core_device_found_details_new(void);

G_END_DECLS
