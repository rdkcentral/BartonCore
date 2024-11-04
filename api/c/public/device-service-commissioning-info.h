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
 * Created by Thomas Lea on 10/9/2024.
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define B_DEVICE_SERVICE_COMMISSIONING_INFO_TYPE (b_device_service_commissioning_info_get_type())
G_DECLARE_FINAL_TYPE(BDeviceServiceCommissioningInfo,
                     b_device_service_commissioning_info,
                     B_DEVICE_SERVICE,
                     COMMISSIONING_INFO,
                     GObject)

typedef enum
{
    B_DEVICE_SERVICE_COMMISSIONING_INFO_PROP_MANUAL_CODE = 1,
    B_DEVICE_SERVICE_COMMISSIONING_INFO_PROP_QR_CODE,
    N_B_DEVICE_SERVICE_COMMISSIONING_INFO_PROPERTIES
} BDeviceServiceCommissioningInfoProperty;

static const char *B_DEVICE_SERVICE_COMMISSIONING_INFO_PROPERTY_NAMES[] = {NULL, "manual-code", "qr-code"};

/**
 * b_device_service_commissioning_info_new
 *
 * @brief
 *
 * Returns: (transfer full): BDeviceServiceCommissioningInfo*
 */
BDeviceServiceCommissioningInfo *b_device_service_commissioning_info_new(void);

G_END_DECLS
