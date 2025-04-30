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

#include "commonDeviceDefs.h"
#include "device-driver/device-driver.h"
#include "device/icDevice.h"
#include "deviceCommunicationWatchdog.h"
#include "deviceServiceCommFail.h"
#include "deviceServicePrivate.h"
#include "icTypes/icLinkedList.h"
#include "icTypes/icLinkedListFuncs.h"
#include <inttypes.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stddef.h>

#include <cmocka.h>

#define NORMAL_DEVICE_ID   "b45fd62e63a651e8"
#define OVERRIDE_DEVICE_ID "bffb573bfbb8050d"
#define CUSTOM_DEVICE_ID   "38c47427-3760-4772-8dad-4e747c1aee4e"

/* Fixture */

static icLinkedList *drivers;

int group_setup(void **state)
{
    drivers = linkedListCreate();

    return 0;
}

int group_teardown(void **state)
{
    linkedListDestroy(drivers, standardDoNotFreeFunc);

    return 0;
}

int setup(void **state)
{
    linkedListClear(drivers, standardDoNotFreeFunc);

    return 0;
}

static void commFailTimeoutSecsChanged(DeviceDriver *driver, const icDevice *device, uint32_t timeoutSecs)
{
    function_called();
    check_expected(driver);
    check_expected(device);
    check_expected(timeoutSecs);
}

static DeviceDriver normal = {.driverName = "n",
                              .customCommFail = false,
                              .commFailTimeoutSecsChanged = commFailTimeoutSecsChanged};

static DeviceDriver override = {.driverName = "o",
                                .customCommFail = false,
                                .commFailTimeoutSecsChanged = commFailTimeoutSecsChanged};

static DeviceDriver custom = {.driverName = "c",
                              .customCommFail = true,
                              .commFailTimeoutSecsChanged = commFailTimeoutSecsChanged};

bool deviceCommunicationWatchdogIsDeviceMonitored(const char *uuid)
{
    return mock_type(bool);
}

void deviceCommunicationWatchdogMonitorDevice(const char *uuid, const uint32_t commFailTimeoutSeconds, bool inCommFail)
{
    function_called();
    check_expected(uuid);
    check_expected(commFailTimeoutSeconds);
    check_expected(inCommFail);
}

void deviceCommunicationWatchdogStopMonitoringDevice(const char *uuid)
{
    function_called();
    check_expected(uuid);
}

icLinkedList *deviceDriverManagerGetDeviceDrivers(void)
{
    return linkedListClone(drivers);
}

DeviceDriver *deviceDriverManagerGetDeviceDriver(const char *driverName)
{
    function_called();
    check_expected(driverName);

    return mock_type(DeviceDriver *);
}

icDevice *deviceServiceGetDevice(const char *uuid)
{
    /* Not used: for linking fail/restore callbacks (invoked beyond unit) */
    function_called();
    return NULL;
}

void updateDeviceDateLastContacted(const char *deviceUuid)
{
    /* Not used: for linking fail/restore callbacks (invoked beyond unit) */
    function_called();
}

void deviceCommunicationWatchdogInit(deviceCommunicationWatchdogCommFailedCallback failedCallback,
                                     deviceCommunicationWatchdogCommRestoredCallback restoredCallback)
{
    function_called();
}

void deviceCommunicationWatchdogTerm(void)
{
    function_called();
}

const char *deviceGetMetadata(const icDevice *device, const char *key)
{
    assert_string_equal(key, COMMON_DEVICE_METADATA_COMMFAIL_OVERRIDE_SECS);

    return mock_type(char *);
}

const char *deviceGetResourceValueById(const icDevice *device, const char *key)
{
    function_called();
    check_expected(device);
    check_expected(key);

    return mock_type(char *);
}

void deviceDestroy(icDevice *device)
{
    // Nope, fixtures are static duration
}

extern inline void deviceDestroy__auto(icDevice **device);
extern inline void deviceListDestroy(icLinkedList *deviceList);
extern inline void deviceListDestroy__auto(icLinkedList **deviceList);

static icDevice normalDevice = {.managingDeviceDriver = "n", .uuid = NORMAL_DEVICE_ID};
static icDevice customDevice = {.managingDeviceDriver = "c", .uuid = CUSTOM_DEVICE_ID};
static icDevice overrideDevice = {.managingDeviceDriver = "o", .uuid = OVERRIDE_DEVICE_ID};

icLinkedList *deviceServiceGetDevicesByDeviceDriver(const char *driverName)
{
    icLinkedList *devices = linkedListCreate();

    switch (driverName[0])
    {
        case 'n':
            linkedListAppend(devices, &normalDevice);
            break;

        case 'c':
            linkedListAppend(devices, &customDevice);
            break;

        case 'o':
            linkedListAppend(devices, &overrideDevice);
            break;

        default:
            fail_msg("Driver '%s' not supported by test", driverName);
            break;
    }

    return devices;
}

/* Driver */

static void test_commfail_default_timeout(void **state)
{
    linkedListAppend(drivers, &normal);

    expect_function_call(deviceDriverManagerGetDeviceDriver);
    expect_string(deviceDriverManagerGetDeviceDriver, driverName, "n");
    will_return(deviceDriverManagerGetDeviceDriver, &normal);
    will_return(deviceGetMetadata, NULL);

    expect_function_call(commFailTimeoutSecsChanged);
    expect_value(commFailTimeoutSecsChanged, driver, &normal);
    expect_value(commFailTimeoutSecsChanged, device, &normalDevice);
    expect_value(commFailTimeoutSecsChanged, timeoutSecs, 100);

    will_return(deviceCommunicationWatchdogIsDeviceMonitored, false);

    expect_function_call(deviceGetResourceValueById);
    expect_value(deviceGetResourceValueById, device, &normalDevice);
    expect_string(deviceGetResourceValueById, key, COMMON_DEVICE_RESOURCE_COMM_FAIL);
    will_return(deviceGetResourceValueById, false);

    expect_function_call(deviceCommunicationWatchdogMonitorDevice);
    expect_string(deviceCommunicationWatchdogMonitorDevice, uuid, NORMAL_DEVICE_ID);
    expect_value(deviceCommunicationWatchdogMonitorDevice, commFailTimeoutSeconds, 100);
    expect_value(deviceCommunicationWatchdogMonitorDevice, inCommFail, false);

    deviceServiceCommFailSetTimeoutSecs(100);
    assert_int_equal(100, deviceServiceCommFailGetTimeoutSecs());
}

static void test_commfail_default_timeout_change(void **state)
{
    linkedListAppend(drivers, &normal);

    expect_function_call(deviceDriverManagerGetDeviceDriver);
    expect_string(deviceDriverManagerGetDeviceDriver, driverName, "n");
    will_return(deviceDriverManagerGetDeviceDriver, &normal);
    will_return(deviceGetMetadata, NULL);

    expect_function_call(commFailTimeoutSecsChanged);
    expect_value(commFailTimeoutSecsChanged, driver, &normal);
    expect_value(commFailTimeoutSecsChanged, device, &normalDevice);
    expect_value(commFailTimeoutSecsChanged, timeoutSecs, 150);

    will_return(deviceCommunicationWatchdogIsDeviceMonitored, true);

    expect_function_call(deviceCommunicationWatchdogStopMonitoringDevice);
    expect_string(deviceCommunicationWatchdogStopMonitoringDevice, uuid, NORMAL_DEVICE_ID);

    expect_function_call(deviceGetResourceValueById);
    expect_value(deviceGetResourceValueById, device, &normalDevice);
    expect_string(deviceGetResourceValueById, key, COMMON_DEVICE_RESOURCE_COMM_FAIL);
    will_return(deviceGetResourceValueById, false);

    expect_function_call(deviceCommunicationWatchdogMonitorDevice);
    expect_string(deviceCommunicationWatchdogMonitorDevice, uuid, NORMAL_DEVICE_ID);
    expect_value(deviceCommunicationWatchdogMonitorDevice, commFailTimeoutSeconds, 150);
    expect_value(deviceCommunicationWatchdogMonitorDevice, inCommFail, false);

    deviceServiceCommFailSetTimeoutSecs(150);
    assert_int_equal(150, deviceServiceCommFailGetTimeoutSecs());
}

static void test_commfail_custom_monitor(void **state)
{
    linkedListAppend(drivers, &custom);

    expect_function_call(deviceDriverManagerGetDeviceDriver);
    expect_string(deviceDriverManagerGetDeviceDriver, driverName, "c");
    will_return(deviceDriverManagerGetDeviceDriver, &custom);

    expect_function_call(commFailTimeoutSecsChanged);
    expect_value(commFailTimeoutSecsChanged, driver, &custom);
    expect_value(commFailTimeoutSecsChanged, device, &customDevice);
    expect_value(commFailTimeoutSecsChanged, timeoutSecs, 100);

    will_return(deviceGetMetadata, NULL);

    deviceServiceCommFailSetTimeoutSecs(100);
    /* Any call to deviceCommunicationWatchdogMonitorDevice will fail this test */

    assert_int_equal(100, deviceServiceCommFailGetTimeoutSecs());
}

static void test_commfail_override_timeout(void **state)
{
    linkedListAppend(drivers, &override);

    expect_function_call(deviceDriverManagerGetDeviceDriver);
    expect_string(deviceDriverManagerGetDeviceDriver, driverName, "o");
    will_return(deviceDriverManagerGetDeviceDriver, &override);

    will_return(deviceGetMetadata, "300");
    will_return(deviceCommunicationWatchdogIsDeviceMonitored, false);
    expect_function_call(commFailTimeoutSecsChanged);
    expect_value(commFailTimeoutSecsChanged, driver, &override);
    expect_value(commFailTimeoutSecsChanged, device, &overrideDevice);
    expect_value(commFailTimeoutSecsChanged, timeoutSecs, 300);

    expect_function_call(deviceGetResourceValueById);
    expect_value(deviceGetResourceValueById, device, &overrideDevice);
    expect_string(deviceGetResourceValueById, key, COMMON_DEVICE_RESOURCE_COMM_FAIL);
    will_return(deviceGetResourceValueById, false);

    expect_function_call(deviceCommunicationWatchdogMonitorDevice);
    expect_string(deviceCommunicationWatchdogMonitorDevice, uuid, OVERRIDE_DEVICE_ID);
    expect_value(deviceCommunicationWatchdogMonitorDevice, commFailTimeoutSeconds, 300);
    expect_value(deviceCommunicationWatchdogMonitorDevice, inCommFail, false);

    deviceServiceCommFailSetTimeoutSecs(100);
    assert_int_equal(100, deviceServiceCommFailGetTimeoutSecs());
}

static void test_commfail_override_device_timeout(void **state)
{
    will_return(deviceGetMetadata, "28800");
    assert_int_equal(28800, deviceServiceCommFailGetDeviceTimeoutSecs(&overrideDevice));
}

static void test_commfail_default(void **state)
{
    deviceServiceCommFailSetTimeoutSecs(100);
    assert_int_equal(deviceServiceCommFailGetTimeoutSecs(), 100);

    deviceServiceCommFailSetTimeoutSecs(250);
    assert_int_equal(deviceServiceCommFailGetTimeoutSecs(), 250);
}

static void test_commfail_uses_default_on_bad_override(void **state)
{
    deviceServiceCommFailSetTimeoutSecs(100);

    will_return(deviceGetMetadata, "not-a-number");
    assert_int_equal(100, deviceServiceCommFailGetDeviceTimeoutSecs(&overrideDevice));
}

static void test_commfail_hint_calls_notify(void **state)
{
    linkedListAppend(drivers, &normal);

    expect_function_call(deviceDriverManagerGetDeviceDriver);
    expect_string(deviceDriverManagerGetDeviceDriver, driverName, "n");
    will_return(deviceDriverManagerGetDeviceDriver, &normal);
    will_return(deviceGetMetadata, NULL);

    expect_function_call(commFailTimeoutSecsChanged);
    expect_value(commFailTimeoutSecsChanged, driver, &normal);
    expect_value(commFailTimeoutSecsChanged, device, &normalDevice);
    expect_value(commFailTimeoutSecsChanged, timeoutSecs, 100);

    deviceServiceCommFailHintDeviceTimeoutSecs(&normalDevice, 100);
}

static void test_nonexistent_driver_is_ignored(void **state)
{
    deviceServiceCommFailSetTimeoutSecs(500);

    will_return_always(deviceGetMetadata, NULL);

    assert_int_equal(deviceServiceCommFailGetDeviceTimeoutSecs(&normalDevice), 500);

    expect_function_call(deviceDriverManagerGetDeviceDriver);
    expect_string(deviceDriverManagerGetDeviceDriver, driverName, "n");
    will_return(deviceDriverManagerGetDeviceDriver, NULL);

    deviceServiceCommFailSetDeviceTimeoutSecs(&normalDevice, 100);

    expect_function_call(deviceDriverManagerGetDeviceDriver);
    expect_string(deviceDriverManagerGetDeviceDriver, driverName, "n");
    will_return(deviceDriverManagerGetDeviceDriver, NULL);

    deviceServiceCommFailHintDeviceTimeoutSecs(&normalDevice, 200);
}

int main(int argc, const char **argv)
{
    const struct CMUnitTest test[] = {cmocka_unit_test_setup(test_commfail_default_timeout, setup),
                                      cmocka_unit_test_setup(test_commfail_custom_monitor, setup),
                                      cmocka_unit_test_setup(test_commfail_override_timeout, setup),
                                      cmocka_unit_test_setup(test_commfail_override_device_timeout, setup),
                                      cmocka_unit_test_setup(test_commfail_default, setup),
                                      cmocka_unit_test_setup(test_commfail_default_timeout_change, setup),
                                      cmocka_unit_test_setup(test_commfail_uses_default_on_bad_override, setup),
                                      cmocka_unit_test_setup(test_commfail_hint_calls_notify, setup),
                                      cmocka_unit_test_setup(test_nonexistent_driver_is_ignored, setup)};

    return cmocka_run_group_tests(test, group_setup, group_teardown);
}
