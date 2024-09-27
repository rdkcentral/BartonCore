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

/*
Properties:
  uuid: gchararray
  deviceClass: gchararray
  deviceClassVersion: guint
  uri: gchararray
  managingDeviceDriver: gchararray
  devices: GList of BDeviceServiceDevice
  resources: GList of BDeviceServiceResource
  metadata: GList of BDeviceServiceMetadata
*/

#define B_DEVICE_SERVICE_DEVICE_TYPE b_device_service_device_get_type()
G_DECLARE_FINAL_TYPE(BDeviceServiceDevice, b_device_service_device, B_DEVICE_SERVICE, DEVICE, GObject)

typedef enum
{
    B_DEVICE_SERVICE_DEVICE_PROP_UUID = 1,
    B_DEVICE_SERVICE_DEVICE_PROP_DEVICE_CLASS,
    B_DEVICE_SERVICE_DEVICE_PROP_DEVICE_CLASS_VERSION,
    B_DEVICE_SERVICE_DEVICE_PROP_URI,
    B_DEVICE_SERVICE_DEVICE_PROP_MANAGING_DEVICE_DRIVER,
    B_DEVICE_SERVICE_DEVICE_PROP_ENDPOINTS,
    B_DEVICE_SERVICE_DEVICE_PROP_RESOURCES,
    B_DEVICE_SERVICE_DEVICE_PROP_METADATA,
    N_B_DEVICE_SERVICE_DEVICE_PROPERTIES
} BDeviceServiceDeviceProperty;

static const char *B_DEVICE_SERVICE_DEVICE_PROPERTY_NAMES[] = {NULL,
                                                               "uuid",
                                                               "device-class",
                                                               "device-class-version",
                                                               "uri",
                                                               "managing-device-driver",
                                                               "endpoints",
                                                               "resources",
                                                               "metadata"};


/**
 * b_device_service_device_new
 *
 * @brief
 *
 * Returns: (transfer full): BDeviceServiceDevice*
 */
BDeviceServiceDevice *b_device_service_device_new(void);

G_END_DECLS
