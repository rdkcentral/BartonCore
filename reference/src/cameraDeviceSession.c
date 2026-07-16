//------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2026 Comcast Cable Communications Management, LLC
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

#include "cameraDeviceSession.h"
#include "barton-core-resource.h"
#include "events/barton-core-resource-updated-event.h"
#include <jsonHelper/jsonHelper.h>

struct _CameraDeviceSession
{
    BCoreClient *client;
    gchar *deviceId;

    CameraDeviceOnRemoteSdp onRemoteSdp;
    CameraDeviceOnRemoteIce onRemoteIce;
    CameraDeviceOnSessionError onError;
    gpointer userData;

    gulong handlerId;
    gchar *sessionId;
    gchar *protocol;   // from the stream execute result
    gchar *entryPoint; // from the stream execute result
    gchar *role;       // 'offerer' | 'answerer', from the stream execute result
    gboolean opened;
};

// ============================================================================
// Resource-updated event handler
// ============================================================================

static void onResourceUpdated(BCoreClient *source, BCoreResourceUpdatedEvent *event, gpointer userData)
{
    (void) source;
    CameraDeviceSession *self = (CameraDeviceSession *) userData;

    g_autoptr(BCoreResource) resource = NULL;
    g_autofree gchar *metadata = NULL;

    g_object_get(G_OBJECT(event),
                 B_CORE_RESOURCE_UPDATED_EVENT_PROPERTY_NAMES[B_CORE_RESOURCE_UPDATED_EVENT_PROP_RESOURCE],
                 &resource,
                 B_CORE_RESOURCE_UPDATED_EVENT_PROPERTY_NAMES[B_CORE_RESOURCE_UPDATED_EVENT_PROP_METADATA],
                 &metadata,
                 NULL);

    if (resource == NULL)
    {
        return;
    }

    g_autofree gchar *uri = NULL;
    g_autofree gchar *value = NULL;

    g_object_get(resource,
                 B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_URI],
                 &uri,
                 B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_VALUE],
                 &value,
                 NULL);

    if (uri == NULL)
    {
        return;
    }

    g_autofree gchar *remoteSdpUri = g_strdup_printf("/%s/ep/webrtc/r/remoteSdp", self->deviceId);
    g_autofree gchar *remoteIceUri = g_strdup_printf("/%s/ep/webrtc/r/remoteIceCandidates", self->deviceId);
    g_autofree gchar *webrtcErrorUri = g_strdup_printf("/%s/ep/webrtc/r/webrtcError", self->deviceId);

    if (g_strcmp0(uri, remoteSdpUri) == 0 && value != NULL)
    {
        if (self->onRemoteSdp != NULL)
        {
            self->onRemoteSdp(value, self->userData);
        }
    }
    else if (g_strcmp0(uri, remoteIceUri) == 0 && value != NULL)
    {
        if (self->onRemoteIce != NULL)
        {
            self->onRemoteIce(value, self->userData);
        }
    }
    else if (g_strcmp0(uri, webrtcErrorUri) == 0)
    {
        // Any webrtcError event (ended/failed) is a session-terminated signal. Surface the
        // human-readable detail from the metadata, falling back to the event value.
        if (self->onError != NULL)
        {
            g_autofree gchar *reason = NULL;

            if (metadata != NULL)
            {
                scoped_cJSON *meta = cJSON_Parse(metadata);
                cJSON *detail = cJSON_IsObject(meta) ? cJSON_GetObjectItem(meta, "detail") : NULL;

                if (cJSON_IsString(detail))
                {
                    reason = g_strdup(detail->valuestring);
                }
            }

            self->onError(reason != NULL ? reason : (value != NULL ? value : "session ended"), self->userData);
        }
    }
}

// ============================================================================
// Public API
// ============================================================================

CameraDeviceSession *cameraDeviceSessionCreate(BCoreClient *client,
                                               const gchar *deviceId,
                                               CameraDeviceOnRemoteSdp onRemoteSdp,
                                               CameraDeviceOnRemoteIce onRemoteIce,
                                               CameraDeviceOnSessionError onError,
                                               gpointer userData)
{
    if (client == NULL || deviceId == NULL)
    {
        return NULL;
    }

    CameraDeviceSession *self = g_new0(CameraDeviceSession, 1);
    self->client = client;
    self->deviceId = g_strdup(deviceId);
    self->onRemoteSdp = onRemoteSdp;
    self->onRemoteIce = onRemoteIce;
    self->onError = onError;
    self->userData = userData;

    self->handlerId =
        g_signal_connect(client, B_CORE_CLIENT_SIGNAL_NAME_RESOURCE_UPDATED, G_CALLBACK(onResourceUpdated), self);

    return self;
}

bool cameraDeviceSessionOpen(CameraDeviceSession *session)
{
    if (session == NULL)
    {
        return false;
    }

    g_autofree gchar *uri = g_strdup_printf("/%s/ep/camera/r/createSession", session->deviceId);
    g_free(session->sessionId);
    session->sessionId = NULL;

    session->opened = b_core_client_execute_resource(session->client, uri, NULL, &session->sessionId);

    return session->opened;
}

bool cameraDeviceSessionStartStream(CameraDeviceSession *session)
{
    if (session == NULL)
    {
        return false;
    }

    g_autofree gchar *uri = g_strdup_printf("/%s/ep/camera/r/stream", session->deviceId);
    g_autofree gchar *result = NULL;

    if (!b_core_client_execute_resource(session->client, uri, session->sessionId, &result))
    {
        return false;
    }

    // The stream execute returns { protocol, entryPoint } identifying the active protocol
    // and the resource URI to begin signaling on. The client uses these rather than assuming
    // a hard-coded protocol/URI.
    if (result != NULL)
    {
        scoped_cJSON *info = cJSON_Parse(result);

        if (cJSON_IsObject(info))
        {
            cJSON *protocol = cJSON_GetObjectItem(info, "protocol");
            cJSON *entryPoint = cJSON_GetObjectItem(info, "entryPoint");

            if (cJSON_IsString(protocol))
            {
                g_free(session->protocol);
                session->protocol = g_strdup(protocol->valuestring);
            }

            if (cJSON_IsString(entryPoint))
            {
                g_free(session->entryPoint);
                session->entryPoint = g_strdup(entryPoint->valuestring);
            }
        }
    }

    return true;
}

const gchar *cameraDeviceSessionGetProtocol(CameraDeviceSession *session)
{
    return session != NULL ? session->protocol : NULL;
}

const gchar *cameraDeviceSessionGetEntryPoint(CameraDeviceSession *session)
{
    return session != NULL ? session->entryPoint : NULL;
}

const gchar *cameraDeviceSessionGetRole(CameraDeviceSession *session)
{
    if (session == NULL)
    {
        return NULL;
    }

    // The negotiation role is a WebRTC concept, so it lives on the webrtc endpoint rather than in
    // the abstract stream result. Read it lazily on first request and cache it for the session.
    if (session->role == NULL)
    {
        g_autofree gchar *uri = g_strdup_printf("/%s/ep/webrtc/r/negotiationRole", session->deviceId);
        g_autoptr(GError) err = NULL;
        gchar *value = b_core_client_read_resource(session->client, uri, &err);

        if (value != NULL)
        {
            session->role = value;
        }
    }

    return session->role;
}

bool cameraDeviceSessionSendOffer(CameraDeviceSession *session, const gchar *sdp)
{
    if (session == NULL)
    {
        return false;
    }

    // Prefer the entry point the driver reported from the stream execute; fall back to the
    // conventional webrtc offer URI if it was unavailable.
    g_autofree gchar *fallback = NULL;
    const gchar *uri = session->entryPoint;

    if (uri == NULL)
    {
        fallback = g_strdup_printf("/%s/ep/webrtc/r/localSdp", session->deviceId);
        uri = fallback;
    }

    return b_core_client_execute_resource(session->client, uri, sdp, NULL);
}

bool cameraDeviceSessionSendIceCandidates(CameraDeviceSession *session, const gchar *jsonCandidates)
{
    if (session == NULL)
    {
        return false;
    }

    g_autofree gchar *uri = g_strdup_printf("/%s/ep/webrtc/r/localIceCandidates", session->deviceId);

    return b_core_client_execute_resource(session->client, uri, jsonCandidates, NULL);
}

void cameraDeviceSessionDestroy(CameraDeviceSession *session)
{
    if (session == NULL)
    {
        return;
    }

    // End the session on the device so it is never leaked if it was opened.
    if (session->opened)
    {
        g_autofree gchar *uri = g_strdup_printf("/%s/ep/camera/r/destroySession", session->deviceId);
        b_core_client_execute_resource(session->client, uri, session->sessionId, NULL);
    }

    if (session->handlerId != 0)
    {
        g_signal_handler_disconnect(session->client, session->handlerId);
    }

    g_free(session->sessionId);
    g_free(session->protocol);
    g_free(session->entryPoint);
    g_free(session->role);
    g_free(session->deviceId);
    g_free(session);
}
