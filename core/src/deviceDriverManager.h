//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2015 iControl Networks, Inc.
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
// iControl Networks retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------

/*
 * The device driver manager handles the registration and interactions with the
 * various device drivers, each of which is responsible for understanding how to
 * interact with various device classes.
 *
 * Created by Thomas Lea on 7/28/15.
 */

#ifndef FLEXCORE_DEVICEDRIVERMANAGER_H
#define FLEXCORE_DEVICEDRIVERMANAGER_H

#include <device-driver/device-driver.h>
#include <stdbool.h>

bool deviceDriverManagerInitialize();
bool deviceDriverManagerStartDeviceDrivers();
bool deviceDriverManagerShutdown();
DeviceDriver *deviceDriverManagerGetDeviceDriver(const char *driverName);
icLinkedList *deviceDriverManagerGetDeviceDriversByDeviceClass(const char *deviceClass);
icLinkedList *deviceDriverManagerGetDeviceDrivers();
icLinkedList *deviceDriverManagerGetDeviceDriversBySubsystem(const char *subsystem);

#endif //FLEXCORE_DEVICEDRIVERMANAGER_H
