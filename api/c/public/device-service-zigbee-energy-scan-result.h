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

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_TYPE b_device_service_zigbee_energy_scan_result_get_type()
G_DECLARE_FINAL_TYPE(BDeviceServiceZigbeeEnergyScanResult,
                     b_device_service_zigbee_energy_scan_result,
                     B_DEVICE_SERVICE,
                     ZIGBEE_ENERGY_SCAN_RESULT,
                     GObject)

typedef enum
{
    B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_CHANNEL = 1,
    B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_RSSI_MAX,
    B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_RSSI_MIN,
    B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_RSSI_AVG,
    B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_SCORE,

    N_B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROPERTIES
} BDeviceServiceZigbeeEnergyScanResultProperty;

static const char *B_DEVICE_SERVICE_ZIGBEE_ENERGY_SCAN_RESULT_PROPERTY_NAMES[] =
    {NULL, "channel", "max-rssi", "min-rssi", "avg-rssi", "score"};

/**
 * b_device_service_device_found_details_new
 *
 * Returns: (transfer full): BDeviceServiceDeviceFoundDetails*
 */
BDeviceServiceZigbeeEnergyScanResult *b_device_service_zigbee_energy_scan_result_new(void);

G_END_DECLS