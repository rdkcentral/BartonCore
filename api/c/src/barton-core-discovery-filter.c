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
 * Created by Christian Leithner on 6/5/2024.
 */

#include "barton-core-discovery-filter.h"

struct _BCoreDiscoveryFilter
{
    GObject parent_instance;

    gchar *uri;
    gchar *value;
};

G_DEFINE_TYPE(BCoreDiscoveryFilter, b_core_discovery_filter, G_TYPE_OBJECT);

static GParamSpec *properties[N_B_CORE_DISCOVERY_FILTER_PROPERTIES];

static void b_core_discovery_filter_set_property(GObject *object,
                                                           guint property_id,
                                                           const GValue *value,
                                                           GParamSpec *pspec)
{
    BCoreDiscoveryFilter *self = B_CORE_DISCOVERY_FILTER(object);

    switch (property_id)
    {
        case B_CORE_DISCOVERY_FILTER_PROP_URI:
            g_free(self->uri);
            self->uri = g_value_dup_string(value);
            break;
        case B_CORE_DISCOVERY_FILTER_PROP_VALUE:
            g_free(self->value);
            self->value = g_value_dup_string(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
b_core_discovery_filter_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    BCoreDiscoveryFilter *self = B_CORE_DISCOVERY_FILTER(object);

    switch (property_id)
    {
        case B_CORE_DISCOVERY_FILTER_PROP_URI:
            g_value_set_string(value, self->uri);
            break;
        case B_CORE_DISCOVERY_FILTER_PROP_VALUE:
            g_value_set_string(value, self->value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_core_discovery_filter_finalize(GObject *object)
{
    BCoreDiscoveryFilter *self = B_CORE_DISCOVERY_FILTER(object);

    g_free(self->uri);
    g_free(self->value);

    G_OBJECT_CLASS(b_core_discovery_filter_parent_class)->finalize(object);
}

static void b_core_discovery_filter_class_init(BCoreDiscoveryFilterClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->set_property = b_core_discovery_filter_set_property;
    object_class->get_property = b_core_discovery_filter_get_property;
    object_class->finalize = b_core_discovery_filter_finalize;

    /**
     * BCoreDiscoveryFilter:uri: (type utf8)
     *
     * Regex string to match against the URI of the device service resource.
     */
    properties[B_CORE_DISCOVERY_FILTER_PROP_URI] = g_param_spec_string(
        B_CORE_DISCOVERY_FILTER_PROPERTY_NAMES[B_CORE_DISCOVERY_FILTER_PROP_URI],
        "URI",
        "Regex string to match against the URI of the device service resource.",
        NULL,
        G_PARAM_READWRITE);

    /**
     * BCoreDiscoveryFilter:value: (type utf8)
     *
     * Regex string to match against the value of the device service resource.
     */
    properties[B_CORE_DISCOVERY_FILTER_PROP_VALUE] = g_param_spec_string(
        B_CORE_DISCOVERY_FILTER_PROPERTY_NAMES[B_CORE_DISCOVERY_FILTER_PROP_VALUE],
        "Value",
        "Regex string to match against the value of the device service resource.",
        NULL,
        G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, N_B_CORE_DISCOVERY_FILTER_PROPERTIES, properties);
}

static void b_core_discovery_filter_init(BCoreDiscoveryFilter *self)
{
    self->uri = NULL;
    self->value = NULL;
}

BCoreDiscoveryFilter *b_core_discovery_filter_new(void)
{
    return B_CORE_DISCOVERY_FILTER(g_object_new(B_CORE_DISCOVERY_FILTER_TYPE, NULL));
}
