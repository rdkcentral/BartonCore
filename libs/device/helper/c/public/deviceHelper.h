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
 * This library provides a higher level interface to device service.  It has some knowledge
 * about well known first class device types.  It also knows how to assemble URIs to access
 * various elements on devices.
 *
 * Created by Thomas Lea on 9/30/15.
 */

#ifndef FLEXCORE_DEVICEHELPER_H
#define FLEXCORE_DEVICEHELPER_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Helper to create a URI to a device.
 *
 * Caller frees result.
 */
char *createDeviceUri(const char *deviceUuid);

/*
 * Helper to create a URI to an endpoint on a device.
 *
 * Caller frees result.
 */
char *createEndpointUri(const char *deviceUuid, const char *endpointId);

/*
 * Helper to create a URI to an resource on a device.
 *
 * Caller frees result.
 */
char *createDeviceResourceUri(const char *deviceUuid, const char *resourceId);

/*
 * Helper to create a URI to an metadata on a device
 *
 * Caller frees result.
 */
char *createDeviceMetadataUri(const char *deviceUuid, const char *metadataId);

/*
 * Helper to create a URI to an resource on an endpoint.
 *
 * Caller frees result.
 */
char *createEndpointResourceUri(const char *deviceUuid, const char *endpointId, const char *resourceId);

/*
 * Helper to create a URI to a metadata item on an endpoint.
 *
 * Caller frees result.
 */
char *createEndpointMetadataUri(const char *deviceUuid, const char *endpointId, const char *metadataId);

#endif // FLEXCORE_DEVICEHELPER_H
