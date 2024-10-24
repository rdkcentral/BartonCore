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
 * Created by Christian Leithner on 9/26/2024.
 */

#pragma once

#include "device-service-properties.h"

// TODO Refactor calls to gobject layer for properties to calls here instead.
//  @see BARTON-136

// Internal mapping to GObject property keys
#define LOCAL_EUI64_PROP_KEY B_DEVICE_SERVICE_BARTON_FIFTEEN_FOUR_EUI64
