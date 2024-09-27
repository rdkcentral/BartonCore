//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2024 Comcast
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
// Comcast retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------

/*
 * Created by Christian Leithner on 6/5/2024.
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define B_DEVICE_SERVICE_DISCOVERY_FILTER_TYPE b_device_service_discovery_filter_get_type()
G_DECLARE_FINAL_TYPE(BDeviceServiceDiscoveryFilter,
                     b_device_service_discovery_filter,
                     B_DEVICE_SERVICE,
                     DISCOVERY_FILTER,
                     GObject)

typedef enum
{
    B_DEVICE_SERVICE_DISCOVERY_FILTER_PROP_URI = 1, // Regex string describing a URI of a resource
    B_DEVICE_SERVICE_DISCOVERY_FILTER_PROP_VALUE,   // Regex string describing a resource value
    N_B_DEVICE_SERVICE_DISCOVERY_FILTER_PROPERTIES
} BDeviceServiceDiscoveryFilterProperty;

static const char *B_DEVICE_SERVICE_DISCOVERY_FILTER_PROPERTY_NAMES[] = {NULL, "uri", "value"};

/**
 * b_device_service_discovery_filter_new
 *
 * @brief
 *
 * Returns: (transfer full): BDeviceServiceDiscoveryFilter*
 */
BDeviceServiceDiscoveryFilter *b_device_service_discovery_filter_new(void);

G_END_DECLS