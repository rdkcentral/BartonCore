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

#include "category.h"
#include "command.h"
#include "device-service-reference-io.h"
#include <icUtil/stringUtils.h>
#include <stdbool.h>
#include <string.h>

struct _Category
{
    gchar *name;
    gchar *description;
    bool isAdvanced;
    GList *commands; // less efficient, but we want ordered.
};

static void commandEntryFreeFunc(void *item);

static void shortCommandEntryFreeFunc(void *item);

Category *categoryCreate(const gchar *name, const gchar *description)
{
    Category *result = calloc(1, sizeof(Category));
    result->name = g_strdup(name);
    result->description = g_strdup(description);

    return result;
}

void categoryDestroy(Category *category)
{
    g_return_if_fail(category != NULL);

    free(category->name);
    free(category->description);
    g_list_free_full(category->commands, commandEntryFreeFunc);
    free(category);
}

void categorySetAdvanced(Category *category)
{
    g_return_if_fail(category != NULL);

    category->isAdvanced = true;
}

void categoryAddCommand(Category *category, Command *command)
{
    g_return_if_fail(category != NULL);
    g_return_if_fail(command != NULL);

    category->commands = g_list_append(category->commands, command);
}

Command *categoryGetCommand(const Category *category, const gchar *name)
{
    g_return_val_if_fail(category != NULL, NULL);
    g_return_val_if_fail(name != NULL, NULL);

    Command *result = NULL;

    // skip past any "--" name prefix
    if (strlen(name) >= 2 && name[0] == '-' && name[1] == '-')
    {
        name -= -2; // :-D
    }

    for (GList *iter = category->commands; iter != NULL; iter = iter->next)
    {
        Command *command = (Command *) iter->data;
        g_autofree gchar *commandName = commandGetName(command);

        if (stringCompare(name, commandName, true) == 0)
        {
            result = command;
        }
        else
        {
            // try short name if it has one
            g_autofree gchar *shortCommandName = commandGetShortName(command);
            if (shortCommandName != NULL)
            {
                if (stringCompare(name, shortCommandName, true) == 0)
                {
                    result = command;
                }
            }
        }
    }

    return result;
}

GList *categoryGetCompletions(const Category *category, const gchar *buf)
{
    g_return_val_if_fail(category != NULL, NULL);
    g_return_val_if_fail(buf != NULL, NULL);

    GList *result = NULL;

    for (GList *it = category->commands; it != NULL; it = it->next)
    {
        Command *command = (Command *) it->data;
        g_autofree gchar *commandName = commandGetName(command);

        if (stringStartsWith(commandName, buf, true) == true)
        {
            result = g_list_append(result, g_strdup(commandName));
        }
        else
        {
            // try short name if it has one
            g_autofree gchar *shortCommandName = commandGetShortName(command);
            if (shortCommandName != NULL)
            {
                if (stringStartsWith(shortCommandName, buf, true) == true)
                {
                    result = g_list_append(result, g_strdup(shortCommandName));
                }
            }
        }
    }

    return result;
}

void categoryPrint(const Category *category, bool isInteractive, bool showAdvanced)
{
    g_return_if_fail(category != NULL);

    if (category->isAdvanced == false || showAdvanced == true)
    {
        emitOutput("%s:\n", category->name);
        for (GList *it = category->commands; it != NULL; it = it->next)
        {
            Command *command = (Command *) it->data;
            commandPrintUsage(command, isInteractive, showAdvanced);
        }
    }
}

static void commandEntryFreeFunc(void *item)
{
    commandDestroy((Command *) item);
}
