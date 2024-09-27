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
 * Created by Thomas Lea on 6/10/2024.
 */

#pragma once

#include "events/device-service-event.h"
#include <glib-object.h>

G_BEGIN_DECLS

#define B_DEVICE_SERVICE_STATUS_EVENT_TYPE (b_device_service_status_event_get_type())
G_DECLARE_FINAL_TYPE(BDeviceServiceStatusEvent,
                     b_device_service_status_event,
                     B_DEVICE_SERVICE,
                     STATUS_EVENT,
                     BDeviceServiceEvent)

typedef enum
{
    B_DEVICE_SERVICE_STATUS_CHANGED_REASON_INVALID = 0,
    B_DEVICE_SERVICE_STATUS_CHANGED_REASON_READY_FOR_PAIRING,
    B_DEVICE_SERVICE_STATUS_CHANGED_REASON_READY_FOR_DEVICE_OPERATION,
    B_DEVICE_SERVICE_STATUS_CHANGED_REASON_SUBSYSTEM_STATUS
} BDeviceServiceStatusChangedReason;

// This the old school way of creating GEnum types. We should be using G_DEFINE_ENUM_TYPE but that requires glib >=2.74
GType b_device_service_status_changed_reason_get_type(void);
#define B_DEVICE_SERVICE_STATUS_CHANGED_REASON_TYPE (b_device_service_status_changed_reason_get_type())

typedef enum
{
    B_DEVICE_SERVICE_STATUS_EVENT_PROP_STATUS = 1, // BDeviceServiceStatus object
    B_DEVICE_SERVICE_STATUS_EVENT_PROP_REASON,     // BDeviceServiceStatusChangedReason enum
    N_B_DEVICE_SERVICE_STATUS_EVENT_PROPERTIES
} BDeviceServiceStatusEventProperty;

static const char *B_DEVICE_SERVICE_STATUS_EVENT_PROPERTY_NAMES[] = {NULL, "status", "reason"};

/**
 * b_device_service_status_event_new
 *
 * @brief
 *
 * Returns: (transfer full): BDeviceServiceStatusEvent*
 */
BDeviceServiceStatusEvent *b_device_service_status_event_new(void);

G_END_DECLS