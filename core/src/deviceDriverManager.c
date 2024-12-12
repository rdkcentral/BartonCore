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
 * The device driver manager handles the registration and interactions with the
 * various device drivers, each of which is responsible for understanding how to
 * interact with various device classes.
 *
 * Created by Thomas Lea on 7/28/15.
 */

#include "device-driver/device-driver.h"
#include <deviceService.h>
#include <icLog/logging.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define LOG_TAG "deviceDriverManager"

#include <icConcurrent/threadUtils.h>
#include <icUtil/stringUtils.h>

#include "device-driver/device-driver-manager.h"
#include "deviceDriverManager.h"
#include "deviceServicePrivate.h"

#include "subsystemManager.h"

static bool driverSupportsDeviceClass(const DeviceDriver *driver, const char *deviceClass);

static void destroyDeviceDriverMapEntry(void *key, void *value);

static icHashMap *deviceDrivers = NULL;
static icLinkedList *orderedDeviceDrivers = NULL; // an ordered index (by load) onto the drivers. Does not own drivers.
static pthread_mutex_t deviceDriversMtx = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;

static pthread_once_t registerExitOnce = PTHREAD_ONCE_INIT;

/*
 * Load up the device drivers and initialize them.
 */
bool deviceDriverManagerInitialize()
{
    icLogDebug(LOG_TAG, "deviceDriverManagerInitialize");
    return true;
}

/*
 * Tell each device driver that we are up and running and it can start.
 */
bool deviceDriverManagerStartDeviceDrivers()
{
    icLogDebug(LOG_TAG, "deviceDriverManagerStartDeviceDrivers");

    sbIcLinkedListIterator *it = linkedListIteratorCreate(orderedDeviceDrivers);
    while (linkedListIteratorHasNext(it))
    {
        DeviceDriver *driver = linkedListIteratorGetNext(it);
        driver->startup(driver->callbackContext);
    }

    return true;
}

/*
 * Tell each driver to shutdown and release all resources.
 */
bool deviceDriverManagerShutdown()
{
    icLogDebug(LOG_TAG, "deviceDriverManagerShutdown");

    LOCK_SCOPE(deviceDriversMtx);

    if (deviceDrivers != NULL)
    {
        icHashMapIterator *iterator = hashMapIteratorCreate(deviceDrivers);
        while (hashMapIteratorHasNext(iterator))
        {
            uint16_t keyLen;
            void *key;
            void *value;

            hashMapIteratorGetNext(iterator, &key, &keyLen, &value);

            DeviceDriver *driver = (DeviceDriver *) value;

            driver->shutdown(driver->callbackContext);
        }
        hashMapIteratorDestroy(iterator);
    }

    return true;
}

static void deviceDriverManagerExit(void)
{
    LOCK_SCOPE(deviceDriversMtx);

    hashMapDestroy(deviceDrivers, destroyDeviceDriverMapEntry);
    deviceDrivers = NULL;

    linkedListDestroy(orderedDeviceDrivers, standardDoNotFreeFunc);
    orderedDeviceDrivers = NULL;
}

static void registerExit(void)
{
    atexit(deviceDriverManagerExit);
}

// Drivers should be registered once per process lifecycle. De-registration will happen atexit (see:
// deviceDriverManagerExit)
bool deviceDriverManagerRegisterDriver(DeviceDriver *driver)
{
    bool result = false;

    pthread_once(&registerExitOnce, registerExit);

    LOCK_SCOPE(deviceDriversMtx);

    if (deviceDrivers == NULL)
    {
        deviceDrivers = hashMapCreate();
        orderedDeviceDrivers = linkedListCreate();
    }

    if (driver != NULL && driver->driverName != NULL)
    {
        icLogDebug(LOG_TAG, "Loading device driver %s", driver->driverName);
        hashMapPut(deviceDrivers, driver->driverName, (uint16_t) (strlen(driver->driverName) + 1), driver);
        linkedListAppend(orderedDeviceDrivers, driver);
        result = true;
    }

    return result;
}

icLinkedList *deviceDriverManagerGetDeviceDriversByDeviceClass(const char *deviceClass)
{
    icLinkedList *result = NULL;

    icLogDebug(LOG_TAG, "deviceDriverManagerGetDeviceDriversByDeviceClass: deviceClass=%s", deviceClass);

    LOCK_SCOPE(deviceDriversMtx);

    if (deviceDrivers != NULL && deviceClass != NULL)
    {
        result = linkedListCreate();

        icHashMapIterator *iterator = hashMapIteratorCreate(deviceDrivers);
        while (hashMapIteratorHasNext(iterator))
        {
            char *driverName;
            uint16_t driverNameLen; // will be 1 more than strlen
            DeviceDriver *driver;
            hashMapIteratorGetNext(iterator, (void **) &driverName, &driverNameLen, (void **) &driver);
            if (driverSupportsDeviceClass(driver, deviceClass))
            {
                linkedListAppend(result, driver);
            }
        }
        hashMapIteratorDestroy(iterator);
    }

    if (result == NULL)
    {
        icLogWarn(
            LOG_TAG, "deviceDriverManagerGetDeviceDriversByDeviceClass: deviceClass=%s: NO DRIVER FOUND", deviceClass);
    }

    return result;
}

icLinkedList *deviceDriverManagerGetDeviceDriversBySubsystem(const char *subsystem)
{
    icLinkedList *result = NULL;

    icLogDebug(LOG_TAG, "%s: subsystem=%s", __FUNCTION__, subsystem);

    LOCK_SCOPE(deviceDriversMtx);

    if (deviceDrivers != NULL && subsystem != NULL)
    {
        result = linkedListCreate();

        icHashMapIterator *iterator = hashMapIteratorCreate(deviceDrivers);
        while (hashMapIteratorHasNext(iterator))
        {
            char *driverName;
            uint16_t driverNameLen; // will be 1 more than strlen()

            DeviceDriver *driver;
            hashMapIteratorGetNext(iterator, (void **) &driverName, &driverNameLen, (void **) &driver);

            if (stringCompare(driver->subsystemName, subsystem, false) == 0)
            {
                linkedListAppend(result, driver);
            }
        }
        hashMapIteratorDestroy(iterator);
    }

    if (result == NULL)
    {
        icLogWarn(LOG_TAG, "%s: subsystem=%s: NO DRIVER FOUND", __FUNCTION__, subsystem);
    }

    return result;
}

DeviceDriver *deviceDriverManagerGetDeviceDriver(const char *driverName)
{
    DeviceDriver *result = NULL;

    LOCK_SCOPE(deviceDriversMtx);

    if (deviceDrivers != NULL)
    {
        result = (DeviceDriver *) hashMapGet(deviceDrivers, (void *) driverName, (int16_t) (strlen(driverName) + 1));
    }
    else
    {
        icLogWarn(LOG_TAG, "deviceDriverManagerGetDeviceDriver did not find driver for name %s", driverName);
    }

    return result;
}

icLinkedList *deviceDriverManagerGetDeviceDrivers()
{
    // FIXME: refcount!
    return linkedListClone(orderedDeviceDrivers);
}

typedef DeviceDriver *(*driverInitFn)(void);

static void loadDriver(driverInitFn fn)
{
    DeviceDriver *driver = fn();

    deviceDriverManagerRegisterDriver(driver);
}

static bool driverSupportsDeviceClass(const DeviceDriver *driver, const char *deviceClass)
{
    bool result = false;
    if (driver != NULL && driver->supportedDeviceClasses != NULL)
    {
        icLinkedListIterator *iterator = linkedListIteratorCreate(driver->supportedDeviceClasses);
        while (linkedListIteratorHasNext(iterator))
        {
            char *supportedDeviceClass = (char *) linkedListIteratorGetNext(iterator);
            if (strcmp(deviceClass, supportedDeviceClass) == 0)
            {
                result = true;
                break;
            }
        }

        linkedListIteratorDestroy(iterator);
    }

    return result;
}

static void destroyDeviceDriverMapEntry(void *key, void *value)
{
    // Key is the driverName member, ignore it
    (void) key;

    DeviceDriver *driver = (DeviceDriver *) value;

    if (driver)
    {
        if (driver->destroy)
        {
            driver->destroy(driver->callbackContext);
        }
        else
        {
            free(driver->driverName);
            free(driver->subsystemName);
            linkedListDestroy(driver->supportedDeviceClasses, NULL);
            hashMapDestroy(driver->endpointProfileVersions, NULL);
            free(driver);
        }
    }
}
