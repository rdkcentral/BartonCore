//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2019 Comcast Corporation
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
 * Created by Thomas Lea on 9/27/19.
 */

#pragma once

#include "device-service-client.h"
#include <stdbool.h>

typedef bool (*commandExecFunc)(BDeviceServiceClient *client, gint argc, gchar *argv[]);

typedef struct _Command Command;

/**
 * Create a command instance.
 *
 * @param name the long name of the command
 * @param shortInteractiveName the optional short name / alias for the command
 * @param argUsage the usage description for any args or NULL if none
 * @param help the usage description for the command
 * @param minArgs the minimum number of arguments for this command
 * @param maxArgs the maximum number of arguments for this command
 * @param func the function to execute when the command is invoked
 *
 * @return the created Command instance
 */
Command *commandCreate(const gchar *name,
                       const gchar *shortInteractiveName,
                       const gchar *argUsage,
                       const gchar *help,
                       int minArgs,
                       int maxArgs,
                       commandExecFunc func);

/**
 * Destroy a Command instance releasing its resources
 *
 * @param command
 */
void commandDestroy(Command *command);

/**
 * Get the long name of the Command.  Caller frees.
 *
 * @param command
 * @return
 */
gchar *commandGetName(const Command *command);

/**
 * Get the optional short name of the Command.  Caller frees.
 *
 * @param command
 * @return
 */
gchar *commandGetShortName(const Command *command);

/**
 * Mark this Command as advanced (it will only appear to the user if advanced mode is enabled)
 *
 * @param command
 */
void commandSetAdvanced(Command *command);

/**
 * Execute a Command.
 *
 * @param client
 * @param command
 * @param argc
 * @param argv
 * @return true if the command succeeded
 */
bool commandExecute(BDeviceServiceClient *client, const Command *command, gint argc, gchar **argv);

/**
 * Add an example usage for this Command.  These help the user understand how to use it.
 * @param command
 * @param example
 */
void commandAddExample(Command *command, const gchar *example);

/**
 * Print the usage of the Command for the user.
 *
 * @param command
 * @param isInteractive true if we are in interactive mode
 * @param showAdvanced true if we are in advanced mode
 */
void commandPrintUsage(const Command *command, bool isInteractive, bool showAdvanced);
