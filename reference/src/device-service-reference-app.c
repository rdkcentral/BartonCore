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
 * Created by Micah Koch on 09/17/24.
 */

#include "coreCategory.h"
#include "device-service-client.h"
#include "device-service-initialize-params-container.h"
#include "device-service-reference-io.h"
#include "eventHandler.h"
#include "matterCategory.h"
#include "threadCategory.h"
#include "provider/device-service-property-provider.h"
#include "reference-network-credentials-provider.h"
#include <icLog/logging.h>
#include <icUtil/stringUtils.h>
#include <linenoise.h>
#include <pthread.h>
#include <pwd.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define HISTORY_FILE          ".barton-ds-reference-history"
#define HISTORY_MAX           100
#define DEFAULT_CONF_DIR_MODE (0755)

static gboolean NO_VT100_MODE = FALSE;
static gboolean NO_ZIGBEE = FALSE;
static gboolean NO_THREAD = FALSE;
static gboolean NO_MATTER = FALSE;

static bool showAdvanced = false;

static GList *categories = NULL;

static GOptionEntry entries[] = {
    { "novt100", '1', 0, G_OPTION_ARG_NONE, &NO_VT100_MODE, "", NULL},
    {"noZigbee", 'z', 0, G_OPTION_ARG_NONE,     &NO_ZIGBEE, "", NULL},
    {"noThread", 't', 0, G_OPTION_ARG_NONE,     &NO_THREAD, "", NULL},
    {"noMatter", 'm', 0, G_OPTION_ARG_NONE,     &NO_MATTER, "", NULL},
    G_OPTION_ENTRY_NULL
};

static void buildCategories()
{
    categories = g_list_append(categories, buildCoreCategory());
    categories = g_list_append(categories, buildMatterCategory());
    categories = g_list_append(categories, buildThreadCategory());
}

static void destroyCategories()
{
    g_list_free_full(categories, (GDestroyNotify) categoryDestroy);
}

static void showInteractiveHelp(bool isInteractive)
{
    for (GList *it = categories; it != NULL; it = it->next)
    {
        Category *category = (Category *) it->data;
        categoryPrint(category, isInteractive, showAdvanced);
    }
}

static Command *findCommand(char *name)
{
    Command *result = NULL;

    for (GList *it = categories; it != NULL && result == NULL; it = it->next)
    {
        Category *category = (Category *) it->data;
        stringToLowerCase(name);
        result = categoryGetCommand(category, name);
    }

    return result;
}

static bool handleInteractiveCommand(BDeviceServiceClient *client, char **args, int numArgs)
{
    bool keepGoing = true;

    if (args != NULL && numArgs > 0)
    {
        // check for a couple of special commands first
        if (stringCompare(args[0], "quit", true) == 0 || stringCompare(args[0], "exit", true) == 0)
        {
            keepGoing = false;
        }
        else if (stringCompare(args[0], "help", true) == 0)
        {
            if (numArgs > 1)
            {
                // show help for a command
                Command *command = findCommand(args[1]);
                if (command != NULL)
                {
                    commandPrintUsage(command, true, showAdvanced);
                }
                else
                {
                    emitError("Invalid command\n");
                }
            }
            else
            {
                // show full help
                showInteractiveHelp(true);
            }
        }
        else
        {
            Command *command = findCommand(args[0]);
            if (command != NULL)
            {
                char **argv = (numArgs == 1) ? NULL : &(args[1]);
                commandExecute(client, command, numArgs - 1, argv);
            }
            else
            {
                emitError("Invalid command\n");
            }
        }
    }

    return keepGoing;
}

static void completionCallback(const char *buf, linenoiseCompletions *lc)
{
    g_autofree gchar *myBuf = g_strdup(buf);
    for (GList *it = categories; it != NULL; it = it->next)
    {
        Category *category = (Category *) it->data;
        stringToLowerCase(myBuf);

        g_autoptr(GList) completions = categoryGetCompletions(category, myBuf);
        for (GList *completionsIt = completions; completionsIt != NULL; completionsIt = completionsIt->next)
        {
            char *aCompletion = (char *) completionsIt->data;
            linenoiseAddCompletion(lc, aCompletion);
        }
    }
}

static gchar *getDefaultConfigDirectory(void)
{
    struct passwd *pw = getpwuid(getuid());
    gchar *confDir = stringBuilder("%s/.brtn-ds", pw->pw_dir);
    g_mkdir_with_parents(confDir, DEFAULT_CONF_DIR_MODE);
    return confDir;
}

static void configureSubsystems(BDeviceServiceInitializeParamsContainer *params)
{
    BDeviceServicePropertyProvider *propProvider =
        b_device_service_initialize_params_container_get_property_provider(params);
    if (propProvider != NULL)
    {
        g_autofree gchar *disableSubsystems = NULL;
        if (NO_ZIGBEE)
        {
            disableSubsystems = g_strdup("zigbee");
        }
        if (NO_THREAD)
        {
            if (disableSubsystems != NULL)
            {
                g_autofree gchar *old = disableSubsystems;
                disableSubsystems = g_strdup_printf("%s,thread", old);
            }
            else
            {
                disableSubsystems = g_strdup("thread");
            }
        }
        if (NO_MATTER)
        {
            if (disableSubsystems != NULL)
            {
                g_autofree gchar *old = disableSubsystems;
                disableSubsystems = g_strdup_printf("%s,matter", old);
            }
            else
            {
                disableSubsystems = g_strdup("matter");
            }
        }

        if (disableSubsystems != NULL)
        {
            b_device_service_property_provider_set_property_string(
                propProvider, "device.subsystem.disable", disableSubsystems);
        }
    }
}

static BDeviceServiceClient *initializeClient(gchar *confDir)
{
    g_autoptr(BDeviceServiceInitializeParamsContainer) params = b_device_service_initialize_params_container_new();
    b_device_service_initialize_params_container_set_storage_dir(params, confDir);

    g_autofree gchar *matterConfDir = stringBuilder("%s/matter", confDir);
    g_mkdir_with_parents(matterConfDir, DEFAULT_CONF_DIR_MODE);
    b_device_service_initialize_params_container_set_matter_storage_dir(params, matterConfDir);
    b_device_service_initialize_params_container_set_matter_attestation_trust_store_dir(params, matterConfDir);

    g_autoptr(BReferenceNetworkCredentialsProvider) networkCredentialsProvider =
        b_reference_network_credentials_provider_new();
    b_device_service_initialize_params_container_set_network_credentials_provider(
        params, B_DEVICE_SERVICE_NETWORK_CREDENTIALS_PROVIDER(networkCredentialsProvider));

    BDeviceServiceClient *client = b_device_service_client_new(params);
    configureSubsystems(params);
    return client;
}

static bool handleLine(const char *line, BDeviceServiceClient *client)
{
    bool retVal = true;

    gint numArgs = 0;

    if (!NO_VT100_MODE && line != NULL)
    {
        linenoiseHistoryAdd(line);
    }

    if (line == NULL)
    {
        line = "exit";
    }

    gchar **args = NULL;
    g_autoptr(GError) parseErr = NULL;
    if (g_shell_parse_argv(line, &numArgs, &args, &parseErr))
    {
        if (numArgs > 0)
        {
            retVal = handleInteractiveCommand(client, args, numArgs);
        }
    }
    else
    {
        if (parseErr != NULL && parseErr->code != G_SHELL_ERROR_EMPTY_STRING)
        {
            emitError("Error parsing command: %s\n", parseErr->message);
        }
    }

    g_strfreev(args);

    return retVal;
}

int main(int argc, char **argv)
{
    bool rc = false;

    setIcLogPriorityFilter(IC_LOG_DEBUG);

    g_autoptr(GError) cmdLineError = NULL;
    g_autoptr(GOptionContext) context = NULL;
    context = g_option_context_new("- reference app for running barton device service");
    g_option_context_add_main_entries(context, entries, NULL);
    if (!g_option_context_parse(context, &argc, &argv, &cmdLineError))
    {
        fprintf(stderr, "Error parsing command line: %s\n", cmdLineError->message);
        return EXIT_FAILURE;
    }

    g_autofree gchar *confDir = getDefaultConfigDirectory();
    g_autofree gchar *histFile = stringBuilder("%s/%s", confDir, HISTORY_FILE);
    buildCategories();

    if (!NO_VT100_MODE)
    {
        linenoiseSetCompletionCallback(completionCallback);
        linenoiseHistorySetMaxLen(HISTORY_MAX);

        if (confDir != NULL)
        {
            linenoiseHistoryLoad(histFile);
        }
    }

    if (argc > 1)
    {
        // Some excess unknown arguments
        fprintf(stderr, "Ignoring unknown arguments:");
        for (int i = 1; i < argc; ++i)
        {
            fprintf(stderr, " %s", argv[i]);
        }
        fprintf(stderr, "\n");
    }

    g_autoptr(BDeviceServiceClient) client = initializeClient(confDir);

    b_device_service_client_start(client);
    // TODO remove once ddl command available
    b_device_service_client_set_system_property(client, "deviceDescriptorBypass", "true");

    registerEventHandlers(client);

    /*
        TODO: There are three issues with logging at this time:
            1. Logging prior to this call to device_service_reference_io_process is not shown if anything prior blocks
       (ex: b_device_service_client_start blocks on subsystem ready wait)
            2. Logging after this call to device_service_reference_io_process (in main()) is not shown
            3. Logging after main is shown but is broken if VT100 mode is enabled.
                - Additionally, if the reference app is run in a terminal the terminal will be left in a broken state.

        For now, to see output like ASAN leaks sensibly, use the -1 argument to turn off VT100 mode.
     */
    device_service_reference_io_process(!NO_VT100_MODE, (processLineCallback) handleLine, client);

    unregisterEventHandlers();
    rc = true;

    if (confDir != NULL)
    {
        linenoiseHistorySave(histFile);
    }

    b_device_service_client_stop(client);

    destroyCategories();

    if (rc != true)
    {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
