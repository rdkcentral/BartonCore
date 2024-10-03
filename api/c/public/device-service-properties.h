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

#include <glib-2.0/glib.h>

G_BEGIN_DECLS

/**
 * A collection of public property keys
 */

// Prefix for all barton properties
#define BARTON_PREFIX             "barton."
// Prefix for 802.15.4 related properties
#define FIFTEEN_FOUR_PREFIX       "fifteenfour."

/**
 * FIFTEEN_FOUR_EUI64: (value "barton.fifteenfour.eui64")
 *
 * The local 802.15.4 eui64 address of this installation.
 */
#define BARTON_FIFTEEN_FOUR_EUI64 BARTON_PREFIX FIFTEEN_FOUR_PREFIX "eui64"

G_END_DECLS