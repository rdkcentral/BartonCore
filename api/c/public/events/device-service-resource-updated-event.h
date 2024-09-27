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
 * Created by Christian Leithner on 7/15/2024.
 */

#pragma once

#include "events/device-service-event.h"

G_BEGIN_DECLS

#define B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_TYPE (b_device_service_resource_updated_event_get_type())
G_DECLARE_FINAL_TYPE(BDeviceServiceResourceUpdatedEvent,
                     b_device_service_resource_updated_event,
                     B_DEVICE_SERVICE,
                     RESOURCE_UPDATED_EVENT,
                     BDeviceServiceEvent)

typedef enum
{
    B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROP_RESOURCE = 1, // The BDeviceServiceResource that was updated
    B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROP_METADATA, // A json string representing extra metadata describing this
                                                           // event
    N_B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROPERTIES
} BDeviceServiceResourceUpdatedEventProperty;

static const char *B_DEVICE_SERVICE_RESOURCE_UPDATED_EVENT_PROPERTY_NAMES[] = {NULL, "resource", "metadata"};

/**
 * b_device_service_resource_updated_event_new
 *
 * @brief
 *
 * Returns: (transfer full): BDeviceServiceResourceUpdatedEvent*
 */
BDeviceServiceResourceUpdatedEvent *b_device_service_resource_updated_event_new(void);

G_END_DECLS