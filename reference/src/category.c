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
