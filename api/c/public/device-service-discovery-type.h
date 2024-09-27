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
 * Created by Thomas Lea on 6/6/2024.
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum
{
    B_DEVICE_SERVICE_DISCOVERY_TYPE_NONE = 1,
    B_DEVICE_SERVICE_DISCOVERY_TYPE_DISCOVERY,
    B_DEVICE_SERVICE_DISCOVERY_TYPE_RECOVERY
} BDeviceServiceDiscoveryType;

// This the old school way of creating GEnum types. We should be using G_DEFINE_ENUM_TYPE but that requires glib >=2.74
GType b_device_service_discovery_type_get_type(void);
#define B_DEVICE_SERVICE_DISCOVERY_TYPE_TYPE (b_device_service_discovery_type_get_type())

G_END_DECLS
