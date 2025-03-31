//------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2025 Comcast Cable Communications Management, LLC
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
// Created by Micah Koch on 2/28/25.
//
#include <setjmp.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <cmocka.h>

#include "deviceServiceConfiguration.h"
#include "device-service-initialize-params-container.h"

static int setup(void **state)
{
    (void)state;

    g_autoptr(BDeviceServiceInitializeParamsContainer) initializeParams = b_device_service_initialize_params_container_new();

    deviceServiceConfigurationStartup(initializeParams);

    return 0;
}

static int teardown(void **state)
{
    (void)state;

    deviceServiceConfigurationShutdown();

    return 0;
}

static void accountIdSetListener(const gchar *accountId)
{
    function_called();
    check_expected(accountId);
}

static void accountIdSetListener2(const gchar *accountId)
{
    function_called();
    check_expected(accountId);
}

static void test_account_id_listeners(void **state)
{
    (void)state;

    const char *accountId = "testAccountId";
    const char *accountId2 = "testAccountId2";

    // Test single listener
    deviceServiceConfigurationRegisterAccountIdListener(accountIdSetListener);
    expect_function_call(accountIdSetListener);
    expect_string(accountIdSetListener, accountId, accountId);
    deviceServiceConfigurationSetAccountId(accountId);

    // multiple listeners
    deviceServiceConfigurationRegisterAccountIdListener(accountIdSetListener2);
    expect_function_call(accountIdSetListener);
    expect_string(accountIdSetListener, accountId, accountId2);
    expect_function_call(accountIdSetListener2);
    expect_string(accountIdSetListener2, accountId, accountId2);
    deviceServiceConfigurationSetAccountId(accountId2);

    // Remove 1 listener
    deviceServiceConfigurationUnregisterAccountIdListener(accountIdSetListener);
    expect_function_call(accountIdSetListener2);
    expect_string(accountIdSetListener2, accountId, accountId);
    deviceServiceConfigurationSetAccountId(accountId);

    // Final cleanup
    deviceServiceConfigurationUnregisterAccountIdListener(accountIdSetListener2);
}

int main(int argc, const char **argv)
{
    const struct CMUnitTest tests[] = {cmocka_unit_test(test_account_id_listeners)};

    return cmocka_run_group_tests(tests, setup, teardown);
}
