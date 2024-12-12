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
 * Created by Thomas Lea on 8/15/15.
 */

#ifndef FLEXCORE_DEVICEMODELHELPER_H
#define FLEXCORE_DEVICEMODELHELPER_H

#include <device/icDevice.h>
#include <device/icDeviceEndpoint.h>
#include <device/icDeviceMetadata.h>
#include <device/icDeviceResource.h>
#include <device/icInitialResourceValues.h>
#include <deviceDescriptor.h>

icDevice *createDevice(const char *uuid,
                       const char *deviceClass,
                       uint8_t deviceClassVersion,
                       const char *deviceDriverName,
                       const DeviceDescriptor *dd);

icDeviceMetadata *createDeviceMetadata(icDevice *device, const char *metadataId, const char *value);

icDeviceEndpoint *createEndpoint(icDevice *device, const char *id, const char *profile, bool enabled);

icDeviceResource *createDeviceResource(icDevice *device,
                                       const char *resourceId,
                                       const char *value,
                                       const char *type,
                                       uint8_t mode,
                                       ResourceCachingPolicy cachingPolicy);

icDeviceResource *createDeviceResourceIfAvailable(icDevice *device,
                                                  const char *resourceId,
                                                  icInitialResourceValues *initialResourceValues,
                                                  const char *type,
                                                  uint8_t mode,
                                                  ResourceCachingPolicy cachingPolicy);

icDeviceResource *createEndpointResource(icDeviceEndpoint *endpoint,
                                         const char *resourceId,
                                         const char *value,
                                         const char *type,
                                         uint8_t mode,
                                         ResourceCachingPolicy cachingPolicy);

icDeviceResource *createEndpointResourceIfAvailable(icDeviceEndpoint *endpoint,
                                                    const char *resourceId,
                                                    icInitialResourceValues *initialResourceValues,
                                                    const char *type,
                                                    uint8_t mode,
                                                    ResourceCachingPolicy cachingPolicy);

icDeviceMetadata *createEndpointMetadata(icDeviceEndpoint *endpoint, const char *metadataId, const char *value);

#endif // FLEXCORE_DEVICEMODELHELPER_H
