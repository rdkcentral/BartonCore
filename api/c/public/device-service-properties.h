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
 */

// Prefix for all barton properties
#define B_DEVICE_SERVICE_BARTON_PREFIX             "barton."
// Prefix for 802.15.4 related properties
#define B_DEVICE_SERVICE_FIFTEEN_FOUR_PREFIX       "fifteenfour."

/**
 * FIFTEEN_FOUR_EUI64: (value "barton.fifteenfour.eui64")
 *
 * The local 802.15.4 eui64 address of this installation.
 */
#define B_DEVICE_SERVICE_BARTON_FIFTEEN_FOUR_EUI64 B_DEVICE_SERVICE_BARTON_PREFIX B_DEVICE_SERVICE_FIFTEEN_FOUR_PREFIX "eui64"

G_END_DECLS
