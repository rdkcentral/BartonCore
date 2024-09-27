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

#define B_DEVICE_SERVICE_METADATA_TYPE (b_device_service_metadata_get_type())
G_DECLARE_FINAL_TYPE(BDeviceServiceMetadata, b_device_service_metadata, B_DEVICE_SERVICE, METADATA, GObject)

typedef enum
{
    B_DEVICE_SERVICE_METADATA_PROP_ID = 1,
    B_DEVICE_SERVICE_METADATA_PROP_URI,
    B_DEVICE_SERVICE_METADATA_PROP_ENDPOINT_ID,
    B_DEVICE_SERVICE_METADATA_PROP_DEVICE_UUID,
    B_DEVICE_SERVICE_METADATA_PROP_VALUE,
    N_B_DEVICE_SERVICE_METADATA_PROPERTIES
} BDeviceServiceMetadataProperty;

static const char *B_DEVICE_SERVICE_METADATA_PROPERTY_NAMES[] =
    {NULL, "id", "uri", "endpoint-id", "device-uuid", "value"};


/**
 * b_device_service_metadata_new
 *
 * @brief
 *
 * Returns: (transfer full): BDeviceServiceMetadata*
 */
BDeviceServiceMetadata *b_device_service_metadata_new(void);

G_END_DECLS
