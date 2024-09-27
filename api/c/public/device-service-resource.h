//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2024 Comcast Corporation
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
// Comcast Corporation retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------

/*
 * Created by Thomas Lea on 5/10/2024.
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define B_DEVICE_SERVICE_RESOURCE_TYPE (b_device_service_resource_get_type())
G_DECLARE_FINAL_TYPE(BDeviceServiceResource, b_device_service_resource, B_DEVICE_SERVICE, RESOURCE, GObject)

typedef enum
{
    B_DEVICE_SERVICE_RESOURCE_PROP_ID = 1,
    B_DEVICE_SERVICE_RESOURCE_PROP_URI,
    B_DEVICE_SERVICE_RESOURCE_PROP_ENDPOINT_ID,
    B_DEVICE_SERVICE_RESOURCE_PROP_DEVICE_UUID,
    B_DEVICE_SERVICE_RESOURCE_PROP_VALUE,
    B_DEVICE_SERVICE_RESOURCE_PROP_TYPE,
    B_DEVICE_SERVICE_RESOURCE_PROP_MODE,
    B_DEVICE_SERVICE_RESOURCE_PROP_CACHING_POLICY,
    B_DEVICE_SERVICE_RESOURCE_PROP_DATE_OF_LAST_SYNC_MILLIS,
    N_B_DEVICE_SERVICE_RESOURCE_PROPERTIES
} BDeviceServiceResourceProperty;

static const char *B_DEVICE_SERVICE_RESOURCE_PROPERTY_NAMES[] = {NULL,
                                                                 "id",
                                                                 "uri",
                                                                 "endpoint-id",
                                                                 "device-uuid",
                                                                 "value",
                                                                 "type",
                                                                 "mode",
                                                                 "caching-policy",
                                                                 "date-of-last-sync-millis"};

typedef enum
{
    B_DEVICE_SERVICE_RESOURCE_CACHING_POLICY_NEVER =
        1, // Never cache this attribute and always call the device driver to retrieve the value
    B_DEVICE_SERVICE_RESOURCE_CACHING_POLICY_ALWAYS // Always cache this attribute and never call the device driver to
                                                    // retrieve the value
} BDeviceServiceResourceCachingPolicy;

// This the old school way of creating GEnum types. We should be using G_DEFINE_ENUM_TYPE but that requires glib >=2.74
GType b_device_service_resource_caching_policy_get_type(void);
#define B_DEVICE_SERVICE_RESOURCE_CACHING_POLICY_TYPE (b_device_service_resource_caching_policy_get_type())


/**
 * b_device_service_resource_new
 *
 * @brief
 *
 * Returns: (transfer full): BDeviceServiceResource*
 */
BDeviceServiceResource *b_device_service_resource_new(void);

G_END_DECLS
