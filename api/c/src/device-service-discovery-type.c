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

#include "device-service-discovery-type.h"

GType b_device_service_discovery_type_get_type(void)
{
    static gsize g_define_type_id__volatile = 0;

    // Initializes the BDeviceServiceDiscoveryType enum GType only once.
    if (g_once_init_enter(&g_define_type_id__volatile))
    {
        static const GEnumValue values[] = {
            {     B_DEVICE_SERVICE_DISCOVERY_TYPE_NONE,      "B_DEVICE_SERVICE_DISCOVERY_TYPE_NONE",      "none"},
            {B_DEVICE_SERVICE_DISCOVERY_TYPE_DISCOVERY, "B_DEVICE_SERVICE_DISCOVERY_TYPE_DISCOVERY", "discovery"},
            { B_DEVICE_SERVICE_DISCOVERY_TYPE_RECOVERY,  "B_DEVICE_SERVICE_DISCOVERY_TYPE_RECOVERY",  "recovery"},
            {                                        0,                                        NULL,        NULL}
        };

        GType g_define_type_id = g_enum_register_static(g_intern_static_string("BDeviceServiceDiscoveryType"), values);
        g_once_init_leave(&g_define_type_id__volatile, g_define_type_id);
    }

    return g_define_type_id__volatile;
}
