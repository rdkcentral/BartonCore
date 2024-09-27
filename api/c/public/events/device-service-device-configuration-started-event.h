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
 * Created by Christian Leithner on 7/1/2024.
 */

#pragma once

#include "events/device-service-device-configuration-event.h"
#include "glib-object.h"

G_BEGIN_DECLS

#define B_DEVICE_SERVICE_DEVICE_CONFIGURATION_STARTED_EVENT_TYPE                                                       \
    (b_device_service_device_configuration_started_event_get_type())

G_DECLARE_FINAL_TYPE(BDeviceServiceDeviceConfigurationStartedEvent,
                     b_device_service_device_configuration_started_event,
                     B_DEVICE_SERVICE,
                     DEVICE_CONFIGURATION_STARTED_EVENT,
                     BDeviceServiceDeviceConfigurationEvent)


// No properties


/**
 * b_device_service_device_configuration_started_event_new
 *
 * @brief
 *
 * Returns: (transfer full): BDeviceServiceDeviceConfigurationStartedEvent*
 */
BDeviceServiceDeviceConfigurationStartedEvent *b_device_service_device_configuration_started_event_new(void);

G_END_DECLS