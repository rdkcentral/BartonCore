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

// Created by Kevin Funderburg on 07/01/2026

/*
 * Property-name keys for the C++ <-> JavaScript wire contract used by SBMD
 * drivers. These names must match exactly what the .sbmd.js handlers and the
 * scriptCommon bundle read and write. A typo on the C++ side does not break
 * the build -- it surfaces as an "undefined" on the JS side -- so the keys are
 * centralized here to avoid duplication and drift
 */

#pragma once

// Handler args (built by SbmdHandlerInvoker, read by handlers)
#define SBMD_KEY_DEVICE_UUID          "deviceUuid"
#define SBMD_KEY_ENDPOINT_ID          "endpointId"
#define SBMD_KEY_CLUSTER_FEATURE_MAPS "clusterFeatureMaps"
#define SBMD_KEY_ATTRIBUTE            "attribute"
#define SBMD_KEY_EVENT                "event"
#define SBMD_KEY_COMMAND              "command"
#define SBMD_KEY_RESOURCE             "resource"
#define SBMD_KEY_CLUSTER_ID           "clusterId"
#define SBMD_KEY_ATTRIBUTE_ID         "attributeId"
#define SBMD_KEY_EVENT_ID             "eventId"
#define SBMD_KEY_COMMAND_ID           "commandId"
#define SBMD_KEY_RESOURCE_ID          "resourceId"
#define SBMD_KEY_TLV_BASE64           "tlvBase64"
#define SBMD_KEY_INPUT                "input"

// Handler context (persisted/transient state exchanged with handlers)
#define SBMD_KEY_HANDLER_CONTEXT "handlerContext"
#define SBMD_KEY_PERSISTENT_DATA "persistentData"
#define SBMD_KEY_TRANSIENT_DATA  "transientData"
#define SBMD_KEY_SUPPLEMENTS     "supplements"
#define SBMD_KEY_ATTRIBUTES      "attributes"
#define SBMD_KEY_RESOURCES       "resources"

// Handler result envelope (returned by handlers, parsed by SbmdResultExecutor)
#define SBMD_KEY_OPS      "ops"
#define SBMD_KEY_OP       "op"
#define SBMD_KEY_TERMINAL "terminal"
#define SBMD_KEY_DEFERRED "deferred"
#define SBMD_KEY_CONTEXT  "context"
#define SBMD_KEY_OPTIONS  "options"

// Op / terminal fields
#define SBMD_KEY_KEY                     "key"
#define SBMD_KEY_NAME                    "name"
#define SBMD_KEY_VALUE                   "value"
#define SBMD_KEY_TYPE                    "type"
#define SBMD_KEY_DATA                    "data"
#define SBMD_KEY_METADATA                "metadata"
#define SBMD_KEY_ENDPOINT                "endpoint"
#define SBMD_KEY_MESSAGE                 "message"
#define SBMD_KEY_ERROR                   "error"
#define SBMD_KEY_RESPONSE                "response"
#define SBMD_KEY_ON_RESPONSE             "onResponse"
#define SBMD_KEY_ON_ERROR                "onError"
#define SBMD_KEY_MATTER_CODE             "matterCode"
#define SBMD_KEY_RESPONSE_COMMAND_ID     "responseCommandId"
#define SBMD_KEY_SUCCESS_VALUE           "successValue"
#define SBMD_KEY_TIMED_INVOKE_TIMEOUT_MS "timedInvokeTimeoutMs"
#define SBMD_KEY_TIMEOUT_MS              "timeoutMs"
#define SBMD_KEY_TTL_SECS                "ttlSecs"
