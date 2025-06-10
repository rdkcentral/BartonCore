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

#include "deviceCommunicationWatchdog.h"
#include "provider/barton-core-property-provider.h"
#include "icUtil/stringUtils.h"
#include "icConcurrent/threadUtils.h"
#include <cmocka.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static char *failedUuid = NULL;
static char *restoredUuid = NULL;
static taskFunc task = NULL;
static void *taskArg = NULL;

static void commFailCb(const char *uuid)
{
    failedUuid = strdup(uuid);
}

static void commRestoreCb(const char *uuid)
{
    restoredUuid = strdup(uuid);
}

gboolean __wrap_b_core_property_provider_get_property_as_bool(BCorePropertyProvider *self,
    const gchar *property_name,
    gboolean default_value)
{
    function_called();

    return (gboolean)mock();
}

bool __wrap_createThread(pthread_t *tid, taskFunc task, void *taskArg, const char *name)
{
    function_called();
    task = task;
    taskArg = taskArg;

    return (bool)mock();
}

static int test_testSetup(void **state)
{
    (void) state;

    // Initialize the watchdog
    expect_function_call(__wrap_createThread);
    will_return(__wrap_createThread, true);
    expect_function_call(__wrap_b_core_property_provider_get_property_as_bool);
    will_return(__wrap_b_core_property_provider_get_property_as_bool, false);
    deviceCommunicationWatchdogInit(commFailCb, commRestoreCb);
    assert_null(failedUuid);
    assert_null(restoredUuid);

    return 0;
}

static int test_testTeardown(void **state)
{
    (void) state;

    // Terminate the watchdog
    deviceCommunicationWatchdogTerm();

    // Clean up
    free(failedUuid);
    failedUuid = NULL;
    free(restoredUuid);
    restoredUuid = NULL;

    return 0;
}

static void test_deviceCommunicationWatchdogInit(void **state)
{
    (void) state;

    // Test initialization with NULL callbacks
    deviceCommunicationWatchdogInit(NULL, NULL);
    assert_null(failedUuid);
    assert_null(restoredUuid);

    // Test initialization with valid callbacks
    if (task != NULL)
    {
        const char *uuid = "device1";
        uint32_t timeout = 30;
        deviceCommunicationWatchdogMonitorDevice(uuid, timeout, false);
        deviceCommunicationWatchdogSetFastCommfail(true);
        deviceCommunicationWatchdogSetMonitorInterval(10);

        task(taskArg);
        assert_string_equal(failedUuid, uuid);
        free(failedUuid);
        failedUuid = NULL;

        deviceCommunicationWatchdogStopMonitoringDevice(uuid);

    }

    // Test that the watchdog is already initialized
    deviceCommunicationWatchdogInit(commFailCb, commRestoreCb);
    assert_null(failedUuid);
    assert_null(restoredUuid);

}

static void test_deviceCommunicationWatchdogMonitorDevice(void **state)
{
    (void) state;

    const char *uuid = "device1";
    uint32_t timeout = 30;

    // invalid arguments
    deviceCommunicationWatchdogMonitorDevice(NULL, timeout, false);


    // Monitor a device
    deviceCommunicationWatchdogMonitorDevice(uuid, timeout, false);
    assert_true(deviceCommunicationWatchdogIsDeviceMonitored(uuid));

    // Pet the device and ensure it doesn't go into comm fail
    deviceCommunicationWatchdogPetDevice(uuid);
    assert_null(failedUuid);

    // Stop monitoring the device
    deviceCommunicationWatchdogStopMonitoringDevice(uuid);
    assert_false(deviceCommunicationWatchdogIsDeviceMonitored(uuid));

    const char *deviceUuid = "device3";
    // Monitor a device which is already in comm fail
    deviceCommunicationWatchdogMonitorDevice(deviceUuid, timeout, true);
    assert_true(deviceCommunicationWatchdogIsDeviceMonitored(deviceUuid));

    //pet the device which is in comm fail
    deviceCommunicationWatchdogPetDevice(deviceUuid);
    assert_string_equal(restoredUuid, deviceUuid);
    free(restoredUuid);
    restoredUuid = NULL;

    // Stop monitoring the device
    deviceCommunicationWatchdogStopMonitoringDevice(deviceUuid);
    assert_false(deviceCommunicationWatchdogIsDeviceMonitored(deviceUuid));

}

static void test_deviceCommunicationWatchdogForceDeviceInCommFail(void **state)
{
    (void) state;

    //invalid arguments
    deviceCommunicationWatchdogForceDeviceInCommFail(NULL);
    assert_null(failedUuid);

    const char *uuid = "device2";
    uint32_t timeout = 30;

    // Monitor a device
    deviceCommunicationWatchdogMonitorDevice(uuid, timeout, false);

    // Force the device into comm fail
    deviceCommunicationWatchdogForceDeviceInCommFail(uuid);
    assert_string_equal(failedUuid, uuid);

    // Clean up
    free(failedUuid);
    failedUuid = NULL;
    deviceCommunicationWatchdogStopMonitoringDevice(uuid);

    const char *deviceUuid = "device1";
    //monitor a device which is in comm fail
    deviceCommunicationWatchdogMonitorDevice(deviceUuid, timeout, true);
    // again force the device to be in comm fail
    deviceCommunicationWatchdogForceDeviceInCommFail(deviceUuid);
    assert_null(failedUuid);

    deviceCommunicationWatchdogStopMonitoringDevice(uuid);

}

static void test_deviceCommunicationWatchdogSetTimeRemaining(void **state)
{
    (void) state;

    const char *uuid = "device3";
    uint32_t timeout = 30;


    // Monitor a device
    deviceCommunicationWatchdogMonitorDevice(uuid, timeout, false);

    // Set time remaining for the device
    deviceCommunicationWatchdogSetTimeRemainingForDeviceFromLPM(uuid, 10);
    int32_t remainingTime = deviceCommunicationWatchdogGetRemainingCommFailTimeoutForLPM(uuid, timeout);
    assert_int_equal(remainingTime, 10);

    // Clean up
    deviceCommunicationWatchdogStopMonitoringDevice(uuid);

    //Invalid arguments
    remainingTime = deviceCommunicationWatchdogGetRemainingCommFailTimeoutForLPM(NULL, timeout);
    assert_int_equal(remainingTime, -1);

    const char *deviceUuid = "device4";
    //monitor a device which is in comm fail
    deviceCommunicationWatchdogMonitorDevice(deviceUuid, timeout, true);
    deviceCommunicationWatchdogSetTimeRemainingForDeviceFromLPM(deviceUuid, 10);
    remainingTime = deviceCommunicationWatchdogGetRemainingCommFailTimeoutForLPM(deviceUuid, timeout);
    assert_int_equal(remainingTime, -1);
}

static void test_deviceCommunicationWatchdogTerm(void **state)
{
    (void) state;

    const char *uuid = "device3";
    uint32_t timeout = 30;


    // Monitor a device
    deviceCommunicationWatchdogMonitorDevice(uuid, timeout, false);

    // Terminate the watchdog
    deviceCommunicationWatchdogTerm();

    // Ensure no devices are monitored
    assert_false(deviceCommunicationWatchdogIsDeviceMonitored("device3"));
}

int main(int argc, const char **argv)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_deviceCommunicationWatchdogInit, test_testSetup, test_testTeardown),
        cmocka_unit_test_setup_teardown(test_deviceCommunicationWatchdogMonitorDevice, test_testSetup, test_testTeardown),
        cmocka_unit_test_setup_teardown(test_deviceCommunicationWatchdogForceDeviceInCommFail, test_testSetup, test_testTeardown),
        cmocka_unit_test_setup_teardown(test_deviceCommunicationWatchdogSetTimeRemaining, test_testSetup, test_testTeardown),
        cmocka_unit_test_setup_teardown(test_deviceCommunicationWatchdogTerm, test_testSetup, test_testTeardown)
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
