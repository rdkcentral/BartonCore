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

//
// Created by Kevin Funderburg on 08/15/2024
//

#include "glib.h"
#include "glibconfig.h"

#include "barton-core-zigbee-energy-scan-result.h"

struct _BCoreZigbeeEnergyScanResult
{
    GObject parent_instance;

    guint channel; // the channel this result corresponds to
    gint maxRssi;  // the maximum RSSI value
    gint minRssi;  // the minimum RSSI value
    gint avgRssi;  // the average RSSI value
    guint score;   // A quantifier for how "good" the channel is.
};

G_DEFINE_TYPE(BCoreZigbeeEnergyScanResult, b_core_zigbee_energy_scan_result, G_TYPE_OBJECT)

static GParamSpec *properties[N_B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROPERTIES];

static void b_core_zigbee_energy_scan_result_set_property(GObject *object,
                                                                    guint property_id,
                                                                    const GValue *value,
                                                                    GParamSpec *pspec)
{
    BCoreZigbeeEnergyScanResult *self = B_CORE_ZIGBEE_ENERGY_SCAN_RESULT(object);

    switch (property_id)
    {
        case B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_CHANNEL:
            self->channel = g_value_get_uint(value);
            break;
        case B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_RSSI_MAX:
            self->maxRssi = g_value_get_int(value);
            break;
        case B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_RSSI_MIN:
            self->minRssi = g_value_get_int(value);
            break;
        case B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_RSSI_AVG:
            self->avgRssi = g_value_get_int(value);
            break;
        case B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_SCORE:
            self->score = g_value_get_uint(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_core_zigbee_energy_scan_result_get_property(GObject *object,
                                                                    guint property_id,
                                                                    GValue *value,
                                                                    GParamSpec *pspec)
{
    BCoreZigbeeEnergyScanResult *self = B_CORE_ZIGBEE_ENERGY_SCAN_RESULT(object);

    switch (property_id)
    {
        case B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_CHANNEL:
            g_value_set_uint(value, self->channel);
            break;
        case B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_RSSI_MAX:
            g_value_set_int(value, self->maxRssi);
            break;
        case B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_RSSI_MIN:
            g_value_set_int(value, self->minRssi);
            break;
        case B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_RSSI_AVG:
            g_value_set_int(value, self->avgRssi);
            break;
        case B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_SCORE:
            g_value_set_uint(value, self->score);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_core_device_found_details_finalize(GObject *object)
{
    BCoreZigbeeEnergyScanResult *self = B_CORE_ZIGBEE_ENERGY_SCAN_RESULT(object);

    // no memory allocated so nothing to free

    G_OBJECT_CLASS(b_core_zigbee_energy_scan_result_parent_class)->finalize(object);
}

static void b_core_zigbee_energy_scan_result_class_init(BCoreZigbeeEnergyScanResultClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->set_property = b_core_zigbee_energy_scan_result_set_property;
    object_class->get_property = b_core_zigbee_energy_scan_result_get_property;
    object_class->finalize = b_core_device_found_details_finalize;

    properties[B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_CHANNEL] =
        g_param_spec_uint(B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROPERTY_NAMES
                              [B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_CHANNEL],
                          "Channel",
                          "The channel this result corresponds to",
                          11,
                          26,
                          11,
                          G_PARAM_READWRITE);

    properties[B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_RSSI_MAX] =
        g_param_spec_int(B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROPERTY_NAMES
                             [B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_RSSI_MAX],
                         "Max RSSI",
                         "The maximum RSSI value",
                         -128,
                         0,
                         0,
                         G_PARAM_READWRITE);

    properties[B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_RSSI_MIN] =
        g_param_spec_int(B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROPERTY_NAMES
                             [B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_RSSI_MIN],
                         "Min RSSI",
                         "The minimum RSSI value",
                         -128,
                         0,
                         0,
                         G_PARAM_READWRITE);

    properties[B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_RSSI_AVG] =
        g_param_spec_int(B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROPERTY_NAMES
                             [B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_RSSI_AVG],
                         "Avg RSSI",
                         "The average RSSI value",
                         -128,
                         0,
                         0,
                         G_PARAM_READWRITE);

    properties[B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_SCORE] =
        g_param_spec_uint(B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROPERTY_NAMES
                              [B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_SCORE],
                          "Score",
                          "A quantifier for how 'good' the channel is",
                          0,
                          G_MAXUINT,
                          0,
                          G_PARAM_READWRITE);

    g_object_class_install_properties(
        object_class, N_B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROPERTIES, properties);
}

static void b_core_zigbee_energy_scan_result_init(BCoreZigbeeEnergyScanResult *self)
{
    self->channel = 0;
    self->maxRssi = 0;
    self->minRssi = 0;
    self->avgRssi = 0;
    self->score = 0;
}

BCoreZigbeeEnergyScanResult *b_core_zigbee_energy_scan_result_new(void)
{
    return B_CORE_ZIGBEE_ENERGY_SCAN_RESULT(
        g_object_new(B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_TYPE, NULL));
}
