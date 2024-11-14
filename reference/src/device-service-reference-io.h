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
 * Created by Micah Koch on 11/08/24.
 */

#pragma once

#include <stdbool.h>

typedef bool (*processLineCallback)(const char *line, void *userData);

/**
 * Blocks waiting on input from the user and manages the prompt and other async output.  In order to keep the prompt
 * from getting mangled by output users should use emitOutput/emitError to print to the terminal.
 */
void device_service_reference_io_process(bool useLinenoise, processLineCallback callback, void *userData);

/**
 * Emit some output to the terminal.  This will be displayed in a way that doesn't mangle the prompt.
 */
__attribute__((format(__printf__, 1, 2))) void emitOutput(const char *format, ...);

/**
 * Emit an error message to the terminal.  This will be displayed in a way that doesn't mangle the prompt.
 */
__attribute__((format(__printf__, 1, 2))) void emitError(const char *format, ...);
