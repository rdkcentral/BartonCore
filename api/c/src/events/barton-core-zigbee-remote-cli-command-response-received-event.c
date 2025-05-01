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
 * Created by Ganesh Induri on 05/27/2025.
 */

#include "events/barton-core-zigbee-remote-cli-command-response-received-event.h"

struct _BCoreZigbeeRemoteCliCommandResponseReceivedEvent
{
    BCoreEvent parent_instance;

    gchar *uuid;
    gchar *command_response;
};

G_DEFINE_TYPE(BCoreZigbeeRemoteCliCommandResponseReceivedEvent,
              b_core_zigbee_remote_cli_command_response_received_event,
              B_CORE_EVENT_TYPE)

static GParamSpec *properties[N_B_CORE_ZIGBEE_REMOTE_CLI_COMMAND_RESPONSE_RECEIVED_EVENT_PROPERTIES];

static void b_core_zigbee_remote_cli_command_response_received_event_init(
    BCoreZigbeeRemoteCliCommandResponseReceivedEvent *self)
{
    self->uuid = NULL;
    self->command_response = NULL;
}

static void b_core_zigbee_remote_cli_command_response_received_event_finalize(GObject *object)
{
    BCoreZigbeeRemoteCliCommandResponseReceivedEvent *self =
        B_CORE_ZIGBEE_REMOTE_CLI_COMMAND_RESPONSE_RECEIVED_EVENT(object);

    g_free(self->uuid);
    g_free(self->command_response);

    G_OBJECT_CLASS(b_core_zigbee_remote_cli_command_response_received_event_parent_class)->finalize(object);
}

static void b_core_zigbee_remote_cli_command_response_received_event_get_property(GObject *object,
                                                                                            guint property_id,
                                                                                            GValue *value,
                                                                                            GParamSpec *pspec)
{
    BCoreZigbeeRemoteCliCommandResponseReceivedEvent *self =
        B_CORE_ZIGBEE_REMOTE_CLI_COMMAND_RESPONSE_RECEIVED_EVENT(object);

    switch (property_id)
    {
        case B_CORE_ZIGBEE_REMOTE_CLI_COMMAND_RESPONSE_RECEIVED_EVENT_PROP_UUID:
            g_value_set_string(value, self->uuid);
            break;
        case B_CORE_ZIGBEE_REMOTE_CLI_COMMAND_RESPONSE_RECEIVED_EVENT_PROP_COMMAND_RESPONSE:
            g_value_set_string(value, self->command_response);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_core_zigbee_remote_cli_command_response_received_event_set_property(GObject *object,
                                                                                            guint property_id,
                                                                                            const GValue *value,
                                                                                            GParamSpec *pspec)
{
    BCoreZigbeeRemoteCliCommandResponseReceivedEvent *self =
        B_CORE_ZIGBEE_REMOTE_CLI_COMMAND_RESPONSE_RECEIVED_EVENT(object);

    switch (property_id)
    {
        case B_CORE_ZIGBEE_REMOTE_CLI_COMMAND_RESPONSE_RECEIVED_EVENT_PROP_UUID:
            g_free(self->uuid);
            self->uuid = g_value_dup_string(value);
            break;
        case B_CORE_ZIGBEE_REMOTE_CLI_COMMAND_RESPONSE_RECEIVED_EVENT_PROP_COMMAND_RESPONSE:
            g_free(self->command_response);
            self->command_response = g_value_dup_string(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_core_zigbee_remote_cli_command_response_received_event_class_init(
    BCoreZigbeeRemoteCliCommandResponseReceivedEventClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = b_core_zigbee_remote_cli_command_response_received_event_finalize;
    object_class->get_property = b_core_zigbee_remote_cli_command_response_received_event_get_property;
    object_class->set_property = b_core_zigbee_remote_cli_command_response_received_event_set_property;

    properties[B_CORE_ZIGBEE_REMOTE_CLI_COMMAND_RESPONSE_RECEIVED_EVENT_PROP_UUID] =
        g_param_spec_string(B_CORE_ZIGBEE_REMOTE_CLI_COMMAND_RESPONSE_RECEIVED_EVENT_PROPERTY_NAMES
                                [B_CORE_ZIGBEE_REMOTE_CLI_COMMAND_RESPONSE_RECEIVED_EVENT_PROP_UUID],
                            "UUID",
                            "The UUID of the device that sent the command response",
                            NULL,
                            G_PARAM_READWRITE);

    properties[B_CORE_ZIGBEE_REMOTE_CLI_COMMAND_RESPONSE_RECEIVED_EVENT_PROP_COMMAND_RESPONSE] =
        g_param_spec_string(
            B_CORE_ZIGBEE_REMOTE_CLI_COMMAND_RESPONSE_RECEIVED_EVENT_PROPERTY_NAMES
                [B_CORE_ZIGBEE_REMOTE_CLI_COMMAND_RESPONSE_RECEIVED_EVENT_PROP_COMMAND_RESPONSE],
            "Command Response",
            "The command response received from the Zigbee remote CLI",
            NULL,
            G_PARAM_READWRITE);

    g_object_class_install_properties(
        object_class, N_B_CORE_ZIGBEE_REMOTE_CLI_COMMAND_RESPONSE_RECEIVED_EVENT_PROPERTIES, properties);
}

BCoreZigbeeRemoteCliCommandResponseReceivedEvent *
b_core_zigbee_remote_cli_command_response_received_event_new(void)
{
    return g_object_new(B_CORE_ZIGBEE_REMOTE_CLI_COMMAND_RESPONSE_RECEIVED_EVENT_TYPE, NULL);
}
