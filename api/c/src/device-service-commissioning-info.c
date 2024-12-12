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

#include "device-service-commissioning-info.h"
#include <inttypes.h>

struct _BDeviceServiceCommissioningInfo
{
    GObject parent_instance;

    gchar *manual_code;
    gchar *qr_code;
};

G_DEFINE_TYPE(BDeviceServiceCommissioningInfo, b_device_service_commissioning_info, G_TYPE_OBJECT)

static void b_device_service_commissioning_info_set_property(GObject *object,
                                                             guint property_id,
                                                             const GValue *value,
                                                             GParamSpec *pspec)
{
    BDeviceServiceCommissioningInfo *info = B_DEVICE_SERVICE_COMMISSIONING_INFO(object);

    switch (property_id)
    {
        case B_DEVICE_SERVICE_COMMISSIONING_INFO_PROP_MANUAL_CODE:
            g_free(info->manual_code);
            info->manual_code = g_value_dup_string(value);
            break;
        case B_DEVICE_SERVICE_COMMISSIONING_INFO_PROP_QR_CODE:
            g_free(info->qr_code);
            info->qr_code = g_value_dup_string(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
b_device_service_commissioning_info_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    BDeviceServiceCommissioningInfo *info = B_DEVICE_SERVICE_COMMISSIONING_INFO(object);

    switch (property_id)
    {
        case B_DEVICE_SERVICE_COMMISSIONING_INFO_PROP_MANUAL_CODE:
            g_value_set_string(value, info->manual_code);
            break;
        case B_DEVICE_SERVICE_COMMISSIONING_INFO_PROP_QR_CODE:
            g_value_set_string(value, info->qr_code);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_device_service_commissioning_info_finalize(GObject *object)
{
    BDeviceServiceCommissioningInfo *info = B_DEVICE_SERVICE_COMMISSIONING_INFO(object);

    g_free(info->manual_code);
    g_free(info->qr_code);

    G_OBJECT_CLASS(b_device_service_commissioning_info_parent_class)->finalize(object);
}

static void b_device_service_commissioning_info_class_init(BDeviceServiceCommissioningInfoClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->set_property = b_device_service_commissioning_info_set_property;
    object_class->get_property = b_device_service_commissioning_info_get_property;
    object_class->finalize = b_device_service_commissioning_info_finalize;

    /*
     * Define properties
     */
    g_object_class_install_property(
        object_class,
        B_DEVICE_SERVICE_COMMISSIONING_INFO_PROP_MANUAL_CODE,
        g_param_spec_string(
            B_DEVICE_SERVICE_COMMISSIONING_INFO_PROPERTY_NAMES[B_DEVICE_SERVICE_COMMISSIONING_INFO_PROP_MANUAL_CODE],
            "Manual Code",
            "Manual Code",
            NULL,
            G_PARAM_READWRITE));
    g_object_class_install_property(
        object_class,
        B_DEVICE_SERVICE_COMMISSIONING_INFO_PROP_QR_CODE,
        g_param_spec_string(
            B_DEVICE_SERVICE_COMMISSIONING_INFO_PROPERTY_NAMES[B_DEVICE_SERVICE_COMMISSIONING_INFO_PROP_QR_CODE],
            "QR Code",
            "QR Code",
            NULL,
            G_PARAM_READWRITE));
}

static void b_device_service_commissioning_info_init(BDeviceServiceCommissioningInfo *info)
{
    info->manual_code = NULL;
    info->qr_code = NULL;
}

BDeviceServiceCommissioningInfo *b_device_service_commissioning_info_new(void)
{
    return g_object_new(B_DEVICE_SERVICE_COMMISSIONING_INFO_TYPE, NULL);
}
