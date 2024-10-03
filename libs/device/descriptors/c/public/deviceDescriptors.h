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

//
// Created by Thomas Lea on 7/29/15.
//

#ifndef FLEXCORE_DEVICEDESCRIPTORS_H
#define FLEXCORE_DEVICEDESCRIPTORS_H

#include <deviceDescriptor.h>
#include <icTypes/icHashMap.h>
#include <icTypes/icLinkedList.h>
#include <libxml/parser.h>

/*
 * Initialize the library and provide the path to where the device descriptor files will be located.
 */
void deviceDescriptorsInit(const char *allowListPath, const char *denyListPath);

/*
 * Release all resources used by the device descriptors library.
 */
void deviceDescriptorsCleanup();

/*
 * Retrieve the matching DeviceDescriptor for the provided input or NULL if a matching one doesnt exist.
 */
DeviceDescriptor *deviceDescriptorsGet(const char *manufacturer,
                                       const char *model,
                                       const char *hardwareVersion,
                                       const char *firmwareVersion);

/*
 * Retrieve the currently configured allowlist path.
 *
 * Caller frees
 */
char *getAllowListPath();

/*
 * Retrieve the currently configured denylist path.
 *
 * Caller frees
 */
char *getDenyListPath();

/*
 * Check whether a given white list is valid/parsable.  This function does NOT require deviceDescriptorsInit to be
 * called
 * @param allowListPath the allow list file to check
 * @return true if valid, false otherwise
 */
bool checkAllowListValid(const char *allowListPath);

/*
 * Check whether a given deny list is valid/parsable.  This function does NOT require deviceDescriptorsInit to be
 * called
 * @param denyListPath the deny list file to check
 * @return true if valid, false otherwise
 */
bool checkDenyListValid(const char *denyListPath);

#endif // FLEXCORE_DEVICEDESCRIPTORS_H
