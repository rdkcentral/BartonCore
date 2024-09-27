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

#include <gio/gio.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define B_DEVICE_SERVICE_ENDPOINT_TYPE b_device_service_endpoint_get_type()
G_DECLARE_FINAL_TYPE(BDeviceServiceEndpoint, b_device_service_endpoint, B_DEVICE_SERVICE, ENDPOINT, GObject)

typedef enum
{
    B_DEVICE_SERVICE_ENDPOINT_PROP_ID = 1,
    B_DEVICE_SERVICE_ENDPOINT_PROP_URI,
    B_DEVICE_SERVICE_ENDPOINT_PROP_PROFILE,
    B_DEVICE_SERVICE_ENDPOINT_PROP_PROFILE_VERSION,
    B_DEVICE_SERVICE_ENDPOINT_PROP_DEVICE_UUID,
    B_DEVICE_SERVICE_ENDPOINT_PROP_ENABLED,
    B_DEVICE_SERVICE_ENDPOINT_PROP_RESOURCES,
    B_DEVICE_SERVICE_ENDPOINT_PROP_METADATA,
    N_B_DEVICE_SERVICE_ENDPOINT_PROPERTIES
} BDeviceServiceEndpointProperty;

static const char *B_DEVICE_SERVICE_ENDPOINT_PROPERTY_NAMES[] =
    {NULL, "id", "uri", "profile", "profile-version", "device-uuid", "enabled", "resources", "metadata"};


/**
 * b_device_service_endpoint_new
 *
 * @brief
 *
 * Returns: (transfer full): BDeviceServiceEndpoint*
 */
BDeviceServiceEndpoint *b_device_service_endpoint_new(void);

G_END_DECLS
