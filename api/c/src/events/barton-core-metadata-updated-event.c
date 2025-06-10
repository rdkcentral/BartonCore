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

#include "events/barton-core-metadata-updated-event.h"
#include "barton-core-metadata.h"

#include <glib-object.h>

struct _BCoreMetadataUpdatedEvent
{
    BCoreEvent parent_instance;

    BCoreMetadata *metadata;
};

G_DEFINE_TYPE(BCoreMetadataUpdatedEvent, b_core_metadata_updated_event, B_CORE_EVENT_TYPE);

static GParamSpec *properties[N_B_CORE_METADATA_UPDATED_EVENT_PROPERTIES];

static void b_core_metadata_updated_event_init(BCoreMetadataUpdatedEvent *self)
{
    self->metadata = NULL;
}

static void b_core_metadata_updated_event_finalize(GObject *object)
{
    BCoreMetadataUpdatedEvent *self = B_CORE_METADATA_UPDATED_EVENT(object);

    g_clear_object(&self->metadata);

    G_OBJECT_CLASS(b_core_metadata_updated_event_parent_class)->finalize(object);
}

static void b_core_metadata_updated_event_get_property(GObject *object,
                                                                 guint property_id,
                                                                 GValue *value,
                                                                 GParamSpec *pspec)
{
    BCoreMetadataUpdatedEvent *self = B_CORE_METADATA_UPDATED_EVENT(object);

    switch (property_id)
    {
        case B_CORE_METADATA_UPDATED_EVENT_PROP_METADATA:
            g_value_set_object(value, self->metadata);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_core_metadata_updated_event_set_property(GObject *object,
                                                                 guint property_id,
                                                                 const GValue *value,
                                                                 GParamSpec *pspec)
{
    BCoreMetadataUpdatedEvent *self = B_CORE_METADATA_UPDATED_EVENT(object);

    switch (property_id)
    {
        case B_CORE_METADATA_UPDATED_EVENT_PROP_METADATA:
            g_set_object(&self->metadata, g_value_get_object(value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_core_metadata_updated_event_class_init(BCoreMetadataUpdatedEventClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = b_core_metadata_updated_event_finalize;
    object_class->get_property = b_core_metadata_updated_event_get_property;
    object_class->set_property = b_core_metadata_updated_event_set_property;

    /**
     * BCoreMetadataUpdatedEvent:metadata: (type BCoreMetadata)
     *
     * The BCoreMetadata that was updated
     */
    properties[B_CORE_METADATA_UPDATED_EVENT_PROP_METADATA] = g_param_spec_object(
        B_CORE_METADATA_UPDATED_EVENT_PROPERTY_NAMES[B_CORE_METADATA_UPDATED_EVENT_PROP_METADATA],
        "Metadata",
        "The BCoreMetadata that was updated",
        B_CORE_METADATA_TYPE,
        G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, N_B_CORE_METADATA_UPDATED_EVENT_PROPERTIES, properties);
}

BCoreMetadataUpdatedEvent *b_core_metadata_updated_event_new(void)
{
    return B_CORE_METADATA_UPDATED_EVENT(g_object_new(B_CORE_METADATA_UPDATED_EVENT_TYPE, NULL));
}
