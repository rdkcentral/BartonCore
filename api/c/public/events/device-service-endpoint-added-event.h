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
 * Created by Christian Leithner on 7/16/2024.
 */

#pragma once

#include "events/device-service-event.h"

G_BEGIN_DECLS

#define B_DEVICE_SERVICE_ENDPOINT_ADDED_EVENT_TYPE (b_device_service_endpoint_added_event_get_type())

G_DECLARE_FINAL_TYPE(BDeviceServiceEndpointAddedEvent,
                     b_device_service_endpoint_added_event,
                     B_DEVICE_SERVICE,
                     ENDPOINT_ADDED_EVENT,
                     BDeviceServiceEvent)

typedef enum
{
    B_DEVICE_SERVICE_ENDPOINT_ADDED_EVENT_PROP_ENDPOINT = 1, // The endpoint that was added
    B_DEVICE_SERVICE_ENDPOINT_ADDED_EVENT_PROP_DEVICE_CLASS, // The device class of the device the endpoint belongs
                                                             // to
    N_B_DEVICE_SERVICE_ENDPOINT_ADDED_EVENT_PROPERTIES,
} BDeviceServiceEndpointAddedEventProperty;

static const char *B_DEVICE_SERVICE_ENDPOINT_ADDED_EVENT_PROPERTY_NAMES[] = {NULL, "endpoint", "device-class"};

/**
 * b_device_service_endpoint_added_event_new
 *
 * @brief
 *
 * Returns: (transfer full): BDeviceServiceEndpointAddedEvent*
 */
BDeviceServiceEndpointAddedEvent *b_device_service_endpoint_added_event_new(void);

G_END_DECLS