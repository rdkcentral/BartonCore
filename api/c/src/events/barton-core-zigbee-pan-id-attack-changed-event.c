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
 * Created by ss411 on 08/08/2024.
 */

#include "events/barton-core-zigbee-pan-id-attack-changed-event.h"

struct _BCoreZigbeePanIdAttackChangedEvent
{
    BCoreEvent parent_instance;

    gboolean attack_detected;
};

G_DEFINE_TYPE(BCoreZigbeePanIdAttackChangedEvent,
              b_core_zigbee_pan_id_attack_changed_event,
              B_CORE_EVENT_TYPE)

static GParamSpec *properties[N_B_CORE_ZIGBEE_PAN_ID_ATTACK_CHANGED_EVENT_PROPERTIES];

static void b_core_zigbee_pan_id_attack_changed_event_init(BCoreZigbeePanIdAttackChangedEvent *self)
{
    self->attack_detected = false;
}

static void b_core_zigbee_pan_id_attack_changed_event_finalize(GObject *object)
{
    G_OBJECT_CLASS(b_core_zigbee_pan_id_attack_changed_event_parent_class)->finalize(object);
}

static void b_core_zigbee_pan_id_attack_changed_event_get_property(GObject *object,
                                                                             guint property_id,
                                                                             GValue *value,
                                                                             GParamSpec *pspec)
{
    BCoreZigbeePanIdAttackChangedEvent *self = B_CORE_ZIGBEE_PAN_ID_ATTACK_CHANGED_EVENT(object);

    switch (property_id)
    {
        case B_CORE_ZIGBEE_PAN_ID_ATTACK_CHANGED_EVENT_PROP_ATTACK_DETECTED:
            g_value_set_boolean(value, self->attack_detected);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_core_zigbee_pan_id_attack_changed_event_set_property(GObject *object,
                                                                             guint property_id,
                                                                             const GValue *value,
                                                                             GParamSpec *pspec)
{
    BCoreZigbeePanIdAttackChangedEvent *self = B_CORE_ZIGBEE_PAN_ID_ATTACK_CHANGED_EVENT(object);

    switch (property_id)
    {
        case B_CORE_ZIGBEE_PAN_ID_ATTACK_CHANGED_EVENT_PROP_ATTACK_DETECTED:
            self->attack_detected = g_value_get_boolean(value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
b_core_zigbee_pan_id_attack_changed_event_class_init(BCoreZigbeePanIdAttackChangedEventClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = b_core_zigbee_pan_id_attack_changed_event_finalize;
    object_class->get_property = b_core_zigbee_pan_id_attack_changed_event_get_property;
    object_class->set_property = b_core_zigbee_pan_id_attack_changed_event_set_property;

    properties[B_CORE_ZIGBEE_PAN_ID_ATTACK_CHANGED_EVENT_PROP_ATTACK_DETECTED] =
        g_param_spec_boolean(B_CORE_ZIGBEE_PAN_ID_ATTACK_CHANGED_EVENT_PROPERTY_NAMES
                                 [B_CORE_ZIGBEE_PAN_ID_ATTACK_CHANGED_EVENT_PROP_ATTACK_DETECTED],
                             "attack-detected",
                             "If true, we have detected zigbee pan Id attack and false if attack "
                             "had occurred and has subsided",
                             FALSE,
                             G_PARAM_READWRITE);

    g_object_class_install_properties(
        object_class, N_B_CORE_ZIGBEE_PAN_ID_ATTACK_CHANGED_EVENT_PROPERTIES, properties);
}

BCoreZigbeePanIdAttackChangedEvent *b_core_zigbee_pan_id_attack_changed_event_new(void)
{
    return g_object_new(B_CORE_ZIGBEE_PAN_ID_ATTACK_CHANGED_EVENT_TYPE, NULL);
}
