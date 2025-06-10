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
 * Created by Christian Leithner on 9/6/2024.
 */

#include "deviceCommunicationWatchdog.h"
#include "deviceDescriptorHandler.h"
#include "devicePrivateProperties.h"
#include "deviceServiceCommFail.h"
#include "deviceServiceConfiguration.h"
#include "deviceServicePrivate.h"
#include "icLog/logging.h"
#include "icUtil/stringUtils.h"
#include "provider/barton-core-property-provider.h"
#include <glib-object.h>

#ifdef BARTON_CONFIG_ZIGBEE
#include "subsystems/zigbee/zigbeeEventTracker.h"
#include "subsystems/zigbee/zigbeeSubsystem.h"
#endif

#define LOG_TAG     "deviceEventHandler"
#define logFmt(fmt) "%s: " fmt, __func__

static void
propertyChangedHandler(GObject *source, gchar *propertyName, gchar *oldPropertyValue, gchar *newPropertyValue);

void deviceEventHandlerStartup(void)
{
    g_autoptr(BCorePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();

    if (propertyProvider)
    {
        g_signal_connect(propertyProvider,
                         B_CORE_PROPERTY_PROVIDER_SIGNAL_PROPERTY_CHANGED,
                         G_CALLBACK(propertyChangedHandler),
                         NULL);
    }
}

void deviceEventHandlerShutdown(void)
{
    // Nothing to do
}

static void
propertyChangedHandler(GObject *source, gchar *propertyName, gchar *oldPropertyValue, gchar *newPropertyValue)
{
    BCorePropertyProvider *propertyProvider = B_CORE_PROPERTY_PROVIDER(source);

    if (g_strcmp0(propertyName, DEVICE_DESCRIPTOR_LIST) == 0)
    {
        if (!b_core_property_provider_has_property(propertyProvider, DEVICE_DESC_ALLOWLIST_URL_OVERRIDE))
        {
            deviceDescriptorsUpdateAllowlist(newPropertyValue);
        }
        else
        {
            icLogInfo(LOG_TAG, "Ignoring new DDL URL %s as there is an override set", newPropertyValue);
        }
    }
    if (g_strcmp0(propertyName, DEVICE_DESC_ALLOWLIST_URL_OVERRIDE) == 0)
    {
        // Check if the override was deleted
        if (newPropertyValue != NULL)
        {
            deviceDescriptorsUpdateAllowlist(newPropertyValue);
        }
        else
        {
            // Restore the regular allowlist
            char *allowlistUrl = b_core_property_provider_get_property_as_string(
                propertyProvider, DEVICE_DESCRIPTOR_LIST, NULL);
            if (allowlistUrl != NULL)
            {
                deviceDescriptorsUpdateAllowlist(allowlistUrl);
                free(allowlistUrl);
            }
        }
    }
    else if (g_strcmp0(propertyName, DEVICE_DESC_DENYLIST) == 0)
    {
        deviceDescriptorsUpdateDenylist(newPropertyValue);
    }
    else if (g_strcmp0(propertyName, POSIX_TIME_ZONE_PROP) == 0)
    {
        icLogDebug(LOG_TAG, "Got new time zone: %s", newPropertyValue);
        timeZoneChanged(newPropertyValue);
    }
    else if (g_strcmp0(propertyName, DEVICE_FIRMWARE_URL_NODE) == 0)
    {
        icLogDebug(LOG_TAG, "Got new firmware url: %s", newPropertyValue);
        // Got a new URL, its possible this could trigger new downloads that failed before, so
        // process device descriptors and attempt to download any needed firmware
        deviceServiceProcessDeviceDescriptors();
    }
    else if (g_strcmp0(propertyName, CPE_DENYLISTED_DEVICES_PROPERTY_NAME) == 0)
    {
        icLogDebug(LOG_TAG, "Denylisted devices property set to : %s", newPropertyValue);
        processDenylistedDevices(newPropertyValue);
    }
    else if (g_strcmp0(propertyName, TOUCHSCREEN_SENSOR_COMMFAIL_TROUBLE_DELAY) == 0)
    {
        uint32_t commFailTimeoutMins;
        /* Validated by props: disable on null/delete; no other conversion errors can occur */

        if (stringToUint32(newPropertyValue, &commFailTimeoutMins))
        {
            deviceServiceCommFailSetTimeoutSecs(commFailTimeoutMins * 60);
        }
        else
        {
            deviceServiceCommFailSetTimeoutSecs(0);
        }

        icDebug("Comm fail timeout set to %s minutes", newPropertyValue);
    }
#ifdef BARTON_CONFIG_ZIGBEE
    else if (g_str_has_prefix(propertyName, ZIGBEE_PROPS_PREFIX) ||
             g_str_has_prefix(propertyName, TELEMETRY_PROPS_PREFIX))
    {
        zigbeeSubsystemHandlePropertyChange(propertyName, newPropertyValue);
    }
    else if (g_strcmp0(propertyName, CPE_ZIGBEE_REPORT_DEVICE_INFO_ENABLED) == 0 ||
             g_strcmp0(propertyName, CPE_DIAGNOSTIC_ZIGBEEDATA_ENABLED) == 0 ||
             g_strcmp0(propertyName, CPE_DIAGNOSTIC_ZIGBEEDATA_PER_CHANNEL_NUMBER_OF_SCANS) == 0 ||
             g_strcmp0(propertyName, CPE_DIAGNOSTIC_ZIGBEEDATA_CHANNEL_SCAN_DURATION_MS) == 0 ||
             g_strcmp0(propertyName, CPE_DIAGNOSTIC_ZIGBEEDATA_CHANNEL_SCAN_DELAY_MS) == 0 ||
             g_strcmp0(propertyName, CPE_DIAGNOSTIC_ZIGBEEDATA_COLLECTION_DELAY_MIN) == 0)
    {
        zigbeeEventTrackerPropertyCallback(propertyName, newPropertyValue);
    }
#endif
    else if (g_strcmp0(propertyName, FAST_COMM_FAIL_PROP) == 0)
    {
        bool inFastCommfail = stringToBool(newPropertyValue);
#ifdef BARTON_CONFIG_ZIGBEE
        zigbeeSubsystemHandlePropertyChange(propertyName, newPropertyValue);
#endif
        deviceCommunicationWatchdogSetFastCommfail(inFastCommfail);
    }


    // Finally, give device service a chance to handle the event
    deviceServiceNotifyPropertyChange(propertyName, newPropertyValue);
}
