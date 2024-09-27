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
 * Created by Christian Leithner on 6/10/2024.
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_TYPE b_device_service_device_found_details_get_type()
G_DECLARE_FINAL_TYPE(BDeviceServiceDeviceFoundDetails,
                     b_device_service_device_found_details,
                     B_DEVICE_SERVICE,
                     DEVICE_FOUND_DETAILS,
                     GObject)

// Note: Device found details do not convey full GObject information like BDeviceServiceEndpoint
// or BDeviceServiceMetadata. This is because these details are built up early in the discovery
// process prior to the full device model being internalized.

typedef enum
{
    B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_UUID = 1,
    B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_MANUFACTURER,
    B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_MODEL,
    B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_HARDWARE_VERSION,
    B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_FIRMWARE_VERSION,
    B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_DEVICE_CLASS,
    B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_METADATA, // GHashTable of string:string metadata key/value pairs
    B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROP_ENDPOINT_PROFILES, // GHashTable of string:string endpointId to endpoint
                                                                  // profile key/value pairs
    N_B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTIES
} BDeviceServiceDeviceFoundDetailsProperty;

static const char *B_DEVICE_SERVICE_DEVICE_FOUND_DETAILS_PROPERTY_NAMES[] = {NULL,
                                                                             "uuid",
                                                                             "manufacturer",
                                                                             "model",
                                                                             "hardware-version",
                                                                             "firmware-version",
                                                                             "device-class",
                                                                             "metadata",
                                                                             "endpoint-profiles"};

/**
 * b_device_service_device_found_details_new
 *
 * Returns: (transfer full): BDeviceServiceDeviceFoundDetails*
 */
BDeviceServiceDeviceFoundDetails *b_device_service_device_found_details_new(void);

G_END_DECLS