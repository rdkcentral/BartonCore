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

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include "cjson/cJSON.h"
#include "icUtil/stringUtils.h"
#include "subsystemManager.c"
#include <cmocka.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_LOG_TAG "subsystemManagerTest"

static int init(void **state)
{
    (void) state;

    int retVal = 0;

    // Clear auto-registered subsystems (matter, zigbee, thread) that get created
    // via constructor functions and would interfere with our test cases.
    // Note: This calls a private function from subsystemManager.c, which is not ideal
    // but provides a direct way to reset the subsystem state for testing.
    subsystemPreRegistrationDestroy();
    subsystemManagerInitialize(NULL);

    g_autoptr(GPtrArray) subsystems = subsystemManagerGetRegisteredSubsystems();
    if (subsystems->len > 0)
    {
        icLogError(TEST_LOG_TAG, "Test initialization failed. Tests require a clean subsystem state.");
        retVal = -1;
    }

    icLogDebug(TEST_LOG_TAG, "done initializing test");
    return retVal;
}

static int tearDown(void **state)
{
    (void) state;

    subsystemManagerShutdown();

    return 0;
}

bool __wrap_deviceServiceGetSystemProperty(const char *name, char **value)
{
    function_called();
    *value = mock_type(char *);

    return (*value != NULL);
}

bool __wrap_deviceServiceSetSystemProperty(const char *name, const char *value)
{
    function_called();
    check_expected(value);

    return mock_type(bool);
}

static gboolean __wrap_stringCompare(gconstpointer a, gconstpointer b)
{
    return g_strcmp0(a, b) == 0;
}

static bool myInitialize(subsystemInitializedFunc initializedCallback, subsystemDeInitializedFunc deInitializedCallback)
{
    return true;
}


static bool initializeAndMakeSubsystemReady(subsystemInitializedFunc initializedCallback, subsystemDeInitializedFunc deInitializedCallback)
{
    initializedCallback("mySubsystem");
    return true;
}


static bool myMigrate(uint16_t oldVersion, uint16_t newVersion)
{
    function_called();
    check_expected(oldVersion);
    check_expected(newVersion);

    return mock_type(bool);
}

static void mockShutdownFunc(void)
{
    function_called();
}

static void mockOnAllDriversStarted(void)
{
    function_called();
}

static void mockOnAllServicesAvailable(void)
{
    function_called();
}

static void mockPostRestoreConfig(void)
{
    function_called();
}

static void mockOnLPMEnd(void)
{
    function_called();
}

static void mockOnLPMStart(void)
{
    function_called();
}

static bool mockOnRestoreConfig(const char *config, const char *restoreConfig)
{
    function_called();
    check_expected(config);
    check_expected(restoreConfig);
    return mock_type(bool);
}

static void mockSetOtaUpgradeDelay(uint32_t delaySeconds)
{
    function_called();
    icDebug("mockSetOtaUpgradeDelay called with %u", delaySeconds);
    check_expected(delaySeconds);
}

static cJSON *mockGetStatusJson(void)
{
    function_called();
    cJSON *fixture = mock_type(cJSON *);
    return cJSON_Duplicate(fixture, 1);
}

static Subsystem *createTestSubsystem(const char *name)
{
    Subsystem *subsystem = createSubsystem();

    subsystem->name = name;
    subsystem->initialize = myInitialize;
    subsystem->migrate = myMigrate;

    return g_steal_pointer(&subsystem);
}

static void assertRegisteredSubsystemCount(int expectedCount)
{
    g_autoptr(GPtrArray) subsystems = subsystemManagerGetRegisteredSubsystems();
    assert_non_null(subsystems);
    assert_int_equal(subsystems->len, expectedCount);
}

static void assertRegisteredSubsystemName(const char *expectedName)
{
    g_autoptr(GPtrArray) subsystems = subsystemManagerGetRegisteredSubsystems();
    assert_non_null(subsystems);

    guint index = 0; // unused.
    gboolean found = g_ptr_array_find_with_equal_func(subsystems, expectedName, __wrap_stringCompare, &index);
    assert_true(found);
}

void test_subsystem_migration(void **state)
{
    (void) state;

    // Reset subsystem state between test cases to ensure clean initialization
    subsystemManagerShutdown();

    static const char *subsystemName = "mySubsystem";

    g_autoptr(Subsystem) mySubsystem = createTestSubsystem(subsystemName);
    subsystemManagerRegister(mySubsystem);

    const char *zero = "0";
    const char *one = "1";
    const char *two = "2";
    const char *oneHundo = "100";

    will_return_always(__wrap_deviceServiceSetSystemProperty, true);

    // Case: No saved version, subsystem version is 0 (unspecified). Save 0 to properties
    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    will_return(__wrap_deviceServiceGetSystemProperty, NULL);

    expect_function_call(__wrap_deviceServiceSetSystemProperty);
    expect_string(__wrap_deviceServiceSetSystemProperty, value, zero);

    subsystemManagerInitialize(NULL);

    subsystemManagerShutdown();
    subsystemPreRegistrationDestroy();

    // Case : No saved version, subsystem version is 0 (specified). Save 0 to properties
    mySubsystem->version = 0;
    subsystemManagerRegister(mySubsystem);

    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    will_return(__wrap_deviceServiceGetSystemProperty, NULL);

    expect_function_call(__wrap_deviceServiceSetSystemProperty);
    expect_string(__wrap_deviceServiceSetSystemProperty, value, zero);

    subsystemManagerInitialize(NULL);

    subsystemManagerShutdown();
    subsystemPreRegistrationDestroy();

    // Case : No saved version, subsystem version is 1. Save 1 to properties.
    mySubsystem->version = 1;
    subsystemManagerRegister(mySubsystem);

    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    will_return(__wrap_deviceServiceGetSystemProperty, NULL);

    expect_function_call(myMigrate);
    will_return(myMigrate, true);
    expect_value(myMigrate, oldVersion, 0);
    expect_value(myMigrate, newVersion, mySubsystem->version);

    expect_function_call(__wrap_deviceServiceSetSystemProperty);
    expect_string(__wrap_deviceServiceSetSystemProperty, value, one);

    subsystemManagerInitialize(NULL);

    subsystemManagerShutdown();
    subsystemPreRegistrationDestroy();

    // Case : Saved version 1, subsystem version is 1. Save 1 to properties.
    subsystemManagerRegister(mySubsystem);

    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    will_return(__wrap_deviceServiceGetSystemProperty, strdup(one));

    expect_function_call(__wrap_deviceServiceSetSystemProperty);
    expect_string(__wrap_deviceServiceSetSystemProperty, value, one);
    subsystemManagerInitialize(NULL);

    subsystemManagerShutdown();
    subsystemPreRegistrationDestroy();

    // Case : Saved version 1, subsystem version is 2. Save 2 to properties.
    mySubsystem->version = 2;
    subsystemManagerRegister(mySubsystem);

    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    will_return(__wrap_deviceServiceGetSystemProperty, strdup(one));

    expect_function_call(myMigrate);
    will_return(myMigrate, true);
    expect_value(myMigrate, oldVersion, 1);
    expect_value(myMigrate, newVersion, mySubsystem->version);

    expect_function_call(__wrap_deviceServiceSetSystemProperty);
    expect_string(__wrap_deviceServiceSetSystemProperty, value, two);
    subsystemManagerInitialize(NULL);

    subsystemManagerShutdown();
    subsystemPreRegistrationDestroy();


    // Case : Saved version 1, subsystem version is 2. Migration routine fails, don't save.
    subsystemManagerRegister(mySubsystem);

    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    will_return(__wrap_deviceServiceGetSystemProperty, strdup(one));

    expect_function_call(myMigrate);
    will_return(myMigrate, false);
    expect_value(myMigrate, oldVersion, 1);
    expect_value(myMigrate, newVersion, mySubsystem->version);

    subsystemManagerInitialize(NULL);

    subsystemManagerShutdown();
    subsystemPreRegistrationDestroy();

    // Case : Saved version 1, subsystem version is 100. Save 100 to properties.
    mySubsystem->version = 100;
    subsystemManagerRegister(mySubsystem);

    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    will_return(__wrap_deviceServiceGetSystemProperty, strdup(one));

    expect_function_call(myMigrate);
    will_return(myMigrate, true);
    expect_value(myMigrate, oldVersion, 1);
    expect_value(myMigrate, newVersion, mySubsystem->version);

    expect_function_call(__wrap_deviceServiceSetSystemProperty);
    expect_string(__wrap_deviceServiceSetSystemProperty, value, oneHundo);

    subsystemManagerInitialize(NULL);

    subsystemManagerShutdown();
    subsystemPreRegistrationDestroy();

    // Case : Saved version 2, subsystem version is 1. Don't save.
    // This is considered time travel.
    mySubsystem->version = 1;
    subsystemManagerRegister(mySubsystem);

    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    will_return(__wrap_deviceServiceGetSystemProperty, strdup(two));

    subsystemManagerInitialize(NULL);
}

void test_subsystemManagerRegister(void **state)
{
    (void) state;

    static const char *subsystemName = "mySubsystem";

    g_autoptr(Subsystem) mySubsystem = createTestSubsystem(NULL);

    // Case 1: Passing NULL subsystem
    subsystemManagerRegister(NULL);
    assertRegisteredSubsystemCount(0);

    // Case 2: Subsystem with NULL initialize function
    mySubsystem->initialize = NULL;
    subsystemManagerRegister(mySubsystem);
    assertRegisteredSubsystemCount(0);

    // Case 3: Subsystem with NULL name
    mySubsystem->initialize = myInitialize;
    subsystemManagerRegister(mySubsystem);
    assertRegisteredSubsystemCount(0);

    // Case 4: Valid subsystem registration
    mySubsystem->name = subsystemName;
    subsystemManagerRegister(mySubsystem);
    assertRegisteredSubsystemCount(1);
    assertRegisteredSubsystemName(subsystemName);

    subsystemManagerShutdown();

    // Case 5: Registering before initialization
    // The subsystem should be pre-registered, and only registered when
    // subsystemManagerInitialize() is called.
    subsystemManagerRegister(mySubsystem);
    assertRegisteredSubsystemCount(0);

    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    will_return(__wrap_deviceServiceGetSystemProperty, NULL);

    expect_function_call(__wrap_deviceServiceSetSystemProperty);
    expect_string(__wrap_deviceServiceSetSystemProperty, value, "0");
    will_return(__wrap_deviceServiceSetSystemProperty, true);

    subsystemManagerInitialize(NULL);
    assertRegisteredSubsystemCount(1);
}

void test_subsystemManagerGetRegisteredSubsystems(void **state)
{
    (void) state;

    // Case 1: No subsystems registered
    assertRegisteredSubsystemCount(0);

    // Case 2: Single subsystem registered
    static const char *subsystemName1 = "Subsystem1";
    g_autoptr(Subsystem) subsystem1 = createTestSubsystem(subsystemName1);
    subsystemManagerRegister(subsystem1);

    assertRegisteredSubsystemCount(1);
    assertRegisteredSubsystemName(subsystemName1);

    // Case 3: Multiple subsystems registered
    static const char *subsystemName2 = "Subsystem2";
    g_autoptr(Subsystem) subsystem2 = createTestSubsystem(subsystemName2);
    subsystemManagerRegister(subsystem2);

    assertRegisteredSubsystemCount(2);
    assertRegisteredSubsystemName(subsystemName1);
    assertRegisteredSubsystemName(subsystemName2);

    // Case 4: No subsystems after shutdown
    subsystemManagerShutdown();
    assertRegisteredSubsystemCount(0);
}

void test_subsystemManagerGetSubsystemStatusJson(void **state)
{
    (void) state;

    static const char *subsystemName = "zigbee";
    g_autoptr(Subsystem) subsystem = createTestSubsystem(subsystemName);

    // Case 1: Subsystem with NULL getStatusJson function
    subsystem->getStatusJson = NULL;
    subsystemManagerRegister(subsystem);
    cJSON *result = subsystemManagerGetSubsystemStatusJson(subsystemName);
    assert_null(result);

    // Case 2: Subsystem's getStatusJson function exists but returns NULL
    subsystem->getStatusJson = mockGetStatusJson;
    expect_function_call(mockGetStatusJson);
    will_return(mockGetStatusJson, NULL);

    result = subsystemManagerGetSubsystemStatusJson(subsystemName);
    assert_null(result);

    // Case 3: Subsystem with valid getStatusJson function returning a real object
    cJSON *fixture = cJSON_CreateObject();
    cJSON_AddStringToObject(fixture, "status", "test-status");
    subsystem->getStatusJson = mockGetStatusJson;

    expect_function_call(mockGetStatusJson);
    will_return(mockGetStatusJson, fixture);
    result = subsystemManagerGetSubsystemStatusJson(subsystemName);
    assert_non_null(result);

    cJSON *statusItem = cJSON_GetObjectItem(result, "status");
    assert_non_null(statusItem);
    assert_string_equal(statusItem->valuestring, "test-status");

    cJSON_Delete(fixture);
    cJSON_Delete(result);
}

void test_subsystemManagerShutdown(void **state)
{
    (void) state;

    static const char *subsystemName = "mySubsystem";
    g_autoptr(Subsystem) mySubsystem = createTestSubsystem(subsystemName);

    // Case 1: Subsystem with NULL shutdown function
    subsystemManagerRegister(mySubsystem);
    subsystemManagerShutdown();
    assertRegisteredSubsystemCount(0);

    subsystemManagerInitialize(NULL);

    static const char *subsystemName2 = "mySubsystem2";
    g_autoptr(Subsystem) mySubsystem2 = createTestSubsystem(subsystemName2);
    subsystemManagerRegister(mySubsystem2);

    // Case 2: Subsystem with valid shutdown function
    mySubsystem2->shutdown = mockShutdownFunc;
    expect_function_call(mockShutdownFunc);
    subsystemManagerShutdown();
    assertRegisteredSubsystemCount(0);
}

static void test_subsystemManagerAllDriversStarted(void **state)
{
    (void) state;

    static const char *subsystemName = "mySubsystem";
    g_autoptr(Subsystem) mySubsystem = createTestSubsystem(subsystemName);
    mySubsystem->onAllDriversStarted = NULL;

    // Case 1: Subsystem with NULL onAllDriversStarted function
    subsystemManagerRegister(mySubsystem);
    subsystemManagerAllDriversStarted();

    // Case 2: Subsystem with valid onAllDriversStarted function
    mySubsystem->onAllDriversStarted = mockOnAllDriversStarted;
    expect_function_call(mockOnAllDriversStarted);
    subsystemManagerAllDriversStarted();
}

static void test_subsystemManagerAllServicesAvailable(void **state)
{
    (void) state;

    static const char *subsystemName = "mySubsystem";
    g_autoptr(Subsystem) mySubsystem = createTestSubsystem(subsystemName);
    mySubsystem->onAllServicesAvailable = NULL;

    // Case 1: Subsystem with NULL onAllServicesAvailable function
    subsystemManagerRegister(mySubsystem);
    subsystemManagerAllServicesAvailable();

    // Case 2: Subsystem with valid onAllServicesAvailable function
    mySubsystem->onAllServicesAvailable = mockOnAllServicesAvailable;
    expect_function_call(mockOnAllServicesAvailable);
    subsystemManagerAllServicesAvailable();
}

static void test_subsystemManagerPostRestoreConfig(void **state)
{
    (void) state;

    static const char *subsystemName = "mySubsystem";
    g_autoptr(Subsystem) mySubsystem = createTestSubsystem(subsystemName);
    mySubsystem->onPostRestoreConfig = NULL;

    // Case 1: Subsystem with NULL postRestoreConfig function
    subsystemManagerRegister(mySubsystem);
    subsystemManagerPostRestoreConfig();

    // Case 2: Subsystem with valid postRestoreConfig function
    mySubsystem->onPostRestoreConfig = mockPostRestoreConfig;
    expect_function_call(mockPostRestoreConfig);
    subsystemManagerPostRestoreConfig();
}

static void test_subsystemManagerSetOtaUpgradeDelay(void **state)
{
    (void) state;

    static const char *subsystemName = "mySubsystem";
    g_autoptr(Subsystem) mySubsystem = createTestSubsystem(subsystemName);
    mySubsystem->setOtaUpgradeDelay = NULL;

    // Case 1: Subsystem with NULL setOtaUpgradeDelay function
    subsystemManagerRegister(mySubsystem);
    subsystemManagerSetOtaUpgradeDelay(10);

    // Case 2: Subsystem with valid setOtaUpgradeDelay function
    mySubsystem->setOtaUpgradeDelay = mockSetOtaUpgradeDelay;
    expect_function_call(mockSetOtaUpgradeDelay);
    expect_value(mockSetOtaUpgradeDelay, delaySeconds, 10);
    subsystemManagerSetOtaUpgradeDelay(10);

    // Case 3: Subsystem with valid setOtaUpgradeDelay function with 0 delay
    expect_function_call(mockSetOtaUpgradeDelay);
    expect_value(mockSetOtaUpgradeDelay, delaySeconds, 0);
    subsystemManagerSetOtaUpgradeDelay(0);

    // Case 4: Subsystem with valid setOtaUpgradeDelay function with large delay
    expect_function_call(mockSetOtaUpgradeDelay);
    expect_value(mockSetOtaUpgradeDelay, delaySeconds, 3600);
    subsystemManagerSetOtaUpgradeDelay(3600);
}

static void test_subsystemManagerExitLPM(void **state)
{
    (void) state;

    static const char *subsystemName = "mySubsystem";
    g_autoptr(Subsystem) mySubsystem = createTestSubsystem(subsystemName);
    mySubsystem->onLPMEnd = NULL;

    // Case 1: Subsystem with NULL onLPMEnd function
    subsystemManagerRegister(mySubsystem);
    subsystemManagerExitLPM();

    // Case 2: Subsystem with valid onLPMEnd function
    mySubsystem->onLPMEnd = mockOnLPMEnd;
    expect_function_call(mockOnLPMEnd);
    subsystemManagerExitLPM();
}

static void test_subsystemManagerEnterLPM(void **state)
{
    (void) state;

    static const char *subsystemName = "mySubsystem";
    g_autoptr(Subsystem) mySubsystem = createTestSubsystem(subsystemName);

    // Case 1: Subsystem with NULL onLPMStart function
    subsystemManagerRegister(mySubsystem);
    subsystemManagerEnterLPM();

    // Case 2: Subsystem with valid onLPMStart function
    mySubsystem->onLPMStart = mockOnLPMStart;
    expect_function_call(mockOnLPMStart);
    subsystemManagerEnterLPM();
}


static void test_subsystemManagerIsSubsystemReady(void **state)
{
    (void) state;

    // Reset subsystem state between test cases to ensure clean initialization
    subsystemManagerShutdown();

    static const char *subsystemName = "mySubsystem";
    g_autoptr(Subsystem) mySubsystem = createTestSubsystem(subsystemName);
    mySubsystem->version = 0;
    const char *zero = "0";
    subsystemManagerRegister(mySubsystem);

    will_return_always(__wrap_deviceServiceSetSystemProperty, true);

    // case 1: Subsystem not ready
    // We are mocking a missing system property for the property name: "mySubsystemSubsystemVersion"
    // This means there is no prior version recorded for this subsystem
    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    will_return(__wrap_deviceServiceGetSystemProperty, NULL);

    // Expect migration logic to attempt saving the version as "0"
    // The property key is still "mySubsystemSubsystemVersion", and value is "0"
    expect_function_call(__wrap_deviceServiceSetSystemProperty);
    expect_string(__wrap_deviceServiceSetSystemProperty, value, zero);

    subsystemManagerInitialize(NULL);
    bool isReady = subsystemManagerIsSubsystemReady(subsystemName);
    assert_false(isReady);

    subsystemManagerShutdown();
    subsystemPreRegistrationDestroy();

    // case 2: Mark subsystem as ready
    // Same mocked behavior as before (missing property), but now the subsystem will initialize and report as ready.
    mySubsystem->initialize = initializeAndMakeSubsystemReady;
    subsystemManagerRegister(mySubsystem);

    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    will_return(__wrap_deviceServiceGetSystemProperty, NULL);

    expect_function_call(__wrap_deviceServiceSetSystemProperty);
    expect_string(__wrap_deviceServiceSetSystemProperty, value, zero);

    subsystemManagerInitialize(NULL);
    isReady = subsystemManagerIsSubsystemReady(subsystemName);
    assert_true(isReady);
}

static void test_subsystemManagerRestoreConfig(void **state)
{
    (void) state;

    static const char *subsystemName = "mySubsystem";
    g_autoptr(Subsystem) mySubsystem = createTestSubsystem(subsystemName);
    mySubsystem->onRestoreConfig = NULL;
    const char *config = "config";
    const char *restoreConfig = "restoreConfig";

    // Case 1: Subsystem with NULL onRestoreConfig function
    subsystemManagerRegister(mySubsystem);
    bool result = subsystemManagerRestoreConfig(config, restoreConfig);
    assert_true(result);

    // Case 2: Subsystem with valid onRestoreConfig function and returns false
    mySubsystem->onRestoreConfig = mockOnRestoreConfig;
    expect_function_call(mockOnRestoreConfig);
    expect_string(mockOnRestoreConfig, config, "config");
    expect_string(mockOnRestoreConfig, restoreConfig, "restoreConfig");
    will_return(mockOnRestoreConfig, false);
    result = subsystemManagerRestoreConfig("config", "restoreConfig");
    assert_false(result);

    // Case 3: Subsystem with valid onRestoreConfig function and returns true
    expect_function_call(mockOnRestoreConfig);
    expect_string(mockOnRestoreConfig, config, "config");
    expect_string(mockOnRestoreConfig, restoreConfig, "restoreConfig");
    will_return(mockOnRestoreConfig, true);
    result = subsystemManagerRestoreConfig("config", "restoreConfig");
    assert_true(result);
}

int main(int argc, const char **argv)
{
    const struct CMUnitTest test[] = {
        cmocka_unit_test_setup_teardown(test_subsystem_migration, init, tearDown),
        cmocka_unit_test_setup_teardown(test_subsystemManagerRegister, init, tearDown),
        cmocka_unit_test_setup_teardown(test_subsystemManagerGetRegisteredSubsystems, init, tearDown),
        cmocka_unit_test_setup_teardown(test_subsystemManagerGetSubsystemStatusJson, init, tearDown),
        cmocka_unit_test_setup_teardown(test_subsystemManagerShutdown, init, tearDown),
        cmocka_unit_test_setup_teardown(test_subsystemManagerAllDriversStarted, init, tearDown),
        cmocka_unit_test_setup_teardown(test_subsystemManagerAllServicesAvailable, init, tearDown),
        cmocka_unit_test_setup_teardown(test_subsystemManagerPostRestoreConfig, init, tearDown),
        cmocka_unit_test_setup_teardown(test_subsystemManagerSetOtaUpgradeDelay, init, tearDown),
        cmocka_unit_test_setup_teardown(test_subsystemManagerExitLPM, init, tearDown),
        cmocka_unit_test_setup_teardown(test_subsystemManagerEnterLPM, init, tearDown),
        cmocka_unit_test_setup_teardown(test_subsystemManagerIsSubsystemReady, init, tearDown),
        cmocka_unit_test_setup_teardown(test_subsystemManagerRestoreConfig, init, tearDown),
    };

    return cmocka_run_group_tests(test, NULL, NULL);
}
