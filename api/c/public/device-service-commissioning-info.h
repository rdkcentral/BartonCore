//------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2024 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
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
