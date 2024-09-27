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
 * Created by Thomas Lea on 5/16/2024.
 */

#pragma once

#include <stdbool.h>
#include "device-driver.h"

/**
 * @brief - Add a device driver to the device driver manager. This must only be called in a constructor attribute
 * function. It will be unregistered at process shutdown.
 *
 * @param driver
 * @return true
 * @return false
 */
bool deviceDriverManagerRegisterDriver(DeviceDriver *driver);
