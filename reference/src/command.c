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

#include "command.h"
#include "device-service-client.h"
#include <stdio.h>
#include <string.h>

struct _Command
{
    gchar *name;
    gchar *shortInteractiveName;
    gchar *argUsage;
    gchar *help;
    bool isAdvanced;
    gint minArgs;
    gint maxArgs; // -1 for unlimited
    commandExecFunc func;
    GList *examples;
};


Command *commandCreate(const gchar *name,
                       const gchar *shortInteractiveName,
                       const gchar *argUsage,
                       const gchar *help,
                       gint minArgs,
                       gint maxArgs,
                       commandExecFunc func)
{
    Command *result = calloc(1, sizeof(Command));
    result->name = g_strdup(name);
    if (shortInteractiveName != NULL)
    {
        result->shortInteractiveName = g_strdup(shortInteractiveName);
    }
    if (argUsage != NULL)
    {
        result->argUsage = g_strdup(argUsage);
    }
    result->help = g_strdup(help);
    result->minArgs = minArgs;
    result->maxArgs = maxArgs;
    result->func = func;

    return result;
}

void commandDestroy(Command *command)
{
    if (command != NULL)
    {
        free(command->name);
        free(command->shortInteractiveName);
        free(command->argUsage);
        free(command->help);
        g_list_free_full(command->examples, g_free);
        free(command);
    }
}

gchar *commandGetName(const Command *command)
{
    if (command != NULL)
    {
        return g_strdup(command->name);
    }
    return NULL;
}

gchar *commandGetShortName(const Command *command)
{
    if (command != NULL && command->shortInteractiveName != NULL)
    {
        return g_strdup(command->shortInteractiveName);
    }
    return NULL;
}

void commandSetAdvanced(Command *command)
{
    if (command != NULL)
    {
        command->isAdvanced = true;
    }
}

bool commandExecute(BDeviceServiceClient *client, const Command *command, gint argc, gchar **argv)
{
    bool result = false;

    if (command != NULL)
    {
        if (argc >= command->minArgs && (command->maxArgs == -1 || argc <= command->maxArgs))
        {
            result = command->func(client, argc, argv);
        }
        else
        {
            printf("Invalid args\n");
        }
    }

    return result;
}

void commandAddExample(Command *command, const gchar *example)
{
    if (command != NULL && example != NULL)
    {
        command->examples = g_list_append(command->examples, (void *) g_strdup(example));
    }
}

void commandPrintUsage(const Command *command, bool isInteractive, bool showAdvanced)
{
    if (command != NULL && (command->isAdvanced == false || showAdvanced == true))
    {
        if (isInteractive)
        {
            if (command->shortInteractiveName != NULL)
            {
                printf("\t%s|%s %s : %s\n",
                       command->name,
                       command->shortInteractiveName,
                       command->argUsage == NULL ? "" : command->argUsage,
                       command->help);
            }
            else
            {
                printf(
                    "\t%s %s : %s\n", command->name, command->argUsage == NULL ? "" : command->argUsage, command->help);
            }
        }
        else
        {
            printf(
                "\t--%s %s : %s\n", command->name, command->argUsage == NULL ? "" : command->argUsage, command->help);
        }

        if (command->examples != NULL)
        {
            printf("\tExamples:\n");
            for (GList *it = command->examples; it != NULL; it = it->next)
            {
                if (isInteractive == true)
                {
                    printf("\t\t%s\n", (gchar *) it->data);
                }
                else
                {
                    printf("\t\t--%s\n", (gchar *) it->data);
                }
            }
            printf("\n");
        }
    }
}