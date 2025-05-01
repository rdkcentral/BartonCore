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

#include <glib-object.h>

G_BEGIN_DECLS

#define B_CORE_METADATA_TYPE (b_core_metadata_get_type())
G_DECLARE_FINAL_TYPE(BCoreMetadata, b_core_metadata, B_CORE, METADATA, GObject)

typedef enum
{
    B_CORE_METADATA_PROP_ID = 1,
    B_CORE_METADATA_PROP_URI,
    B_CORE_METADATA_PROP_ENDPOINT_ID,
    B_CORE_METADATA_PROP_DEVICE_UUID,
    B_CORE_METADATA_PROP_VALUE,
    N_B_CORE_METADATA_PROPERTIES
} BCoreMetadataProperty;

static const char *B_CORE_METADATA_PROPERTY_NAMES[] =
    {NULL, "id", "uri", "endpoint-id", "device-uuid", "value"};


/**
 * b_core_metadata_new
 *
 * @brief
 *
 * Returns: (transfer full): BCoreMetadata*
 */
BCoreMetadata *b_core_metadata_new(void);

G_END_DECLS
