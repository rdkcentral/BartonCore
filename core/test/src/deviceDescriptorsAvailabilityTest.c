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
// Created by naeem on 12/16/21.
//

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include "../../src/deviceDescriptorHandler.h"
#include "icUtil/stringUtils.h"
#include "provider/barton-core-property-provider.h"
#include "urlHelper/urlHelper.h"
#include <asm/errno.h>
#include <cmocka.h>
#include <deviceDescriptors.h>
#include <icConcurrent/timedWait.h>
#include <icCrypto/digest.h>
#include <icTime/timeUtils.h>
#include <icUtil/fileUtils.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define DEVICE_DESCRIPTORS_READY_TIMEOUT_SECS 5

static pthread_cond_t cond;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static void remoteDeviceDescriptorsReadyForPairingCb(void)
{
    pthread_mutex_lock(&mutex);
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
}

static void localDeviceDescriptorsReadyForPairingCb(void)
{
    function_called();
}

static void descriptorsUpdatedCb(void)
{
    return;
}

gboolean __wrap_b_core_property_provider_has_property(BCorePropertyProvider *provider,
                                                                const char *propName)
{
    return mock_type(gboolean);
}

gchar *__wrap_b_core_property_provider_get_property_as_string(BCorePropertyProvider *provider,
                                                                        const char *propName,
                                                                        const char *defValue)
{
    // not great but this unit test erroneously runs in different threads which is a big no-no
    // in cmocka. When it tries to ask about sslVerify prop stuff, it's a different thread
    // context and so we can't rely on mocked types. Just return "none" - we don't care about
    // TLS for any of this.
    if (stringCompare(propName, "cpe.sslCert.validateForHttpsToServer", false) == 0)
    {
        return strdup("none");
    }

    char *val = mock_ptr_type(char *);
    if (val != NULL)
    {
        return strdup(val);
    }

    return NULL;
}

bool __wrap_deviceServiceSetSystemProperty(const char *name, const char *value)
{
    return true;
}

bool __wrap_deviceServiceGetSystemProperty(const char *name, char **value)
{
    if (stringCompare(name, "currentDeviceDescriptorUrl", false) == 0)
    {
        *value = NULL; // triggers download
    }
    else if (stringCompare(name, "currentDeviceDescriptorMd5", false) == 0)
    {
        *value = NULL; // triggers download
    }
    else if (stringCompare(name, "currentBlacklistUrl", false) == 0)
    {
        *value = NULL;
    }
    else if (stringCompare(name, "currentBlacklistMd5", false) == 0)
    {
        *value = NULL;
    }
    else if (stringCompare(name, "DEFAULT_INVALID_BLACKLIST_URL", false) == 0)
    {
        *value = NULL;
    }
    else
    {
        *value = NULL;
    }

    return true;
}

uint8_t *__wrap_digestFile(const char *filename, CryptoDigest algorithm, uint8_t *digestLen)
{
    return NULL;
}

size_t __wrap_urlHelperDownloadFile(const char *url,
                                    long *httpCode,
                                    const char *username,
                                    const char *password,
                                    uint32_t timeoutSecs,
                                    sslVerify verifyFlag,
                                    bool allowCellular,
                                    const char *pathname)
{
    assert_true(copyFileByPath(url, pathname));
    *httpCode = 0;
    return 1;
}

static void test_remote_device_descriptors_availability(void **state)
{
    (void) state; /* unused */

    scoped_generic char *localWhiteListPath = stringBuilder("%s%s", TEST_DIR, "TmpAllowList.xml");
    scoped_generic char *remoteWhiteListPath = stringBuilder("%sAllowList.xml", TEST_DIR);

    // remove temporary local file, if present, from last test run
    if (doesFileExist(localWhiteListPath))
    {
        assert_int_equal(unlink(localWhiteListPath), 0);
    }

    deviceDescriptorsInit(localWhiteListPath, NULL);

    will_return(__wrap_b_core_property_provider_has_property,
                false); // deviceDescriptor.whitelist.url.override
    will_return(__wrap_b_core_property_provider_has_property,
                true); // deviceDescriptorList
    will_return(__wrap_b_core_property_provider_get_property_as_string,
                remoteWhiteListPath); // deviceDescriptorList
    // set it to NULL to set isBlacklistValid flag
    will_return(__wrap_b_core_property_provider_get_property_as_string, NULL); // deviceDescriptor.blacklist

    pthread_mutex_lock(&mutex);

    deviceServiceDeviceDescriptorsInit(remoteDeviceDescriptorsReadyForPairingCb, descriptorsUpdatedCb);
    // we should be signalled through deviceDescriptorsReadyForPairingCb, indicating descriptors are ready
    int waitRet = incrementalCondTimedWait(&cond, &mutex, DEVICE_DESCRIPTORS_READY_TIMEOUT_SECS);
    // Unlock before asserting to ensure a failed assert does not cause us to hang
    pthread_mutex_unlock(&mutex);
    assert_int_equal(waitRet, 0);

    assert_int_equal(unlink(localWhiteListPath), 0);
    deviceDescriptorsCleanup();
}

static void test_local_device_descriptors_availability(void **state)
{
    (void) state; /* unused */

    scoped_generic char *localWhiteListPath = stringBuilder("%sAllowList.xml", TEST_DIR);

    deviceDescriptorsInit(localWhiteListPath, NULL);

    will_return(__wrap_b_core_property_provider_has_property,
                false); // deviceDescriptor.whitelist.url.override
    will_return(__wrap_b_core_property_provider_has_property, false); // deviceDescriptorList
    // set it to NULL to set isBlacklistValid flag
    will_return(__wrap_b_core_property_provider_get_property_as_string, NULL); // deviceDescriptor.blacklist

    expect_function_call(localDeviceDescriptorsReadyForPairingCb);
    deviceServiceDeviceDescriptorsInit(localDeviceDescriptorsReadyForPairingCb, descriptorsUpdatedCb);

    deviceDescriptorsCleanup();
}

int main(int argc, const char **argv)
{
    initTimedWaitCond(&cond);

    const struct CMUnitTest test[] = {
        cmocka_unit_test(test_local_device_descriptors_availability),
        cmocka_unit_test(test_remote_device_descriptors_availability),
    };

    return cmocka_run_group_tests(test, NULL, NULL);
}
