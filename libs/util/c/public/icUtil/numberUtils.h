//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2020 Comcast
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
// Comcast Networks retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------

/*-----------------------------------------------
 * numberUtils.h
 *
 * Utilities for numbers
 *
 * Author: eInfochips
 *-----------------------------------------------*/
#ifndef ZILKER_NUMBERUTILS_H
#define ZILKER_NUMBERUTILS_H

/*
 * Generate random number between given range
 * NOTE: caller is responsible for passing first arg value lower than second arg.
 */
bool generateRandomNumberInRange(uint32_t lowerRange, uint32_t higherRange, uint64_t *randomNumber);

#endif // ZILKER_NUMBERUTILS_H