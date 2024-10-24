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
 * Created by Christian Leithner on 8/29/2024.
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

/*
 * An empty default implementation of the property provider interface. Only exists so that
 * device service can use one if a client fails to provide an implementation.
 */
#define B_DEVICE_SERVICE_DEFAULT_PROPERTY_PROVIDER_TYPE (b_device_service_default_property_provider_get_type())
G_DECLARE_FINAL_TYPE(BDeviceServiceDefaultPropertyProvider,
                     b_device_service_default_property_provider,
                     B_DEVICE_SERVICE,
                     DEFAULT_PROPERTY_PROVIDER,
                     GObject);

/**
 * b_device_service_default_property_provider_new:
 *
 * @brief Creates a new instance of the default property provider.
 *
 * Returns: (transfer full): A new instance of the default property provider.
 */
BDeviceServiceDefaultPropertyProvider *b_device_service_default_property_provider_new(void);

G_END_DECLS
