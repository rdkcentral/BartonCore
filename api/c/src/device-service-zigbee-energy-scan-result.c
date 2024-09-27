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

//
// Created by Kevin Funderburg on 08/15/2024
//

#include "glib.h"
#include "glibconfig.h"

#include "device-service-zigbee-energy-scan-result.h"

struct _BDeviceServiceZigbeeEnergyScanResult
{
    GObject parent_instance;

    guint8 channel; // the channel this result corresponds to
    gint8 maxRssi;  // the maximum RSSI value
    gint8 minRssi;  // the minimum RSSI value
    gint8 avgRssi;  // the average RSSI value
    guint32 score;  // A quantifier for how "good" the channel is.
};

G_DEFINE_TYPE(BDeviceServiceZigbeeEnergyScanResult, b_device_service_zigbee_energy_scan_result, G_TYPE_OBJECT)

static GParamSpec *properties[N_B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROPERTIES];

static void b_device_service_zigbee_energy_scan_result_set_property(GObject *object,
                                                                    guint property_id,
                                                                    const GValue *value,
                                                                    GParamSpec *pspec)
{
    BDeviceServiceZigbeeEnergyScanResult *self = B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT(object);

    switch (property_id)
    {
        case B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_CHANNEL:
            self->channel = g_value_get_uint(value);
            break;
        case B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_RSSI_MAX:
            self->maxRssi = g_value_get_int(value);
            break;
        case B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_RSSI_MIN:
            self->minRssi = g_value_get_int(value);
            break;
        case B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_RSSI_AVG:
            self->avgRssi = g_value_get_int(value);
            break;
        case B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_SCORE:
            self->score = g_value_get_uint(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_device_service_zigbee_energy_scan_result_get_property(GObject *object,
                                                                    guint property_id,
                                                                    GValue *value,
                                                                    GParamSpec *pspec)
{
    BDeviceServiceZigbeeEnergyScanResult *self = B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT(object);

    switch (property_id)
    {
        case B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_CHANNEL:
            g_value_set_uint(value, self->channel);
            break;
        case B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_RSSI_MAX:
            g_value_set_int(value, self->maxRssi);
            break;
        case B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_RSSI_MIN:
            g_value_set_int(value, self->minRssi);
            break;
        case B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_RSSI_AVG:
            g_value_set_int(value, self->avgRssi);
            break;
        case B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_SCORE:
            g_value_set_uint(value, self->score);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void b_device_service_device_found_details_finalize(GObject *object)
{
    BDeviceServiceZigbeeEnergyScanResult *self = B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT(object);

    // no memory allocated so nothing to free

    G_OBJECT_CLASS(b_device_service_zigbee_energy_scan_result_parent_class)->finalize(object);
}

static void b_device_service_zigbee_energy_scan_result_class_init(BDeviceServiceZigbeeEnergyScanResultClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->set_property = b_device_service_zigbee_energy_scan_result_set_property;
    object_class->get_property = b_device_service_zigbee_energy_scan_result_get_property;
    object_class->finalize = b_device_service_device_found_details_finalize;

    properties[B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_CHANNEL] =
        g_param_spec_uint(B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROPERTY_NAMES
                              [B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_CHANNEL],
                          "Channel",
                          "The channel this result corresponds to",
                          11,
                          26,
                          11,
                          G_PARAM_READWRITE);

    properties[B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_RSSI_MAX] =
        g_param_spec_int(B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROPERTY_NAMES
                             [B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_RSSI_MAX],
                         "Max RSSI",
                         "The maximum RSSI value",
                         -128,
                         0,
                         0,
                         G_PARAM_READWRITE);

    properties[B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_RSSI_MIN] =
        g_param_spec_int(B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROPERTY_NAMES
                             [B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_RSSI_MIN],
                         "Min RSSI",
                         "The minimum RSSI value",
                         -128,
                         0,
                         0,
                         G_PARAM_READWRITE);

    properties[B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_RSSI_AVG] =
        g_param_spec_int(B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROPERTY_NAMES
                             [B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_RSSI_AVG],
                         "Avg RSSI",
                         "The average RSSI value",
                         -128,
                         0,
                         0,
                         G_PARAM_READWRITE);

    properties[B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_SCORE] =
        g_param_spec_uint(B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROPERTY_NAMES
                              [B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_SCORE],
                          "Score",
                          "A quantifier for how 'good' the channel is",
                          0,
                          G_MAXUINT32,
                          0,
                          G_PARAM_READWRITE);

    g_object_class_install_properties(
        object_class, N_B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROPERTIES, properties);
}

static void b_device_service_zigbee_energy_scan_result_init(BDeviceServiceZigbeeEnergyScanResult *self)
{
    self->channel = 0;
    self->maxRssi = 0;
    self->minRssi = 0;
    self->avgRssi = 0;
    self->score = 0;
}

BDeviceServiceZigbeeEnergyScanResult *b_device_service_zigbee_energy_scan_result_new(void)
{
    return B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT(
        g_object_new(B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_TYPE, NULL));
}
