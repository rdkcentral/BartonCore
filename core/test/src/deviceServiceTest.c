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

// ========== Discovery Start Tests ==========

// Mock driver callback tracking
static int discoverDevicesCalled = 0;
static int recoverDevicesCalled = 0;
static int stopDiscoveringDevicesCalled = 0;
static bool mockDriverShouldFailDiscovery = false;
static bool mockDriverShouldFailRecovery = false;

static bool mockDiscoverDevicesCallback(void *ctx, const char *deviceClass)
{
    (void) ctx;
    (void) deviceClass;
    check_expected(deviceClass);
    discoverDevicesCalled++;
    return !mockDriverShouldFailDiscovery;
}

// Separate callback that always fails for testing partial failures
static bool mockDiscoverDevicesCallbackAlwaysFails(void *ctx, const char *deviceClass)
{
    (void) ctx;
    (void) deviceClass;
    check_expected(deviceClass);
    discoverDevicesCalled++;
    return false;
}

static bool mockRecoverDevicesCallback(void *ctx, const char *deviceClass)
{
    (void) ctx;
    (void) deviceClass;
    check_expected(deviceClass);
    recoverDevicesCalled++;
    return !mockDriverShouldFailRecovery;
}

static void mockStopDiscoveringDevicesCallback(void *ctx, const char *deviceClass)
{
    (void) ctx;
    (void) deviceClass;
    check_expected(deviceClass);
    stopDiscoveringDevicesCalled++;
}

// Mock event tracking
static int discoveryStartedEventCalled = 0;
static int discoveryStoppedEventCalled = 0;
static int recoveryStoppedEventCalled = 0;
static icLinkedList *discoveryStoppedEventClasses = NULL;
static icLinkedList *recoveryStoppedEventClasses = NULL;

void __wrap_sendDiscoveryStartedEvent(const icLinkedList *deviceClasses,
                                      uint16_t timeoutSeconds,
                                      bool findOrphanedDevices)
{
    (void) deviceClasses;
    check_expected(timeoutSeconds);
    check_expected(findOrphanedDevices);
    discoveryStartedEventCalled++;
}

void __wrap_sendDiscoveryStoppedEvent(const char *deviceClass)
{
    // Track in list to allow any order for thread-safety
    if (discoveryStoppedEventClasses != NULL)
    {
        linkedListAppend(discoveryStoppedEventClasses, (void *) deviceClass);
    }
    else
    {
        // Fallback to ordered check if list not initialized
        check_expected(deviceClass);
    }
    discoveryStoppedEventCalled++;
}

void __wrap_sendRecoveryStoppedEvent(const char *deviceClass)
{
    // Track in list to allow any order for thread-safety
    if (recoveryStoppedEventClasses != NULL)
    {
        linkedListAppend(recoveryStoppedEventClasses, (void *) deviceClass);
    }
    else
    {
        // Fallback to ordered check if list not initialized
        check_expected(deviceClass);
    }
    recoveryStoppedEventCalled++;
}

// Mock driver manager
icLinkedList *__wrap_deviceDriverManagerGetDeviceDriversByDeviceClass(const char *deviceClass)
{
    // Note: We don't check deviceClass parameter to allow multiple calls
    (void) deviceClass;
    return mock_ptr_type(icLinkedList *);
}

// Track whether device descriptors should report ready
static bool mockDeviceDescriptorsReady = true;

// Mock for device descriptors
bool __wrap_deviceDescriptorsListIsReady(void)
{
    return mockDeviceDescriptorsReady;
}

// Helper to check if a device class is in the stopped events list
static bool hasStoppedEventForClass(const char *deviceClass)
{
    if (discoveryStoppedEventClasses == NULL)
    {
        return false;
    }

    icLinkedListIterator *iter = linkedListIteratorCreate(discoveryStoppedEventClasses);
    while (linkedListIteratorHasNext(iter))
    {
        const char *cls = (const char *) linkedListIteratorGetNext(iter);
        if (strcmp(cls, deviceClass) == 0)
        {
            linkedListIteratorDestroy(iter);
            return true;
        }
    }
    linkedListIteratorDestroy(iter);
    return false;
}

// Helper to check if a device class is in the recovery stopped events list
static bool hasRecoveryStoppedEventForClass(const char *deviceClass)
{
    if (recoveryStoppedEventClasses == NULL)
    {
        return false;
    }

    icLinkedListIterator *iter = linkedListIteratorCreate(recoveryStoppedEventClasses);
    while (linkedListIteratorHasNext(iter))
    {
        const char *cls = (const char *) linkedListIteratorGetNext(iter);
        if (strcmp(cls, deviceClass) == 0)
        {
            linkedListIteratorDestroy(iter);
            return true;
        }
    }
    linkedListIteratorDestroy(iter);
    return false;
}

// Helper to reset mock tracking
static void resetMockTracking(void)
{
    discoverDevicesCalled = 0;
    recoverDevicesCalled = 0;
    stopDiscoveringDevicesCalled = 0;
    discoveryStartedEventCalled = 0;
    discoveryStoppedEventCalled = 0;
    recoveryStoppedEventCalled = 0;
    mockDriverShouldFailDiscovery = false;
    mockDriverShouldFailRecovery = false;
    mockDeviceDescriptorsReady = true;

    // Clean up old tracking lists
    if (discoveryStoppedEventClasses != NULL)
    {
        linkedListDestroy(discoveryStoppedEventClasses, standardDoNotFreeFunc);
    }
    if (recoveryStoppedEventClasses != NULL)
    {
        linkedListDestroy(recoveryStoppedEventClasses, standardDoNotFreeFunc);
    }

    // Initialize new tracking lists
    discoveryStoppedEventClasses = linkedListCreate();
    recoveryStoppedEventClasses = linkedListCreate();
}

// Helper to create a mock driver
static DeviceDriver *createMockDriver(const char *name, bool supportsDiscovery, bool supportsRecovery, bool neverReject)
{
    DeviceDriver *driver = calloc(1, sizeof(DeviceDriver));
    driver->driverName = strdup(name);
    driver->neverReject = neverReject;

    if (supportsDiscovery)
    {
        driver->discoverDevices = mockDiscoverDevicesCallback;
    }

    if (supportsRecovery)
    {
        driver->recoverDevices = mockRecoverDevicesCallback;
    }

    driver->stopDiscoveringDevices = mockStopDiscoveringDevicesCallback;

    return driver;
}

// Helper to destroy mock driver
static void destroyMockDriver(DeviceDriver *driver)
{
    if (driver)
    {
        free(driver->driverName);
        free(driver);
    }
}

static void test_discovery_start_success_single_driver(void **state)
{
    (void) state;
    resetMockTracking();

    // Create mock driver - need two lists since deviceDriverManagerGetDeviceDriversByDeviceClass is called twice
    DeviceDriver *driver = createMockDriver("testDriver", true, false, true);
    icLinkedList *drivers1 = linkedListCreate();
    linkedListAppend(drivers1, driver);
    icLinkedList *drivers2 = linkedListCreate();
    linkedListAppend(drivers2, driver);

    // Setup expectations:
    // 1. getDriversForDiscovery calls deviceDriverManagerGetDeviceDriversByDeviceClass
    // 2. deviceDescriptorsListIsReady is called once (line 496)
    // 3. deviceDriverManagerGetDeviceDriversByDeviceClass is called again (line 575)
    // 4. Driver's discoverDevices callback is called
    will_return(__wrap_deviceDriverManagerGetDeviceDriversByDeviceClass, drivers1);
    will_return(__wrap_deviceDriverManagerGetDeviceDriversByDeviceClass, drivers2);

    expect_value(__wrap_sendDiscoveryStartedEvent, timeoutSeconds, 30);
    expect_value(__wrap_sendDiscoveryStartedEvent, findOrphanedDevices, false);
    expect_string(mockDiscoverDevicesCallback, deviceClass, "testClass");

    // Create device class list
    icLinkedList *deviceClasses = linkedListCreate();
    linkedListAppend(deviceClasses, "testClass");

    // Call function under test
    bool result = deviceServiceDiscoverStart(deviceClasses, NULL, 30, false);

    // Verify
    assert_true(result);
    assert_int_equal(1, discoverDevicesCalled);
    assert_int_equal(0, stopDiscoveringDevicesCalled);
    assert_int_equal(1, discoveryStartedEventCalled);
    assert_int_equal(0, discoveryStoppedEventCalled);

    // Cleanup
    linkedListDestroy(deviceClasses, standardDoNotFreeFunc);
    // drivers1 and drivers2 are destroyed by deviceServiceDiscoverStart
    destroyMockDriver(driver);
}

static void test_discovery_start_failure_single_driver(void **state)
{
    (void) state;
    resetMockTracking();
    mockDriverShouldFailDiscovery = true; // Make driver fail

    // Create mock driver - need two lists since deviceDriverManagerGetDeviceDriversByDeviceClass is called twice
    DeviceDriver *driver = createMockDriver("testDriver", true, false, true);
    icLinkedList *drivers1 = linkedListCreate();
    linkedListAppend(drivers1, driver);
    icLinkedList *drivers2 = linkedListCreate();
    linkedListAppend(drivers2, driver);

    // Setup expectations
    will_return(__wrap_deviceDriverManagerGetDeviceDriversByDeviceClass, drivers1);
    will_return(__wrap_deviceDriverManagerGetDeviceDriversByDeviceClass, drivers2);

    expect_value(__wrap_sendDiscoveryStartedEvent, timeoutSeconds, 30);
    expect_value(__wrap_sendDiscoveryStartedEvent, findOrphanedDevices, false);
    expect_string(mockDiscoverDevicesCallback, deviceClass, "testClass");
    // Note: stopped event tracked in list, verified after call

    // Create device class list
    icLinkedList *deviceClasses = linkedListCreate();
    linkedListAppend(deviceClasses, "testClass");

    // Call function under test
    bool result = deviceServiceDiscoverStart(deviceClasses, NULL, 30, false);

    // Verify - should return false on failure
    assert_false(result);
    assert_int_equal(1, discoverDevicesCalled);
    assert_int_equal(0, stopDiscoveringDevicesCalled); // No drivers to stop since none started
    assert_int_equal(1, discoveryStartedEventCalled);  // Event sent optimistically
    assert_int_equal(1, discoveryStoppedEventCalled);  // Stop event sent due to failure
    assert_true(hasStoppedEventForClass("testClass"));

    // Cleanup
    linkedListDestroy(deviceClasses, standardDoNotFreeFunc);
    // drivers1 and drivers2 are destroyed by deviceServiceDiscoverStart
    destroyMockDriver(driver);
}

static void test_discovery_start_partial_failure_multiple_drivers(void **state)
{
    (void) state;
    resetMockTracking();

    // Create two mock drivers - we'll make one succeed and one fail
    DeviceDriver *driver1 = createMockDriver("testDriver1", true, false, true);
    DeviceDriver *driver2 = createMockDriver("testDriver2", true, false, true);

    // Make driver2 use the always-fail callback
    driver2->discoverDevices = mockDiscoverDevicesCallbackAlwaysFails;

    icLinkedList *drivers1 = linkedListCreate();
    linkedListAppend(drivers1, driver1);
    linkedListAppend(drivers1, driver2);
    icLinkedList *drivers2 = linkedListCreate();
    linkedListAppend(drivers2, driver1);
    linkedListAppend(drivers2, driver2);

    // Setup expectations
    will_return(__wrap_deviceDriverManagerGetDeviceDriversByDeviceClass, drivers1);
    will_return(__wrap_deviceDriverManagerGetDeviceDriversByDeviceClass, drivers2);

    expect_value(__wrap_sendDiscoveryStartedEvent, timeoutSeconds, 30);
    expect_value(__wrap_sendDiscoveryStartedEvent, findOrphanedDevices, false);
    expect_string(mockDiscoverDevicesCallback, deviceClass, "testClass");            // driver1 succeeds
    expect_string(mockDiscoverDevicesCallbackAlwaysFails, deviceClass, "testClass"); // driver2 fails
    expect_string(mockStopDiscoveringDevicesCallback, deviceClass, "testClass");
    // Note: stopped event tracked in list, verified after call

    // Create device class list
    icLinkedList *deviceClasses = linkedListCreate();
    linkedListAppend(deviceClasses, "testClass");

    // Call function under test
    bool result = deviceServiceDiscoverStart(deviceClasses, NULL, 30, false);

    // Verify - should return false since one driver failed
    assert_false(result);
    assert_int_equal(2, discoverDevicesCalled);        // Both drivers attempted
    assert_int_equal(1, stopDiscoveringDevicesCalled); // Only the successful driver gets stopped
    assert_int_equal(1, discoveryStartedEventCalled);
    assert_int_equal(1, discoveryStoppedEventCalled);
    assert_true(hasStoppedEventForClass("testClass"));

    // Cleanup
    linkedListDestroy(deviceClasses, standardDoNotFreeFunc);
    // drivers1 and drivers2 are destroyed by deviceServiceDiscoverStart
    destroyMockDriver(driver1);
    destroyMockDriver(driver2);
}

static void test_recovery_start_success(void **state)
{
    (void) state;
    resetMockTracking();

    // Create mock driver with recovery support - need two lists
    DeviceDriver *driver = createMockDriver("testDriver", false, true, true);
    icLinkedList *drivers1 = linkedListCreate();
    linkedListAppend(drivers1, driver);
    icLinkedList *drivers2 = linkedListCreate();
    linkedListAppend(drivers2, driver);

    // Setup expectations
    will_return(__wrap_deviceDriverManagerGetDeviceDriversByDeviceClass, drivers1);
    will_return(__wrap_deviceDriverManagerGetDeviceDriversByDeviceClass, drivers2);

    expect_value(__wrap_sendDiscoveryStartedEvent, timeoutSeconds, 30);
    expect_value(__wrap_sendDiscoveryStartedEvent, findOrphanedDevices, true);
    expect_string(mockRecoverDevicesCallback, deviceClass, "testClass");

    // Create device class list
    icLinkedList *deviceClasses = linkedListCreate();
    linkedListAppend(deviceClasses, "testClass");

    // Call function under test with recovery mode
    bool result = deviceServiceDiscoverStart(deviceClasses, NULL, 30, true);

    // Verify
    assert_true(result);
    assert_int_equal(1, recoverDevicesCalled);
    assert_int_equal(0, discoverDevicesCalled);
    assert_int_equal(0, stopDiscoveringDevicesCalled);
    assert_int_equal(1, discoveryStartedEventCalled);
    assert_int_equal(0, recoveryStoppedEventCalled);

    // Cleanup
    linkedListDestroy(deviceClasses, standardDoNotFreeFunc);
    // drivers1 and drivers2 are destroyed by deviceServiceDiscoverStart
    destroyMockDriver(driver);
}

static void test_recovery_start_failure(void **state)
{
    (void) state;
    resetMockTracking();
    mockDriverShouldFailRecovery = true;

    // Create mock driver with recovery support - need two lists
    DeviceDriver *driver = createMockDriver("testDriver", false, true, true);
    icLinkedList *drivers1 = linkedListCreate();
    linkedListAppend(drivers1, driver);
    icLinkedList *drivers2 = linkedListCreate();
    linkedListAppend(drivers2, driver);

    // Setup expectations
    will_return(__wrap_deviceDriverManagerGetDeviceDriversByDeviceClass, drivers1);
    will_return(__wrap_deviceDriverManagerGetDeviceDriversByDeviceClass, drivers2);

    expect_value(__wrap_sendDiscoveryStartedEvent, timeoutSeconds, 30);
    expect_value(__wrap_sendDiscoveryStartedEvent, findOrphanedDevices, true);
    expect_string(mockRecoverDevicesCallback, deviceClass, "testClass");
    // Note: recovery stopped event tracked in list, verified after call

    // Create device class list
    icLinkedList *deviceClasses = linkedListCreate();
    linkedListAppend(deviceClasses, "testClass");

    // Call function under test with recovery mode
    bool result = deviceServiceDiscoverStart(deviceClasses, NULL, 30, true);

    // Verify
    assert_false(result);
    assert_int_equal(1, recoverDevicesCalled);
    assert_int_equal(0, stopDiscoveringDevicesCalled);
    assert_int_equal(1, discoveryStartedEventCalled);
    assert_int_equal(1, recoveryStoppedEventCalled); // Recovery stopped event sent
    assert_true(hasRecoveryStoppedEventForClass("testClass"));

    // Cleanup
    linkedListDestroy(deviceClasses, standardDoNotFreeFunc);
    // drivers1 and drivers2 are destroyed by deviceServiceDiscoverStart
    destroyMockDriver(driver);
}

static void test_multiple_device_classes_all_succeed(void **state)
{
    (void) state;
    resetMockTracking();

    // Create drivers for two different device classes
    DeviceDriver *driver1 = createMockDriver("testDriver1", true, false, true);
    DeviceDriver *driver2 = createMockDriver("testDriver2", true, false, true);

    // First device class driver lists
    icLinkedList *class1Drivers1 = linkedListCreate();
    linkedListAppend(class1Drivers1, driver1);
    icLinkedList *class1Drivers2 = linkedListCreate();
    linkedListAppend(class1Drivers2, driver1);

    // Second device class driver lists
    icLinkedList *class2Drivers1 = linkedListCreate();
    linkedListAppend(class2Drivers1, driver2);
    icLinkedList *class2Drivers2 = linkedListCreate();
    linkedListAppend(class2Drivers2, driver2);

    // Setup expectations - deviceDriverManagerGetDeviceDriversByDeviceClass called twice per class
    will_return(__wrap_deviceDriverManagerGetDeviceDriversByDeviceClass, class1Drivers1);
    will_return(__wrap_deviceDriverManagerGetDeviceDriversByDeviceClass, class2Drivers1);
    will_return(__wrap_deviceDriverManagerGetDeviceDriversByDeviceClass, class1Drivers2);
    will_return(__wrap_deviceDriverManagerGetDeviceDriversByDeviceClass, class2Drivers2);

    expect_value(__wrap_sendDiscoveryStartedEvent, timeoutSeconds, 30);
    expect_value(__wrap_sendDiscoveryStartedEvent, findOrphanedDevices, false);
    expect_string(mockDiscoverDevicesCallback, deviceClass, "testClass1");
    expect_string(mockDiscoverDevicesCallback, deviceClass, "testClass2");

    // Create device class list
    icLinkedList *deviceClasses = linkedListCreate();
    linkedListAppend(deviceClasses, "testClass1");
    linkedListAppend(deviceClasses, "testClass2");

    // Call function under test
    bool result = deviceServiceDiscoverStart(deviceClasses, NULL, 30, false);

    // Verify
    assert_true(result);
    assert_int_equal(2, discoverDevicesCalled);
    assert_int_equal(0, stopDiscoveringDevicesCalled);
    assert_int_equal(1, discoveryStartedEventCalled);
    assert_int_equal(0, discoveryStoppedEventCalled);

    // Cleanup
    linkedListDestroy(deviceClasses, standardDoNotFreeFunc);
    destroyMockDriver(driver1);
    destroyMockDriver(driver2);
}

static void test_multiple_device_classes_one_fails(void **state)
{
    (void) state;
    resetMockTracking();

    // Create drivers for two different device classes
    DeviceDriver *driver1 = createMockDriver("testDriver1", true, false, true);
    DeviceDriver *driver2 = createMockDriver("testDriver2", true, false, true);
    driver2->discoverDevices = mockDiscoverDevicesCallbackAlwaysFails;

    // First device class driver lists
    icLinkedList *class1Drivers1 = linkedListCreate();
    linkedListAppend(class1Drivers1, driver1);
    icLinkedList *class1Drivers2 = linkedListCreate();
    linkedListAppend(class1Drivers2, driver1);

    // Second device class driver lists
    icLinkedList *class2Drivers1 = linkedListCreate();
    linkedListAppend(class2Drivers1, driver2);
    icLinkedList *class2Drivers2 = linkedListCreate();
    linkedListAppend(class2Drivers2, driver2);

    // Setup expectations
    will_return(__wrap_deviceDriverManagerGetDeviceDriversByDeviceClass, class1Drivers1);
    will_return(__wrap_deviceDriverManagerGetDeviceDriversByDeviceClass, class2Drivers1);
    will_return(__wrap_deviceDriverManagerGetDeviceDriversByDeviceClass, class1Drivers2);
    will_return(__wrap_deviceDriverManagerGetDeviceDriversByDeviceClass, class2Drivers2);

    expect_value(__wrap_sendDiscoveryStartedEvent, timeoutSeconds, 30);
    expect_value(__wrap_sendDiscoveryStartedEvent, findOrphanedDevices, false);
    expect_string(mockDiscoverDevicesCallback, deviceClass, "testClass1");
    expect_string(mockDiscoverDevicesCallbackAlwaysFails, deviceClass, "testClass2");
    expect_string(mockStopDiscoveringDevicesCallback, deviceClass, "testClass1");
    // Note: stopped events tracked in list, verified after call (order not guaranteed due to threads)

    // Create device class list
    icLinkedList *deviceClasses = linkedListCreate();
    linkedListAppend(deviceClasses, "testClass1");
    linkedListAppend(deviceClasses, "testClass2");

    // Call function under test
    bool result = deviceServiceDiscoverStart(deviceClasses, NULL, 30, false);

    // Verify - should fail since one class failed
    assert_false(result);
    assert_int_equal(2, discoverDevicesCalled);
    assert_int_equal(1, stopDiscoveringDevicesCalled); // Only successful driver gets stopped
    assert_int_equal(1, discoveryStartedEventCalled);
    assert_int_equal(2, discoveryStoppedEventCalled); // Both classes get stopped events
    // Verify both classes got stopped events (order not guaranteed)
    assert_true(hasStoppedEventForClass("testClass1"));
    assert_true(hasStoppedEventForClass("testClass2"));

    // Cleanup
    linkedListDestroy(deviceClasses, standardDoNotFreeFunc);
    destroyMockDriver(driver1);
    destroyMockDriver(driver2);
}

static void test_multiple_device_classes_multiple_drivers_per_class(void **state)
{
    (void) state;
    resetMockTracking();

    // Create drivers - 2 drivers for class1, 3 drivers for class2
    DeviceDriver *driver1a = createMockDriver("testDriver1a", true, false, true);
    DeviceDriver *driver1b = createMockDriver("testDriver1b", true, false, true);
    DeviceDriver *driver2a = createMockDriver("testDriver2a", true, false, true);
    DeviceDriver *driver2b = createMockDriver("testDriver2b", true, false, true);
    DeviceDriver *driver2c = createMockDriver("testDriver2c", true, false, true);

    // First device class driver lists (2 drivers)
    icLinkedList *class1Drivers1 = linkedListCreate();
    linkedListAppend(class1Drivers1, driver1a);
    linkedListAppend(class1Drivers1, driver1b);
    icLinkedList *class1Drivers2 = linkedListCreate();
    linkedListAppend(class1Drivers2, driver1a);
    linkedListAppend(class1Drivers2, driver1b);

    // Second device class driver lists (3 drivers)
    icLinkedList *class2Drivers1 = linkedListCreate();
    linkedListAppend(class2Drivers1, driver2a);
    linkedListAppend(class2Drivers1, driver2b);
    linkedListAppend(class2Drivers1, driver2c);
    icLinkedList *class2Drivers2 = linkedListCreate();
    linkedListAppend(class2Drivers2, driver2a);
    linkedListAppend(class2Drivers2, driver2b);
    linkedListAppend(class2Drivers2, driver2c);

    // Setup expectations
    will_return(__wrap_deviceDriverManagerGetDeviceDriversByDeviceClass, class1Drivers1);
    will_return(__wrap_deviceDriverManagerGetDeviceDriversByDeviceClass, class2Drivers1);
    will_return(__wrap_deviceDriverManagerGetDeviceDriversByDeviceClass, class1Drivers2);
    will_return(__wrap_deviceDriverManagerGetDeviceDriversByDeviceClass, class2Drivers2);

    expect_value(__wrap_sendDiscoveryStartedEvent, timeoutSeconds, 30);
    expect_value(__wrap_sendDiscoveryStartedEvent, findOrphanedDevices, false);
    expect_string(mockDiscoverDevicesCallback, deviceClass, "testClass1");
    expect_string(mockDiscoverDevicesCallback, deviceClass, "testClass1");
    expect_string(mockDiscoverDevicesCallback, deviceClass, "testClass2");
    expect_string(mockDiscoverDevicesCallback, deviceClass, "testClass2");
    expect_string(mockDiscoverDevicesCallback, deviceClass, "testClass2");

    // Create device class list
    icLinkedList *deviceClasses = linkedListCreate();
    linkedListAppend(deviceClasses, "testClass1");
    linkedListAppend(deviceClasses, "testClass2");

    // Call function under test
    bool result = deviceServiceDiscoverStart(deviceClasses, NULL, 30, false);

    // Verify
    assert_true(result);
    assert_int_equal(5, discoverDevicesCalled); // 2 + 3 drivers
    assert_int_equal(0, stopDiscoveringDevicesCalled);
    assert_int_equal(1, discoveryStartedEventCalled);
    assert_int_equal(0, discoveryStoppedEventCalled);

    // Cleanup
    linkedListDestroy(deviceClasses, standardDoNotFreeFunc);
    destroyMockDriver(driver1a);
    destroyMockDriver(driver1b);
    destroyMockDriver(driver2a);
    destroyMockDriver(driver2b);
    destroyMockDriver(driver2c);
}

static void test_start_discovery_while_already_running(void **state)
{
    (void) state;
    resetMockTracking();

    // First, start discovery for testClass1
    DeviceDriver *driver1 = createMockDriver("testDriver1", true, false, true);
    icLinkedList *drivers1a = linkedListCreate();
    linkedListAppend(drivers1a, driver1);
    icLinkedList *drivers1b = linkedListCreate();
    linkedListAppend(drivers1b, driver1);

    will_return(__wrap_deviceDriverManagerGetDeviceDriversByDeviceClass, drivers1a);
    will_return(__wrap_deviceDriverManagerGetDeviceDriversByDeviceClass, drivers1b);

    expect_value(__wrap_sendDiscoveryStartedEvent, timeoutSeconds, 30);
    expect_value(__wrap_sendDiscoveryStartedEvent, findOrphanedDevices, false);
    expect_string(mockDiscoverDevicesCallback, deviceClass, "testClass1");

    icLinkedList *deviceClasses1 = linkedListCreate();
    linkedListAppend(deviceClasses1, "testClass1");

    bool result1 = deviceServiceDiscoverStart(deviceClasses1, NULL, 30, false);
    assert_true(result1);
    assert_int_equal(1, discoverDevicesCalled);

    // Stop discovery for testClass1 to clean up before second test
    deviceServiceDiscoverStop(deviceClasses1);

    // Now try to start discovery for testClass1 again - should succeed since we stopped it
    resetMockTracking();

    // testClass1 drivers for getDriversForDiscovery check
    icLinkedList *class1Drivers2 = linkedListCreate();
    linkedListAppend(class1Drivers2, driver1);

    // testClass1 drivers for actual start
    icLinkedList *class1Drivers3 = linkedListCreate();
    linkedListAppend(class1Drivers3, driver1);

    will_return(__wrap_deviceDriverManagerGetDeviceDriversByDeviceClass, class1Drivers2);
    will_return(__wrap_deviceDriverManagerGetDeviceDriversByDeviceClass, class1Drivers3);

    expect_value(__wrap_sendDiscoveryStartedEvent, timeoutSeconds, 30);
    expect_value(__wrap_sendDiscoveryStartedEvent, findOrphanedDevices, false);
    expect_string(mockDiscoverDevicesCallback, deviceClass, "testClass1");

    // Now testClass1 is not running anymore, so it should start successfully
    icLinkedList *deviceClasses2 = linkedListCreate();
    linkedListAppend(deviceClasses2, "testClass1");

    bool result2 = deviceServiceDiscoverStart(deviceClasses2, NULL, 30, false);
    assert_true(result2);                       // Should succeed since we stopped first discovery
    assert_int_equal(1, discoverDevicesCalled); // Driver should be called again

    // Cleanup
    linkedListDestroy(deviceClasses1, standardDoNotFreeFunc);
    linkedListDestroy(deviceClasses2, standardDoNotFreeFunc);
    destroyMockDriver(driver1);
}

static void test_start_discovery_new_class_while_another_running(void **state)
{
    (void) state;
    resetMockTracking();

    // First, start discovery for testClass1
    DeviceDriver *driver1 = createMockDriver("testDriver1", true, false, true);
    icLinkedList *drivers1a = linkedListCreate();
    linkedListAppend(drivers1a, driver1);
    icLinkedList *drivers1b = linkedListCreate();
    linkedListAppend(drivers1b, driver1);

    will_return(__wrap_deviceDriverManagerGetDeviceDriversByDeviceClass, drivers1a);
    will_return(__wrap_deviceDriverManagerGetDeviceDriversByDeviceClass, drivers1b);

    expect_value(__wrap_sendDiscoveryStartedEvent, timeoutSeconds, 30);
    expect_value(__wrap_sendDiscoveryStartedEvent, findOrphanedDevices, false);
    expect_string(mockDiscoverDevicesCallback, deviceClass, "testClass1");

    icLinkedList *deviceClasses1 = linkedListCreate();
    linkedListAppend(deviceClasses1, "testClass1");

    bool result1 = deviceServiceDiscoverStart(deviceClasses1, NULL, 30, false);
    assert_true(result1);
    assert_int_equal(1, discoverDevicesCalled);

    // Now start discovery for testClass2 while testClass1 is still running
    resetMockTracking();

    DeviceDriver *driver2 = createMockDriver("testDriver2", true, false, true);

    // testClass2 drivers for getDriversForDiscovery check
    icLinkedList *class2Drivers1 = linkedListCreate();
    linkedListAppend(class2Drivers1, driver2);

    // testClass2 drivers for actual start
    icLinkedList *class2Drivers2 = linkedListCreate();
    linkedListAppend(class2Drivers2, driver2);

    // getDriversForDiscovery is called once for testClass2
    will_return(__wrap_deviceDriverManagerGetDeviceDriversByDeviceClass, class2Drivers1);
    // Then deviceDriverManagerGetDeviceDriversByDeviceClass is called again to actually start drivers
    will_return(__wrap_deviceDriverManagerGetDeviceDriversByDeviceClass, class2Drivers2);

    expect_value(__wrap_sendDiscoveryStartedEvent, timeoutSeconds, 30);
    expect_value(__wrap_sendDiscoveryStartedEvent, findOrphanedDevices, false);
    expect_string(mockDiscoverDevicesCallback, deviceClass, "testClass2");

    icLinkedList *deviceClasses2 = linkedListCreate();
    linkedListAppend(deviceClasses2, "testClass2"); // New class

    bool result2 = deviceServiceDiscoverStart(deviceClasses2, NULL, 30, false);
    assert_true(result2);
    assert_int_equal(1, discoverDevicesCalled); // Only testClass2 driver called

    // Cleanup
    linkedListDestroy(deviceClasses1, standardDoNotFreeFunc);
    linkedListDestroy(deviceClasses2, standardDoNotFreeFunc);
    destroyMockDriver(driver1);
    destroyMockDriver(driver2);
}

/*
 * neverReject test coverage explanation:
 *
 * The neverReject flag controls driver filtering in getDriversForDiscovery():
 * - When deviceServiceIsReadyForDeviceOperation() returns false, drivers with
 *   neverReject=false are filtered out.
 * - Drivers with neverReject=true are always included regardless of service state.
 *
 * Test limitations:
 * We CANNOT mock deviceServiceIsReadyForDeviceOperation() via linker --wrap because
 * it is defined in the same compilation unit (deviceService.c) as getDriversForDiscovery()
 * which calls it. The linker's --wrap option only intercepts external symbol references
 * between object files. Intra-object-file calls are resolved at compile time and bypass
 * the linker entirely.
 *
 * As a result, deviceServiceIsReadyForDeviceOperation() always returns the real value
 * (subsystemsReadyForDevices, which is false in tests), so:
 * - neverReject=true drivers: Always included (tested below)
 * - neverReject=false drivers: Always filtered out when service not ready
 *
 * To test neverReject=false with a ready service would require either:
 * 1. Moving deviceServiceIsReadyForDeviceOperation() to a separate .c file, or
 * 2. Using compile-time function pointer indirection for testability
 */
static void test_neverReject_true_descriptors_ready(void **state)
{
    (void) state;
    resetMockTracking();
    mockDeviceDescriptorsReady = true; // Descriptors are ready

    // Test with neverReject=true driver - should always work when descriptors ready
    DeviceDriver *driver = createMockDriver("testDriver", true, false, true); // neverReject=true
    icLinkedList *drivers1 = linkedListCreate();
    linkedListAppend(drivers1, driver);
    icLinkedList *drivers2 = linkedListCreate();
    linkedListAppend(drivers2, driver);

    will_return(__wrap_deviceDriverManagerGetDeviceDriversByDeviceClass, drivers1);
    will_return(__wrap_deviceDriverManagerGetDeviceDriversByDeviceClass, drivers2);

    expect_value(__wrap_sendDiscoveryStartedEvent, timeoutSeconds, 30);
    expect_value(__wrap_sendDiscoveryStartedEvent, findOrphanedDevices, false);
    expect_string(mockDiscoverDevicesCallback, deviceClass, "testClass");

    icLinkedList *deviceClasses = linkedListCreate();
    linkedListAppend(deviceClasses, "testClass");

    bool result = deviceServiceDiscoverStart(deviceClasses, NULL, 30, false);

    assert_true(result);
    assert_int_equal(1, discoverDevicesCalled);

    linkedListDestroy(deviceClasses, standardDoNotFreeFunc);
    destroyMockDriver(driver);
}

static void test_neverReject_true_descriptors_not_ready_succeeds(void **state)
{
    (void) state;
    resetMockTracking();
    mockDeviceDescriptorsReady = false; // Descriptors NOT ready

    // Test with neverReject=true driver - should work even when descriptors not ready
    DeviceDriver *driver = createMockDriver("testDriver", true, false, true);
    icLinkedList *drivers1 = linkedListCreate();
    linkedListAppend(drivers1, driver);
    icLinkedList *drivers2 = linkedListCreate();
    linkedListAppend(drivers2, driver);

    will_return(__wrap_deviceDriverManagerGetDeviceDriversByDeviceClass, drivers1);
    will_return(__wrap_deviceDriverManagerGetDeviceDriversByDeviceClass, drivers2);

    expect_value(__wrap_sendDiscoveryStartedEvent, timeoutSeconds, 30);
    expect_value(__wrap_sendDiscoveryStartedEvent, findOrphanedDevices, false);
    // Driver WILL be called because neverReject=true bypasses the descriptor check
    expect_string(mockDiscoverDevicesCallback, deviceClass, "testClass");

    icLinkedList *deviceClasses = linkedListCreate();
    linkedListAppend(deviceClasses, "testClass");

    bool result = deviceServiceDiscoverStart(deviceClasses, NULL, 30, false);

    assert_true(result);
    assert_int_equal(1, discoverDevicesCalled);

    linkedListDestroy(deviceClasses, standardDoNotFreeFunc);
    destroyMockDriver(driver);
}

static void test_mixed_neverReject_descriptors_not_ready(void **state)
{
    (void) state;
    resetMockTracking();
    mockDeviceDescriptorsReady = false; // Descriptors NOT ready

    // Create two drivers: one with neverReject=true, one with neverReject=false
    DeviceDriver *driver1 = createMockDriver("testDriver1", true, false, true);  // neverReject=true
    DeviceDriver *driver2 = createMockDriver("testDriver2", true, false, false); // neverReject=false

    icLinkedList *drivers1 = linkedListCreate();
    linkedListAppend(drivers1, driver1);
    linkedListAppend(drivers1, driver2);
    icLinkedList *drivers2 = linkedListCreate();
    linkedListAppend(drivers2, driver1);
    linkedListAppend(drivers2, driver2);

    will_return(__wrap_deviceDriverManagerGetDeviceDriversByDeviceClass, drivers1);
    will_return(__wrap_deviceDriverManagerGetDeviceDriversByDeviceClass, drivers2);

    expect_value(__wrap_sendDiscoveryStartedEvent, timeoutSeconds, 30);
    expect_value(__wrap_sendDiscoveryStartedEvent, findOrphanedDevices, false);
    // Only driver1 (neverReject=true) should be called
    expect_string(mockDiscoverDevicesCallback, deviceClass, "testClass");

    icLinkedList *deviceClasses = linkedListCreate();
    linkedListAppend(deviceClasses, "testClass");

    bool result = deviceServiceDiscoverStart(deviceClasses, NULL, 30, false);

    // Should succeed - at least one driver (neverReject=true) can run
    assert_true(result);
    assert_int_equal(1, discoverDevicesCalled); // Only driver1 called, driver2 skipped

    linkedListDestroy(deviceClasses, standardDoNotFreeFunc);
    destroyMockDriver(driver1);
    destroyMockDriver(driver2);
}

static void test_all_neverReject_false_descriptors_not_ready_fails(void **state)
{
    (void) state;
    resetMockTracking();
    mockDeviceDescriptorsReady = false; // Descriptors NOT ready

    // Create driver with neverReject=false
    DeviceDriver *driver = createMockDriver("testDriver", true, false, false);
    icLinkedList *drivers1 = linkedListCreate();
    linkedListAppend(drivers1, driver);

    will_return(__wrap_deviceDriverManagerGetDeviceDriversByDeviceClass, drivers1);

    // No events should be sent - discovery cannot start at all

    icLinkedList *deviceClasses = linkedListCreate();
    linkedListAppend(deviceClasses, "testClass");

    bool result = deviceServiceDiscoverStart(deviceClasses, NULL, 30, false);

    // Should FAIL - no drivers can run (all have neverReject=false and descriptors not ready)
    assert_false(result);
    assert_int_equal(0, discoverDevicesCalled);
    assert_int_equal(0, discoveryStartedEventCalled); // Event not sent because discovery couldn't start

    linkedListDestroy(deviceClasses, standardDoNotFreeFunc);
    destroyMockDriver(driver);
}


// Test that multiple drivers for same class only send ONE stopped event during rollback
static void test_rollback_multiple_drivers_same_class_single_event(void **state)
{
    (void) state;
    resetMockTracking();

    // Create THREE mock drivers for the same device class - one will fail
    DeviceDriver *driver1 = createMockDriver("testDriver1", true, false, true);
    DeviceDriver *driver2 = createMockDriver("testDriver2", true, false, true);
    DeviceDriver *driver3 = createMockDriver("testDriver3", true, false, true);

    // Make driver3 use the always-fail callback to trigger rollback
    driver3->discoverDevices = mockDiscoverDevicesCallbackAlwaysFails;

    icLinkedList *drivers1 = linkedListCreate();
    linkedListAppend(drivers1, driver1);
    linkedListAppend(drivers1, driver2);
    linkedListAppend(drivers1, driver3);
    icLinkedList *drivers2 = linkedListCreate();
    linkedListAppend(drivers2, driver1);
    linkedListAppend(drivers2, driver2);
    linkedListAppend(drivers2, driver3);

    // Setup expectations
    will_return(__wrap_deviceDriverManagerGetDeviceDriversByDeviceClass, drivers1);
    will_return(__wrap_deviceDriverManagerGetDeviceDriversByDeviceClass, drivers2);

    expect_value(__wrap_sendDiscoveryStartedEvent, timeoutSeconds, 30);
    expect_value(__wrap_sendDiscoveryStartedEvent, findOrphanedDevices, false);

    // All three drivers attempt to start
    expect_string(mockDiscoverDevicesCallback, deviceClass, "testClass");            // driver1 succeeds
    expect_string(mockDiscoverDevicesCallback, deviceClass, "testClass");            // driver2 succeeds
    expect_string(mockDiscoverDevicesCallbackAlwaysFails, deviceClass, "testClass"); // driver3 fails

    // During rollback, both successful drivers get stopped
    expect_string(mockStopDiscoveringDevicesCallback, deviceClass, "testClass"); // driver1 stopped
    expect_string(mockStopDiscoveringDevicesCallback, deviceClass, "testClass"); // driver2 stopped

    // CRITICAL: Only ONE stopped event should be sent for this device class
    // Note: stopped event tracked in list, verified after call

    // Create device class list
    icLinkedList *deviceClasses = linkedListCreate();
    linkedListAppend(deviceClasses, "testClass");

    // Call function under test
    bool result = deviceServiceDiscoverStart(deviceClasses, NULL, 30, false);

    // Verify - should return false since one driver failed
    assert_false(result);
    assert_int_equal(3, discoverDevicesCalled);        // All three drivers attempted
    assert_int_equal(2, stopDiscoveringDevicesCalled); // Only the two successful drivers get stopped
    assert_int_equal(1, discoveryStartedEventCalled);
    assert_int_equal(1, discoveryStoppedEventCalled); // CRITICAL: Exactly one stopped event
    assert_true(hasStoppedEventForClass("testClass"));

    // Cleanup
    linkedListDestroy(deviceClasses, standardDoNotFreeFunc);
    destroyMockDriver(driver1);
    destroyMockDriver(driver2);
    destroyMockDriver(driver3);
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

    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_restore),
        cmocka_unit_test(test_discovery_start_success_single_driver),
        cmocka_unit_test(test_discovery_start_failure_single_driver),
        cmocka_unit_test(test_discovery_start_partial_failure_multiple_drivers),
        cmocka_unit_test(test_recovery_start_success),
        cmocka_unit_test(test_recovery_start_failure),
        cmocka_unit_test(test_multiple_device_classes_all_succeed),
        cmocka_unit_test(test_multiple_device_classes_one_fails),
        cmocka_unit_test(test_multiple_device_classes_multiple_drivers_per_class),
        cmocka_unit_test(test_start_discovery_while_already_running),
        cmocka_unit_test(test_start_discovery_new_class_while_another_running),
        cmocka_unit_test(test_neverReject_true_descriptors_ready),
        cmocka_unit_test(test_neverReject_true_descriptors_not_ready_succeeds),
        cmocka_unit_test(test_mixed_neverReject_descriptors_not_ready),
        cmocka_unit_test(test_all_neverReject_false_descriptors_not_ready_fails),
        cmocka_unit_test(test_rollback_multiple_drivers_same_class_single_event),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
