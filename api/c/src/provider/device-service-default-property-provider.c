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

#include "provider/device-service-default-property-provider.h"
#include "provider/device-service-property-provider.h"

struct _BDeviceServiceDefaultPropertyProvider
{
    GObject parent_instance;
};

static void b_device_service_property_provider_interface_init(BDeviceServicePropertyProviderInterface *iface);

G_DEFINE_TYPE_WITH_CODE(BDeviceServiceDefaultPropertyProvider,
                        b_device_service_default_property_provider,
                        G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE(B_DEVICE_SERVICE_PROPERTY_PROVIDER_TYPE,
                                              b_device_service_default_property_provider_init))

static void b_device_service_property_provider_interface_init(BDeviceServicePropertyProviderInterface *iface)
{
    iface->get_property = NULL;
}

static void b_device_service_default_property_provider_init(BDeviceServiceDefaultPropertyProvider *self)
{
    // Nothing to do here
}

static void b_device_service_default_property_provider_class_init(BDeviceServiceDefaultPropertyProviderClass *klass)
{
    // Nothing to do here
}

BDeviceServiceDefaultPropertyProvider *b_device_service_default_property_provider_new(void)
{
    return g_object_new(B_DEVICE_SERVICE_DEFAULT_PROPERTY_PROVIDER_TYPE, NULL);
}