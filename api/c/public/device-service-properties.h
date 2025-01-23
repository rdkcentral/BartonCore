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
 * Created by Christian Leithner on 9/26/2024.
 */

#pragma once

#include <glib-2.0/glib.h>

G_BEGIN_DECLS

/*
 * A collection of public property keys
 *
 * Property values are always stores as strings, regardles of
 * the actual type of the property data.
 *
 * Properties whose keys are defined here will document the
 * expected type of the property value. If a type is not
 * specified, the property value is expected to be a string.
 *
 * For properties that stores numerical values, the base
 * of the value can be flexible and auto-detected. However, correct
 * base prefixes should be used when setting the property
 * (e.g. "0x" for hexidecimal)
 */

// Prefix for all barton properties
#define B_DEVICE_SERVICE_BARTON_PREFIX             "barton."
// Prefix for matter properties
#define B_DEVICE_SERVICE_MATTER_PREFIX             "matter."
// Prefix for 802.15.4 related properties
#define B_DEVICE_SERVICE_FIFTEEN_FOUR_PREFIX       "fifteenfour."

/**
 * FIFTEEN_FOUR_EUI64: (value "barton.fifteenfour.eui64")
 *
 * The local 802.15.4 eui64 address of this installation.
 * Type: uint64
 */
#define B_DEVICE_SERVICE_BARTON_FIFTEEN_FOUR_EUI64 B_DEVICE_SERVICE_BARTON_PREFIX B_DEVICE_SERVICE_FIFTEEN_FOUR_PREFIX "eui64"

/**
 * MATTER_VID: (value "barton.matter.vid")
 *
 * The Vendor ID of the Matter device.
 * Type: uint16
 */
#define B_DEVICE_SERVICE_BARTON_MATTER_VID B_DEVICE_SERVICE_BARTON_PREFIX B_DEVICE_SERVICE_MATTER_PREFIX "vid"

G_END_DECLS
