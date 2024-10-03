//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2015 iControl Networks, Inc.
//
// All rights reserved.
//
// This software is protected by copyright laws of the United States
// and of foreign countries. This material may also be protected by
// patent laws of the United States and of foreign countries.
//
// This software is furnished under a license agreement and/or a
// nondisclosure agreement and may only be used or copied in accordance
// with the terms of those agreements.
//
// The mere transfer of this software does not imply any licenses of trade
// secrets, proprietary technology, copyrights, patents, trademarks, or
// any other form of intellectual property whatsoever.
//
// iControl Networks retains all ownership rights.
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
