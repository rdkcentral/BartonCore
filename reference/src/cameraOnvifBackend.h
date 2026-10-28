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

/*
 * ONVIF/RTSP camera stream backend: applies credentials, optionally takes a snapshot, then pulls
 * the camera's RTSP media URL and streams it into the shared sink via GStreamer rtspsrc.
 */

#pragma once

#include "cameraStreamBackend.h"

/**
 * Create an ONVIF/RTSP stream backend.
 *
 * @param options command-line options (user/pass credentials and optional snapshot path)
 * @return the backend
 */
CameraStreamBackend *cameraOnvifBackendCreate(const CameraStreamOptions *options);
