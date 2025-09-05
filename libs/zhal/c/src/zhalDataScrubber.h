//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2025 Comcast Corporation
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
 * Created by Raiyan Chowdhury on 2/21/25.
 */

#pragma once

#include <cjson/cJSON.h>

/**
 * Convert the given cJSON object to a string for logging and filter out any sensitive data.
 *
 * @param str The string to scrub
 * @return Pointer to a heap allocated string with the sensitive data replaced
 */
char *zhalFilterJsonForLog(const cJSON *event);
