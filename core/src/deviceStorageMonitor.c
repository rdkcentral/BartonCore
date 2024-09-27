//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2024 Comcast
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
// Comcast retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------

/*
 * Created by Christian Leithner on 8/5/2024.
 */

#include "deviceStorageMonitor.h"
#include "deviceServiceConfiguration.h"
#include "event/deviceEventProducer.h"
#include "events/device-service-storage-changed-event.h"
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