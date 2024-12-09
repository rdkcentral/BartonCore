//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2016 iControl Networks, Inc.
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

/*-----------------------------------------------
 * tests.h
 *
 * list out the tests to run (from main)
 *
 * Author: jelderton -  9/15/16.
 *-----------------------------------------------*/

#ifndef FLEXCORE_TESTS_H
#define FLEXCORE_TESTS_H

#include <stdbool.h>

bool testProtectConfig();

bool obfuscateTest();

int runStorageTest();

#endif // FLEXCORE_TESTS_H
