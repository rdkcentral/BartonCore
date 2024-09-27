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

#include "command.h"

typedef struct _Category Category;

/**
 * Create a category of commands.
 *
 * @param name the name of the Category
 * @param description the description of the Category
 * @return the newly created Category
 */
Category *categoryCreate(const gchar *name,
                         const gchar *description);

/**
 * Destroy a Category, releasing any related resources.
 * @param category
 */
void categoryDestroy(Category *category);

/**
 * Mark this Category as advanced (only showing when advanced mode is enabled).
 *
 * @param category
 */
void categorySetAdvanced(Category *category);

/**
 * Add a Command instance to this Category.
 *
 * @param category
 * @param command
 */
void categoryAddCommand(Category *category,
                        Command *command);

/**
 * Retrieve a Command instance from this Category (searching long and short names).
 *
 * @param category
 * @param name
 * @return the matching Command or NULL if not found
 */
Command *categoryGetCommand(const Category *category,
                            const gchar *name);

/**
 * Retrieve a list of possible matches (completions) for the provided partial command.
 *
 * @param buf
 * @return a linked list of completion strings.   Caller destroys.
 */
GList *categoryGetCompletions(const Category *category,
                              const gchar *buf);

/**
 * Print the end-user help for this Category.
 *
 * @param category
 * @param isInteractive true if we are in interactive mode
 * @param showAdvanced true if we are in advanced mode
 */
void categoryPrint(const Category *category,
                   bool isInteractive,
                   bool showAdvanced);
