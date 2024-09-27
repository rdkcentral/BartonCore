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
 * systemCommandUtils.h
 *
 * Utilities for system command
 *
 * Author: eInfochips
 *-----------------------------------------------*/

#ifndef ZILKER_SYSTEMCOMMANDUTILS_H
#define ZILKER_SYSTEMCOMMANDUTILS_H

#define executeSystemCommand(a, ...) execSystemCommand(a, ##__VA_ARGS__, NULL)

/**
 * Run a shell command (i.e., system) and return the exit status code
 * @param commandStr Any valid bourne shell command
 * @note Avoid using this if any arguments are provided by externally
 *       controlled sources (filenames, URLs, user input, etc.)
 * @return exit status code
 */
int execShellCommand(const char *commandStr);

/**
 * Fork and run a command with arguments and return the exit status code
 * @param program The program to run
 * @param ... Arguments (if any)
 * @return the exit status code
 */
int execSystemCommand(const char *program, ...);

/**
 * Fork and run a command
 * @param path path to executable
 * @param argv arguments (c strings) with NULL terminator
 * @return program exit status code, or -1 on pre-invocation error
 */
int __executeSystemCommand(const char *path, char *const argv[]);

#endif // ZILKER_SYSTEMCOMMANDUTILS_H
