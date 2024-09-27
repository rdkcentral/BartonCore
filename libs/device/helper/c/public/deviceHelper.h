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
char* createDeviceUri(const char *deviceUuid);

/*
 * Helper to create a URI to an endpoint on a device.
 *
 * Caller frees result.
 */
char* createEndpointUri(const char *deviceUuid, const char *endpointId);

/*
 * Helper to create a URI to an resource on a device.
 *
 * Caller frees result.
 */
char* createDeviceResourceUri(const char *deviceUuid, const char *resourceId);

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
char* createEndpointResourceUri(const char *deviceUuid, const char *endpointId, const char *resourceId);

/*
 * Helper to create a URI to a metadata item on an endpoint.
 *
 * Caller frees result.
 */
char* createEndpointMetadataUri(const char *deviceUuid, const char *endpointId, const char *metadataId);

#endif //FLEXCORE_DEVICEHELPER_H
