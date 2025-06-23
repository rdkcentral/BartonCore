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
 * Created by Christian Leithner on 8/5/2024.
 */

#include "deviceStorageMonitor.h"
#include "deviceServiceConfiguration.h"
#include "event/deviceEventProducer.h"
#include "events/barton-core-storage-changed-event.h"
#include "icUtil/stringUtils.h"

static GFileMonitor *monitor = NULL;

static void
configurationChanged(GFileMonitor *monitor, GFile *file, GFile *otherFile, GFileMonitorEvent event, gpointer userData);

void deviceStorageMonitorStart(void)
{
    g_return_if_fail(monitor == NULL);

    // TODO: this should be retrieved from the storage directory provided to us in initial params
    g_autofree char *configPath = deviceServiceConfigurationGetStorageDir();
    g_autofree char *storageDir = stringBuilder("%s/%s/%s", configPath, "storage", "devicedb");

    g_autoptr(GFile) storageFile = g_file_new_for_path(storageDir);
    g_autoptr(GError) err = NULL;

    monitor = g_file_monitor_directory(
        /* file */ storageFile,
        /* flags */ G_FILE_MONITOR_WATCH_MOVES,
        /* cancellable */ NULL,
        /* error */ &err);

    if (err)
    {
        g_critical("Failed to monitor '%s': %s", storageDir, err->message);
        g_object_unref(g_steal_pointer(&storageFile));
        return;
    }

    g_signal_connect(monitor, "changed", G_CALLBACK(configurationChanged), NULL);
}

void deviceStorageMonitorStop(void)
{
    if (monitor != NULL)
    {
        g_object_unref(monitor);
        monitor = NULL;
    }
}

static void
configurationChanged(GFileMonitor *monitor, GFile *file, GFile *otherFile, GFileMonitorEvent event, gpointer userData)
{
    bool notify = false;

    switch (event)
    {
        case G_FILE_MONITOR_EVENT_MOVED_IN:
        case G_FILE_MONITOR_EVENT_MOVED_OUT:
        case G_FILE_MONITOR_EVENT_CHANGED:
        case G_FILE_MONITOR_EVENT_DELETED:
        case G_FILE_MONITOR_EVENT_CREATED:
            g_debug("File '%s' changed", g_file_peek_path(file));
            notify = true;
            break;

        case G_FILE_MONITOR_EVENT_RENAMED:
            g_debug("File '%s' renamed from '%s'", g_file_peek_path(otherFile), g_file_peek_path(file));
            notify = true;
            break;

        default:
            // Not interested
            break;
    }

    if (notify)
    {
        sendStorageChangedEvent(event);
    }
}
