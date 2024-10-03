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
 * The device descriptor handler is responsible for ensuring that the latest allowlist and denylist (which provide
 * the set of device descriptors) are downloaded and available.
 *
 * Created by Thomas Lea on 10/23/15.
 */

#ifndef FLEXCORE_DEVICEDESCRIPTORHANDLER_H
#define FLEXCORE_DEVICEDESCRIPTORHANDLER_H

#include <stdbool.h>

/*
 * Callback to be called only once after startup, when we have device descriptors and are ready to pair devices
 */
typedef void (*deviceDescriptorsReadyForPairingFunc)(void);

/*
 * Callback for when device descriptors have been updated
 */
typedef void (*deviceDescriptorsUpdatedFunc)();

/*
 * Initialize the device descriptor handler. The readyForPairingCallback will be invoked when
 * we have valid descriptors.  The descriptorsUpdatedCallback will be invoked whenever the
 * white or black list changes.
 */
void deviceServiceDeviceDescriptorsInit(deviceDescriptorsReadyForPairingFunc readyForPairingCallback,
                                        deviceDescriptorsUpdatedFunc descriptorsUpdatedCallback);

/*
 * Cleanup the handler for shutdown.
 */
void deviceServiceDeviceDescriptorsDestroy();

/*
 * Process the provided allowlist URL.  This will download it if required then invoke the readyForDevicesCallback
 * if we have a valid list.
 *
 * @param allowlistUrl - the allowlist url to download from
 */
void deviceDescriptorsUpdateAllowlist(const char *allowlistUrl);

/*
 * Process the provided denylist URL.  This will download it if required.
 *
 * @param denylistUrl - the denylist url to download from
 */
void deviceDescriptorsUpdateDenylist(const char *denylistUrl);

#endif // FLEXCORE_DEVICEDESCRIPTORHANDLER_H
