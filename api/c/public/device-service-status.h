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

#define B_DEVICE_SERVICE_STATUS_TYPE (b_device_service_status_get_type())
G_DECLARE_FINAL_TYPE(BDeviceServiceStatus, b_device_service_status, B_DEVICE_SERVICE, STATUS, GObject)

typedef enum
{
    B_DEVICE_SERVICE_STATUS_PROP_DEVICE_CLASSES = 1,
    B_DEVICE_SERVICE_STATUS_PROP_DISCOVERY_TYPE,
    B_DEVICE_SERVICE_STATUS_PROP_SEARCHING_DEVICE_CLASSES,
    B_DEVICE_SERVICE_STATUS_PROP_DISCOVERY_SECONDS,
    B_DEVICE_SERVICE_STATUS_PROP_READY_FOR_OPERATION,
    B_DEVICE_SERVICE_STATUS_PROP_READY_FOR_PAIRING,
    B_DEVICE_SERVICE_STATUS_PROP_SUBSYSTEMS,
    B_DEVICE_SERVICE_STATUS_PROP_JSON,
    N_B_DEVICE_SERVICE_STATUS_PROPERTIES
} BDeviceServiceStatusProperty;

static const char *B_DEVICE_SERVICE_STATUS_PROPERTY_NAMES[] = {NULL,
                                                               "device-classes",
                                                               "discovery-type",
                                                               "searching-device-classes",
                                                               "discovery-seconds",
                                                               "ready-for-operation",
                                                               "ready-for-pairing",
                                                               "subsystems",
                                                               "json"};
/**
 * b_device_service_status_new
 *
 * @brief
 *
 * Returns: (transfer full): BDeviceServiceStatus*
 */
BDeviceServiceStatus *b_device_service_status_new(void);

G_END_DECLS
