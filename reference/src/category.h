//------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2024 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
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
Category *categoryCreate(const gchar *name, const gchar *description);

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
void categoryAddCommand(Category *category, Command *command);

/**
 * Retrieve a Command instance from this Category (searching long and short names).
 *
 * @param category
 * @param name
 * @return the matching Command or NULL if not found
 */
Command *categoryGetCommand(const Category *category, const gchar *name);

/**
 * Retrieve a list of possible matches (completions) for the provided partial command.
 *
 * @param buf
 * @return a linked list of completion strings.   Caller destroys.
 */
GList *categoryGetCompletions(const Category *category, const gchar *buf);

/**
 * Print the end-user help for this Category.
 *
 * @param category
 * @param isInteractive true if we are in interactive mode
 * @param showAdvanced true if we are in advanced mode
 */
void categoryPrint(const Category *category, bool isInteractive, bool showAdvanced);
