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
// Created by VijayKrishna Ramachandran on 12/6/21.
//

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include "subsystems/zigbee/zigbeeSubsystem.h"
#include <cmocka.h>
#include <commonDeviceDefs.h>
#include <device-driver/device-driver.h>
#include <device/deviceModelHelper.h>
#include <device/icDevice.h>
#include <deviceDrivers/zigbeeDriverCommon.h>
#include <errno.h>
#include <icLog/logging.h>
#include <resourceTypes.h>
#include <stdio.h>
#include <string.h>
#include <subsystems/zigbee/zigbeeSubsystem.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <zhal-client.h>

#define LOG_TAG                         "zigbeeDriverCommonTest"
#define ZIGBEE_LIGHT_DEVICE_DRIVER_NAME "zigbeeLight"

icLinkedList *__wrap_deviceServiceGetDevicesBySubsystem(const char *subsystem);

static ZhalOtaUpgradeEvent *
createDummyOtaEvent(ZhalOtaEventType eventType, uint8_t *buffer, uint16_t bufferLen, bool isSent);
static uint8_t *createDummyZclPayload(uint8_t buffer[], uint16_t bufferLen);
static uint64_t getTimestamp();

// ******************************
// Tests
// ******************************

// As of now there are only ZhalOtaUpgradeEvent tests from zigbeeDriverCommon

static void test_zigbeeDriverCommonVerifyLegacyBootloadUpgradeStartedMessage(void **state)
{
    ZhalOtaUpgradeEvent *otaEvent = createDummyOtaEvent(ZHAL_OTA_LEGACY_BOOTLOAD_UPGRADE_STARTED_EVENT, NULL, 0, false);
    assert_true(validateOtaUpgradeMessage(otaEvent)); // NULL payload is valid
    ZhalOtaUpgradeEvent_release(otaEvent);

    (void) state;
}

static void test_zigbeeDriverCommonVerifyLegacyBootloadUpgradeCompletedMessage(void **state)
{
    ZhalOtaUpgradeEvent *otaEvent = createDummyOtaEvent(ZHAL_OTA_LEGACY_BOOTLOAD_UPGRADE_COMPLETED_EVENT, NULL, 0, false);
    assert_true(validateOtaUpgradeMessage(otaEvent)); // NULL payload is valid
    ZhalOtaUpgradeEvent_release(otaEvent);

    (void) state;
}

static void test_zigbeeDriverCommonVerifyLegacyBootloadUpgradeFailedMessage(void **state)
{
    ZhalOtaUpgradeEvent *otaEvent = createDummyOtaEvent(ZHAL_OTA_LEGACY_BOOTLOAD_UPGRADE_FAILED_EVENT, NULL, 0, false);
    assert_true(validateOtaUpgradeMessage(otaEvent)); // NULL payload is valid
    ZhalOtaUpgradeEvent_release(otaEvent);

    (void) state;
}

static void test_zigbeeDriverCommonVerifyImageNotifyMessage(void **state)
{
    ZhalOtaUpgradeEvent *otaEvent = createDummyOtaEvent(ZHAL_OTA_IMAGE_NOTIFY_EVENT, NULL, 0, true);
    assert_false(validateOtaUpgradeMessage(otaEvent)); // NULL payload is invalid for imageNotify!
    ZhalOtaUpgradeEvent_release(otaEvent);

    otaEvent = createDummyOtaEvent(ZHAL_OTA_IMAGE_NOTIFY_EVENT, createDummyZclPayload((uint8_t[]) {0x00}, 1), 1, true);
    assert_false(validateOtaUpgradeMessage(otaEvent)); // need atleast 2 bytes for payload type 0x00
    ZhalOtaUpgradeEvent_release(otaEvent);

    otaEvent =
        createDummyOtaEvent(ZHAL_OTA_IMAGE_NOTIFY_EVENT, createDummyZclPayload((uint8_t[]) {0x00, 0x32}, 2), 2, true);
    assert_true(validateOtaUpgradeMessage(otaEvent)); // payload type is from 0x00 - 0x03
    ZhalOtaUpgradeEvent_release(otaEvent);

    otaEvent =
        createDummyOtaEvent(ZHAL_OTA_IMAGE_NOTIFY_EVENT, createDummyZclPayload((uint8_t[]) {0x04, 0x32}, 2), 2, true);
    assert_false(validateOtaUpgradeMessage(otaEvent)); // payload type is from 0x00 - 0x03
    ZhalOtaUpgradeEvent_release(otaEvent);

    otaEvent =
        createDummyOtaEvent(ZHAL_OTA_IMAGE_NOTIFY_EVENT, createDummyZclPayload((uint8_t[]) {0x00, 0x01}, 2), 2, true);
    assert_true(validateOtaUpgradeMessage(otaEvent)); // query jitter is from 0x01 - 0x64
    ZhalOtaUpgradeEvent_release(otaEvent);

    otaEvent =
        createDummyOtaEvent(ZHAL_OTA_IMAGE_NOTIFY_EVENT, createDummyZclPayload((uint8_t[]) {0x00, 0x00}, 2), 2, true);
    assert_false(validateOtaUpgradeMessage(otaEvent)); // query jitter is from 0x01 - 0x64
    ZhalOtaUpgradeEvent_release(otaEvent);

    otaEvent =
        createDummyOtaEvent(ZHAL_OTA_IMAGE_NOTIFY_EVENT, createDummyZclPayload((uint8_t[]) {0x00, 0x64}, 2), 2, true);
    assert_true(validateOtaUpgradeMessage(otaEvent)); // query jitter is from 0x01 - 0x64
    ZhalOtaUpgradeEvent_release(otaEvent);

    otaEvent =
        createDummyOtaEvent(ZHAL_OTA_IMAGE_NOTIFY_EVENT, createDummyZclPayload((uint8_t[]) {0x00, 0x65}, 2), 2, true);
    assert_false(validateOtaUpgradeMessage(otaEvent)); // query jitter is from 0x01 - 0x64
    ZhalOtaUpgradeEvent_release(otaEvent);

    otaEvent =
        createDummyOtaEvent(ZHAL_OTA_IMAGE_NOTIFY_EVENT, createDummyZclPayload((uint8_t[]) {0x01, 0x32}, 2), 2, true);
    assert_false(validateOtaUpgradeMessage(otaEvent)); // For payload type 0x01 we need atleast 4 bytes
    ZhalOtaUpgradeEvent_release(otaEvent);

    otaEvent = createDummyOtaEvent(
        ZHAL_OTA_IMAGE_NOTIFY_EVENT, createDummyZclPayload((uint8_t[]) {0x01, 0x32, 0x00, 0x00}, 4), 4, true);
    assert_true(validateOtaUpgradeMessage(otaEvent)); // For payload type 0x01 we need atleast 4 bytes
    ZhalOtaUpgradeEvent_release(otaEvent);

    otaEvent = createDummyOtaEvent(
        ZHAL_OTA_IMAGE_NOTIFY_EVENT, createDummyZclPayload((uint8_t[]) {0x02, 0x32, 0x00, 0x00}, 4), 4, true);
    assert_false(validateOtaUpgradeMessage(otaEvent)); // For payload type 0x02 we need atleast 6 bytes
    ZhalOtaUpgradeEvent_release(otaEvent);

    otaEvent = createDummyOtaEvent(ZHAL_OTA_IMAGE_NOTIFY_EVENT,
                                   createDummyZclPayload((uint8_t[]) {0x02, 0x32, 0x00, 0x00, 0x00, 0x00}, 6),
                                   6,
                                   true);
    assert_true(validateOtaUpgradeMessage(otaEvent)); // For payload type 0x02 we need atleast 6 bytes
    ZhalOtaUpgradeEvent_release(otaEvent);

    otaEvent = createDummyOtaEvent(ZHAL_OTA_IMAGE_NOTIFY_EVENT,
                                   createDummyZclPayload((uint8_t[]) {0x03, 0x32, 0x00, 0x00, 0x00, 0x00}, 6),
                                   6,
                                   true);
    assert_false(validateOtaUpgradeMessage(otaEvent)); // For payload type 0x03 we need atleast 10 bytes
    ZhalOtaUpgradeEvent_release(otaEvent);

    otaEvent = createDummyOtaEvent(
        ZHAL_OTA_IMAGE_NOTIFY_EVENT,
        createDummyZclPayload((uint8_t[]) {0x03, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 10),
        10,
        true);
    assert_true(validateOtaUpgradeMessage(otaEvent)); // For payload type 0x03 we need atleast 10 bytes
    ZhalOtaUpgradeEvent_release(otaEvent);

    (void) state;
}

static void test_zigbeeDriverCommonVerifyQueryNextImageRequestMessage(void **state)
{
    ZhalOtaUpgradeEvent *otaEvent = createDummyOtaEvent(ZHAL_OTA_QUERY_NEXT_IMAGE_REQUEST_EVENT, NULL, 0, false);
    assert_false(validateOtaUpgradeMessage(otaEvent)); // NULL payload is invalid
    ZhalOtaUpgradeEvent_release(otaEvent);

    otaEvent =
        createDummyOtaEvent(ZHAL_OTA_QUERY_NEXT_IMAGE_REQUEST_EVENT,
                            createDummyZclPayload((uint8_t[]) {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 8),
                            8,
                            false);
    assert_false(validateOtaUpgradeMessage(otaEvent)); // need atleast 9 bytes
    ZhalOtaUpgradeEvent_release(otaEvent);

    otaEvent = createDummyOtaEvent(
        ZHAL_OTA_QUERY_NEXT_IMAGE_REQUEST_EVENT,
        createDummyZclPayload((uint8_t[]) {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 9),
        9,
        false);
    assert_true(validateOtaUpgradeMessage(otaEvent)); // need atleast 9 bytes
    ZhalOtaUpgradeEvent_release(otaEvent);

    (void) state;
}

static void test_zigbeeDriverCommonVerifyQueryNextImageResponseMessage(void **state)
{
    ZhalOtaUpgradeEvent *otaEvent = createDummyOtaEvent(ZHAL_OTA_QUERY_NEXT_IMAGE_RESPONSE_EVENT, NULL, 0, true);
    assert_false(validateOtaUpgradeMessage(otaEvent)); // NULL payload is invalid
    ZhalOtaUpgradeEvent_release(otaEvent);

    otaEvent = createDummyOtaEvent(
        ZHAL_OTA_QUERY_NEXT_IMAGE_RESPONSE_EVENT, createDummyZclPayload((uint8_t[]) {0x00}, 1), 1, true);
    assert_true(validateOtaUpgradeMessage(otaEvent)); // need atleast 1 byte
    ZhalOtaUpgradeEvent_release(otaEvent);

    (void) state;
}

static void test_zigbeeDriverCommonVerifyUpgradeStartedMessage(void **state)
{
    ZhalOtaUpgradeEvent *otaEvent = createDummyOtaEvent(ZHAL_OTA_UPGRADE_STARTED_EVENT, NULL, 0, false);
    assert_true(validateOtaUpgradeMessage(otaEvent)); // NULL payload is valid
    ZhalOtaUpgradeEvent_release(otaEvent);

    (void) state;
}

static void test_zigbeeDriverCommonVerifyUpgradeEndRequestMessage(void **state)
{
    ZhalOtaUpgradeEvent *otaEvent = createDummyOtaEvent(ZHAL_OTA_UPGRADE_END_REQUEST_EVENT, NULL, 0, false);
    assert_false(validateOtaUpgradeMessage(otaEvent)); // NULL payload is invalid
    ZhalOtaUpgradeEvent_release(otaEvent);

    otaEvent =
        createDummyOtaEvent(ZHAL_OTA_UPGRADE_END_REQUEST_EVENT,
                            createDummyZclPayload((uint8_t[]) {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 8),
                            8,
                            false);
    assert_false(validateOtaUpgradeMessage(otaEvent)); // should have atleast 9 bytes
    ZhalOtaUpgradeEvent_release(otaEvent);

    otaEvent = createDummyOtaEvent(
        ZHAL_OTA_UPGRADE_END_REQUEST_EVENT,
        createDummyZclPayload((uint8_t[]) {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 9),
        9,
        false);
    assert_true(validateOtaUpgradeMessage(otaEvent)); // should have atleast 9 bytes
    ZhalOtaUpgradeEvent_release(otaEvent);

    (void) state;
}

static void test_zigbeeDriverCommonVerifyUpgradeEndResponseMessage(void **state)
{
    ZhalOtaUpgradeEvent *otaEvent = createDummyOtaEvent(ZHAL_OTA_UPGRADE_END_RESPONSE_EVENT, NULL, 0, true);
    assert_false(validateOtaUpgradeMessage(otaEvent)); // NULL payload is invalid
    ZhalOtaUpgradeEvent_release(otaEvent);

    otaEvent = createDummyOtaEvent(
        ZHAL_OTA_UPGRADE_END_RESPONSE_EVENT,
        createDummyZclPayload(
            (uint8_t[]) {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 15),
        15,
        true);
    assert_false(validateOtaUpgradeMessage(otaEvent)); // should have atleast 16 bytes
    ZhalOtaUpgradeEvent_release(otaEvent);

    otaEvent = createDummyOtaEvent(
        ZHAL_OTA_UPGRADE_END_RESPONSE_EVENT,
        createDummyZclPayload(
            (uint8_t[]) {
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
            16),
        16,
        true);
    assert_true(validateOtaUpgradeMessage(otaEvent)); // should have atleast 16 bytes
    ZhalOtaUpgradeEvent_release(otaEvent);

    (void) state;
}

// ******************************
// Setup/Teardown
// ******************************

static int zigbeeDriverSetup(void **state)
{
    (void) state;

    return 0;
}

static int zigbeeDriverTeardown(void **state)
{
    (void) state;

    return 0;
}

// ******************************
// Helpers
// ******************************

static uint8_t *createDummyZclPayload(uint8_t buffer[], uint16_t bufferLen)
{
    if (bufferLen == 0 || buffer == NULL)
    {
        return NULL;
    }

    uint8_t *payload = (uint8_t *) malloc(bufferLen * sizeof(uint8_t));

    for (int i = 0; i < bufferLen; i++)
    {
        payload[i] = buffer[i];
    }

    return payload;
}

static ZhalOtaUpgradeEvent *
createDummyOtaEvent(ZhalOtaEventType eventType, uint8_t *buffer, uint16_t bufferLen, bool isSent)
{
    ZhalOtaUpgradeEvent *otaEvent = ZhalOtaUpgradeEvent_new();
    otaEvent->eventType = eventType;
    otaEvent->eui64 = 0x000000000;
    otaEvent->timestamp = getTimestamp();
    // Take ownership of the caller-provided (malloc'd) buffer, matching the old
    // semantics where the event owned and freed it.
    otaEvent->buffer = (buffer != NULL) ? g_bytes_new_take(buffer, bufferLen) : NULL;

    if (isSent)
    {
        otaEvent->sentStatus = (ZHAL_STATUS *) g_malloc(sizeof(ZHAL_STATUS));
        *otaEvent->sentStatus = ZHAL_STATUS_OK;
    }

    return otaEvent;
}

static uint64_t getTimestamp()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_usec;
}

// ******************************
// wrapped(mocked) functions
// ******************************

icLinkedList *__wrap_deviceServiceGetDevicesBySubsystem(const char *subsystem)
{
    icLogDebug(LOG_TAG, "%s: subsystem=%s", __FUNCTION__, subsystem);

    icLinkedList *result = mock_type(icLinkedList *);
    return result;
}

int main(int argc, const char **argv)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_zigbeeDriverCommonVerifyLegacyBootloadUpgradeStartedMessage),
        cmocka_unit_test(test_zigbeeDriverCommonVerifyLegacyBootloadUpgradeCompletedMessage),
        cmocka_unit_test(test_zigbeeDriverCommonVerifyLegacyBootloadUpgradeFailedMessage),
        cmocka_unit_test(test_zigbeeDriverCommonVerifyImageNotifyMessage),
        cmocka_unit_test(test_zigbeeDriverCommonVerifyQueryNextImageRequestMessage),
        cmocka_unit_test(test_zigbeeDriverCommonVerifyQueryNextImageResponseMessage),
        cmocka_unit_test(test_zigbeeDriverCommonVerifyUpgradeStartedMessage),
        cmocka_unit_test(test_zigbeeDriverCommonVerifyUpgradeEndRequestMessage),
        cmocka_unit_test(test_zigbeeDriverCommonVerifyUpgradeEndResponseMessage),
    };

    int retval = cmocka_run_group_tests(tests, zigbeeDriverSetup, zigbeeDriverTeardown);

    return retval;
}
