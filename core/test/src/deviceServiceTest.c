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

//
// Created by Thomas Lea on 7/22/15.
//
#include <setjmp.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "../../src/deviceDriverManager.h"
#include "deviceServicePrivate.h"
#include <cmocka.h>
#include <device/icDevice.h>
#include <device/icDeviceEndpoint.h>
#include <device/icDeviceResource.h>
#include <deviceCommunicationWatchdog.h>
#include <deviceDescriptors.h>
#include <deviceService.h>
#include <icLog/logging.h>
#include <unistd.h>

#define LOG_CAT        "deviceServiceTest"

#define DB_SCRIPT_PATH "../../../../../filesystem/etc"

extern char *deviceServiceConfigDir;
extern char *deviceServiceDatabasePath;
extern char *deviceServiceDatabaseInitScriptPath;

#if 0

static void basic_startup()
{
    //override the database file to use an in-memory one for testing
    deviceServiceDatabasePath = strdup(":memory:");

    //override device service's config dir so it wont get it from 'getDynamicConfigPath'
    deviceServiceConfigDir = strdup("/tmp");

    deviceServiceDatabaseInitScriptPath = strdup(DB_SCRIPT_PATH);

    //start up but dont block
    assert_int_equal(true, deviceServiceInitialize(false));

    deviceDescriptorsInit("test/AllowList.xml", NULL);

    icLinkedList* deviceClassList = linkedListCreate();
    linkedListAppend(deviceClassList, strdup("testDeviceClass"));

    deviceServiceDiscoverStart(deviceClassList, 1);

    sleep(1);

    deviceServiceDiscoverStop(NULL);

    linkedListDestroy(deviceClassList, NULL);
}

static void basic_cleanup()
{
    deviceServiceShutdown();
    deviceDescriptorsCleanup();
}

static void test_device_driver_manager(void **state)
{
    deviceDriverManagerInitialize();

    assert_non_null(deviceDriverManagerGetDeviceDriver("testDeviceDriver"));

    icLinkedList *drivers = deviceDriverManagerGetDeviceDriversByDeviceClass("testDeviceClass");
    assert_non_null(drivers);
    assert_int_equal(1, linkedListCount(drivers));
    linkedListDestroy(drivers, standardDoNotFreeFunc);

    deviceDriverManagerShutdown();

    (void) state;
}

static void test_device_lookup_by_profile(void **state)
{
    basic_startup();

    //find the device by profile
    icLinkedList *devices = deviceServiceGetDevicesByProfile("testProfile");
    assert_non_null(devices);
    assert_int_equal(1, linkedListCount(devices));
    linkedListDestroy(devices, standardDoNotFreeFunc);
    basic_cleanup();

    (void)state;
}

static void test_device_lookup_by_uuid(void **state)
{
    basic_startup();
    icDevice *device = deviceServiceGetDevice("testsomeuuid");
    devicePrint(device, "");
    assert_non_null(device);
    basic_cleanup();

    (void)state;
}

static void test_device_lookup_by_uri(void **state)
{
    basic_startup();
    icDevice *device = deviceServiceGetDeviceByUri("/testsomeuuid");
    assert_non_null(device);

    device = deviceServiceGetDeviceByUri("/testsomeuuid.3/lksdjf");
    assert_non_null(device);
    basic_cleanup();

    (void)state;
}

static void test_device_lookup_by_class(void **state)
{
    basic_startup();

    icLinkedList *devices = deviceServiceGetDevicesByDeviceClass("testDeviceClass");
    assert_non_null(devices);
    assert_int_equal(1, linkedListCount(devices));
    linkedListDestroy(devices, standardDoNotFreeFunc);

    basic_cleanup();

    (void)state;
}

static void test_device_lookup_by_driver(void **state)
{
    basic_startup();

    icLinkedList *devices = deviceServiceGetDevicesByDeviceDriver("testDeviceDriver");
    assert_non_null(devices);
    assert_int_equal(1, linkedListCount(devices));
    linkedListDestroy(devices, standardDoNotFreeFunc);

    basic_cleanup();

    (void)state;
}

static void test_endpoint_lookup_by_profile(void **state)
{
    basic_startup();

    icLinkedList *endpoints = deviceServiceGetEndpointsByProfile("testProfile");
    assert_non_null(endpoints);
    assert_int_equal(1, linkedListCount(endpoints));

    linkedListDestroy(endpoints, standardDoNotFreeFunc);

    (void)state;
}

static void test_get_all_devices(void **state)
{
    basic_startup();

    icLinkedList *devices = deviceServiceGetAllDevices();
    assert_non_null(devices);
    assert_int_equal(1, linkedListCount(devices));
    linkedListDestroy(devices, standardDoNotFreeFunc);

    (void)state;
}


static void test_get_endpoint_by_uri(void **state)
{
    basic_startup();

    char *targetUri = strdup("/testsomeuuid.1");
    icDeviceEndpoint *endpoint = deviceServiceGetEndpointByUri(targetUri);
    assert_non_null(endpoint);
    assert_int_equal(1, endpoint->endpointId);
    free(targetUri);

    targetUri = strdup("/testsomeuuid.1/something/blalba");
    endpoint = deviceServiceGetEndpointByUri(targetUri);
    assert_non_null(endpoint);
    assert_int_equal(1, endpoint->endpointNumber);
    free(targetUri);

    (void)state;
}

static void test_get_endpoint_by_id(void **state)
{
    basic_startup();

    icDeviceEndpoint *endpoint = deviceServiceGetEndpoint("testsomeuuid", 1);
    assert_non_null(endpoint);
    assert_int_equal(1, endpoint->endpointNumber);

    (void)state;
}


char *failedUuid = NULL;
static void commFailCb(const char *uuid)
{
    failedUuid = strdup(uuid);
}

char *restoredUuid = NULL;
static void commRestoreCb(const char *uuid)
{
    restoredUuid = strdup(uuid);
}

static void test_comm_watchdog_init(void **state)
{
    deviceCommunicationWatchdogInit(commFailCb, commRestoreCb);
    deviceCommunicationWatchdogTerm();
    sleep(2); //need to make the Term() synchronous
}

static void test_comm_watchdog_fail(void **state)
{
    free(failedUuid); failedUuid = NULL;
    free(restoredUuid); restoredUuid = NULL;

    deviceCommunicationWatchdogSetMonitorInterval(1);
    deviceCommunicationWatchdogInit(commFailCb, commRestoreCb);
    deviceCommunicationWatchdogMonitorDevice("1234", 1);
    sleep(2);
    assert_non_null(failedUuid);
    assert_string_equal("1234", failedUuid);
    deviceCommunicationWatchdogTerm();
}

static void test_comm_watchdog_restore(void **state)
{
    free(failedUuid); failedUuid = NULL;
    free(restoredUuid); restoredUuid = NULL;

    deviceCommunicationWatchdogSetMonitorInterval(1);
    deviceCommunicationWatchdogInit(commFailCb, commRestoreCb);
    deviceCommunicationWatchdogMonitorDevice("1234", 1);
    sleep(2);
    assert_non_null(failedUuid);
    assert_string_equal("1234", failedUuid);
    sleep(1);
    deviceCommunicationWatchdogPetDevice("1234");
    assert_non_null(restoredUuid);
    assert_string_equal("1234", restoredUuid);
    deviceCommunicationWatchdogTerm();
}

//************* Database testing *******************
static void test_database_init(void **state)
{
    assert_int_equal(databaseInitialize(":memory:", DB_INIT_SCRIPT_PATH), true);

    databaseCleanup();

    (void)state;
}

static void test_database_system_properties(void **state)
{
    assert_int_equal(databaseInitialize(":memory:", DB_INIT_SCRIPT_PATH), true);

    //create a new property
    assert_int_equal(databaseSetSystemProperty("key", "value"), true);

    char *value = NULL;
    assert_int_equal(databaseGetSystemProperty("key", &value), true);
    assert_non_null(value);
    assert_string_equal(value, "value");
    free(value);

    //change an existing property
    value = NULL;
    assert_int_equal(databaseSetSystemProperty("key", "other"), true);
    assert_int_equal(databaseGetSystemProperty("key", &value), true);
    assert_non_null(value);
    assert_string_equal(value, "other");
    free(value);

    //set existing property to null
    value = NULL;
    assert_int_equal(databaseSetSystemProperty("key", NULL), true);
    assert_int_equal(databaseGetSystemProperty("key", &value), true);
    assert_null(value);

    databaseCleanup();

    (void)state;
}
//************* Database testing *******************

#endif

bool __wrap_jsonDatabaseRestore(const char *tempRestoreDir)
{
    return mock_type(bool);
}

bool __wrap_subsystemManagerRestoreConfig(const char *tempRestoreDir, const char *dynamicConfigPath)
{
    return mock_type(bool);
}

static void test_restore(void **state)
{
    /* Drivers don't autoload, but openhomeCameraDeviceDriver must be loaded */
    deviceDriverManagerInitialize();

    will_return(__wrap_jsonDatabaseRestore, false);
    assert_false(deviceServiceRestoreConfig("fake"));

    will_return(__wrap_jsonDatabaseRestore, true);
    will_return(__wrap_subsystemManagerRestoreConfig, false);
    assert_false(deviceServiceRestoreConfig("fake"));

    will_return(__wrap_jsonDatabaseRestore, true);
    will_return(__wrap_subsystemManagerRestoreConfig, false);
    assert_false(deviceServiceRestoreConfig("fake"));
}

int main(int argc, const char **argv)
{


#if 0



    if(argc > 1 && strcmp(argv[1], "noTests") == 0)
    {
        //dont really run the unit tests, instead start up device service in a mode easier to work with in a debugger
        deviceServiceInitialize(false);

        deviceDescriptorsInit("test/AllowList.xml", NULL);
        while(1) sleep(1);
    }
    else
    {

        const struct CMUnitTest tests[] =
            {
                    cmocka_unit_test(test_device_lookup_by_profile),
                    cmocka_unit_test(test_device_lookup_by_uuid),
                    cmocka_unit_test(test_database_init),
                    cmocka_unit_test(test_database_system_properties),
                    cmocka_unit_test(test_device_driver_manager),
                    cmocka_unit_test(test_comm_watchdog_init),
                    cmocka_unit_test(test_comm_watchdog_fail),
                    cmocka_unit_test(test_comm_watchdog_restore),
                    cmocka_unit_test(test_device_lookup_by_uri),
                    cmocka_unit_test(test_device_lookup_by_class),
                    cmocka_unit_test(test_device_lookup_by_driver),
                    cmocka_unit_test(test_get_all_devices),
                    cmocka_unit_test(test_endpoint_lookup_by_profile),
                    cmocka_unit_test(test_get_endpoint_by_uri),
                    cmocka_unit_test(test_get_endpoint_by_id),
            };

        return cmocka_run_group_tests(tests, NULL, NULL);
    }

    return 0;
#endif

    const struct CMUnitTest tests[] = {cmocka_unit_test(test_restore)};

    return cmocka_run_group_tests(tests, NULL, NULL);
}
