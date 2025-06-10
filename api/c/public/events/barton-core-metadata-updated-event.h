//------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2025 Comcast Cable Communications Management, LLC
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
 * Created by Micah Koch on 1/14/2025.
 */

#include "events/barton-core-event.h"

G_BEGIN_DECLS

#define B_CORE_METADATA_UPDATED_EVENT_TYPE (b_core_metadata_updated_event_get_type())
G_DECLARE_FINAL_TYPE(BCoreMetadataUpdatedEvent,
                     b_core_metadata_updated_event,
                     B_CORE,
                     METADATA_UPDATED_EVENT,
                     BCoreEvent)

typedef enum
{
    B_CORE_METADATA_UPDATED_EVENT_PROP_METADATA = 1, // The BCoreMetadata that was updated

    N_B_CORE_METADATA_UPDATED_EVENT_PROPERTIES
} BCoreMetadataUpdatedEventProperty;

static const char *B_CORE_METADATA_UPDATED_EVENT_PROPERTY_NAMES[] = {NULL, "metadata"};

/**
 * b_core_metadata_updated_event_new
 *
 * @brief
 *
 * Returns: (transfer full): BCoreMetadataUpdatedEvent*
 */
BCoreMetadataUpdatedEvent *b_core_metadata_updated_event_new(void);

G_END_DECLS
