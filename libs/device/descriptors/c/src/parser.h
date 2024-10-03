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
// Created by Thomas Lea on 7/31/15.
//

#ifndef FLEXCORE_PARSER_H
#define FLEXCORE_PARSER_H

#include <icTypes/icLinkedList.h>
#include <icTypes/icStringHashMap.h>

/*
 * Parse the device descriptor list (aka allowlist) and any optional denylist at the provided paths
 * and return a list of device descriptors that are not explicitly denylisted.
 */
icLinkedList *parseDeviceDescriptors(const char *allowlistPath, const char *denylistPath);

/*
 * Allows just parsing a denylist, can be used for validating a denylist file is valid
 * @param denylistPath the path to the denylist
 * @return the denylisted uuids
 */
icStringHashMap *getDenylistedUuids(const char *denylistPath);

#endif // FLEXCORE_PARSER_H
