//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2021 Comcast
//
// All rights reserved.
//
// This software is protected by copyright laws of the United States
// and of foreign countries. This material may also be protected by
// patent laws of the United States and of foreign countries.
//
// This software is furnished under a license agreement and/or a
// nondisclosure agreement and may only be used or copied in accordance
// with the terms of those agreements.
//
// The mere transfer of this software does not imply any licenses of trade
// secrets, proprietary technology, copyrights, patents, trademarks, or
// any other form of intellectual property whatsoever.
//
// Comcast retains all ownership rights.
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
#include <zhal/zhal.h>

#define LOG_TAG                         "zigbeeDriverCommonTest"
#define ZIGBEE_LIGHT_DEVICE_DRIVER_NAME "zigbeeLight"

icLinkedList *__wrap_deviceServiceGetDevicesBySubsystem(const char *subsystem);

static OtaUpgradeEvent *
createDummyOtaEvent(zhalOtaEventType eventType, uint8_t *buffer, uint16_t bufferLen, bool isSent);
static uint8_t *createDummyZclPayload(uint8_t buffer[], uint16_t bufferLen);
static uint64_t getTimestamp();

// ******************************
// Tests
// ******************************

// As of now there are only OtaUpgradeEvent tests from zigbeeDriverCommon

static void test_zigbeeDriverCommonVerifyLegacyBootloadUpgradeStartedMessage(void **state)
{
    OtaUpgradeEvent *otaEvent = createDummyOtaEvent(ZHAL_OTA_LEGACY_BOOTLOAD_UPGRADE_STARTED_EVENT, NULL, 0, false);
    assert_true(validateOtaUpgradeMessage(otaEvent)); // NULL payload is valid
    freeOtaUpgradeEvent(otaEvent);

    (void) state;
}

static void test_zigbeeDriverCommonVerifyLegacyBootloadUpgradeCompletedMessage(void **state)
{
    OtaUpgradeEvent *otaEvent = createDummyOtaEvent(ZHAL_OTA_LEGACY_BOOTLOAD_UPGRADE_COMPLETED_EVENT, NULL, 0, false);
    assert_true(validateOtaUpgradeMessage(otaEvent)); // NULL payload is valid
    freeOtaUpgradeEvent(otaEvent);

    (void) state;
}

static void test_zigbeeDriverCommonVerifyLegacyBootloadUpgradeFailedMessage(void **state)
{
    OtaUpgradeEvent *otaEvent = createDummyOtaEvent(ZHAL_OTA_LEGACY_BOOTLOAD_UPGRADE_FAILED_EVENT, NULL, 0, false);
    assert_true(validateOtaUpgradeMessage(otaEvent)); // NULL payload is valid
    freeOtaUpgradeEvent(otaEvent);

    (void) state;
}

static void test_zigbeeDriverCommonVerifyImageNotifyMessage(void **state)
{
    OtaUpgradeEvent *otaEvent = createDummyOtaEvent(ZHAL_OTA_IMAGE_NOTIFY_EVENT, NULL, 0, true);
    assert_false(validateOtaUpgradeMessage(otaEvent)); // NULL payload is invalid for imageNotify!
    freeOtaUpgradeEvent(otaEvent);

    otaEvent = createDummyOtaEvent(ZHAL_OTA_IMAGE_NOTIFY_EVENT, createDummyZclPayload((uint8_t[]) {0x00}, 1), 1, true);
    assert_false(validateOtaUpgradeMessage(otaEvent)); // need atleast 2 bytes for payload type 0x00
    freeOtaUpgradeEvent(otaEvent);

    otaEvent =
        createDummyOtaEvent(ZHAL_OTA_IMAGE_NOTIFY_EVENT, createDummyZclPayload((uint8_t[]) {0x00, 0x32}, 2), 2, true);
    assert_true(validateOtaUpgradeMessage(otaEvent)); // payload type is from 0x00 - 0x03
    freeOtaUpgradeEvent(otaEvent);

    otaEvent =
        createDummyOtaEvent(ZHAL_OTA_IMAGE_NOTIFY_EVENT, createDummyZclPayload((uint8_t[]) {0x04, 0x32}, 2), 2, true);
    assert_false(validateOtaUpgradeMessage(otaEvent)); // payload type is from 0x00 - 0x03
    freeOtaUpgradeEvent(otaEvent);

    otaEvent =
        createDummyOtaEvent(ZHAL_OTA_IMAGE_NOTIFY_EVENT, createDummyZclPayload((uint8_t[]) {0x00, 0x01}, 2), 2, true);
    assert_true(validateOtaUpgradeMessage(otaEvent)); // query jitter is from 0x01 - 0x64
    freeOtaUpgradeEvent(otaEvent);

    otaEvent =
        createDummyOtaEvent(ZHAL_OTA_IMAGE_NOTIFY_EVENT, createDummyZclPayload((uint8_t[]) {0x00, 0x00}, 2), 2, true);
    assert_false(validateOtaUpgradeMessage(otaEvent)); // query jitter is from 0x01 - 0x64
    freeOtaUpgradeEvent(otaEvent);

    otaEvent =
        createDummyOtaEvent(ZHAL_OTA_IMAGE_NOTIFY_EVENT, createDummyZclPayload((uint8_t[]) {0x00, 0x64}, 2), 2, true);
    assert_true(validateOtaUpgradeMessage(otaEvent)); // query jitter is from 0x01 - 0x64
    freeOtaUpgradeEvent(otaEvent);

    otaEvent =
        createDummyOtaEvent(ZHAL_OTA_IMAGE_NOTIFY_EVENT, createDummyZclPayload((uint8_t[]) {0x00, 0x65}, 2), 2, true);
    assert_false(validateOtaUpgradeMessage(otaEvent)); // query jitter is from 0x01 - 0x64
    freeOtaUpgradeEvent(otaEvent);

    otaEvent =
        createDummyOtaEvent(ZHAL_OTA_IMAGE_NOTIFY_EVENT, createDummyZclPayload((uint8_t[]) {0x01, 0x32}, 2), 2, true);
    assert_false(validateOtaUpgradeMessage(otaEvent)); // For payload type 0x01 we need atleast 4 bytes
    freeOtaUpgradeEvent(otaEvent);

    otaEvent = createDummyOtaEvent(
        ZHAL_OTA_IMAGE_NOTIFY_EVENT, createDummyZclPayload((uint8_t[]) {0x01, 0x32, 0x00, 0x00}, 4), 4, true);
    assert_true(validateOtaUpgradeMessage(otaEvent)); // For payload type 0x01 we need atleast 4 bytes
    freeOtaUpgradeEvent(otaEvent);

    otaEvent = createDummyOtaEvent(
        ZHAL_OTA_IMAGE_NOTIFY_EVENT, createDummyZclPayload((uint8_t[]) {0x02, 0x32, 0x00, 0x00}, 4), 4, true);
    assert_false(validateOtaUpgradeMessage(otaEvent)); // For payload type 0x02 we need atleast 6 bytes
    freeOtaUpgradeEvent(otaEvent);

    otaEvent = createDummyOtaEvent(ZHAL_OTA_IMAGE_NOTIFY_EVENT,
                                   createDummyZclPayload((uint8_t[]) {0x02, 0x32, 0x00, 0x00, 0x00, 0x00}, 6),
                                   6,
                                   true);
    assert_true(validateOtaUpgradeMessage(otaEvent)); // For payload type 0x02 we need atleast 6 bytes
    freeOtaUpgradeEvent(otaEvent);

    otaEvent = createDummyOtaEvent(ZHAL_OTA_IMAGE_NOTIFY_EVENT,
                                   createDummyZclPayload((uint8_t[]) {0x03, 0x32, 0x00, 0x00, 0x00, 0x00}, 6),
                                   6,
                                   true);
    assert_false(validateOtaUpgradeMessage(otaEvent)); // For payload type 0x03 we need atleast 10 bytes
    freeOtaUpgradeEvent(otaEvent);

    otaEvent = createDummyOtaEvent(
        ZHAL_OTA_IMAGE_NOTIFY_EVENT,
        createDummyZclPayload((uint8_t[]) {0x03, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 10),
        10,
        true);
    assert_true(validateOtaUpgradeMessage(otaEvent)); // For payload type 0x03 we need atleast 10 bytes
    freeOtaUpgradeEvent(otaEvent);

    (void) state;
}

static void test_zigbeeDriverCommonVerifyQueryNextImageRequestMessage(void **state)
{
    OtaUpgradeEvent *otaEvent = createDummyOtaEvent(ZHAL_OTA_QUERY_NEXT_IMAGE_REQUEST_EVENT, NULL, 0, false);
    assert_false(validateOtaUpgradeMessage(otaEvent)); // NULL payload is invalid
    freeOtaUpgradeEvent(otaEvent);

    otaEvent =
        createDummyOtaEvent(ZHAL_OTA_QUERY_NEXT_IMAGE_REQUEST_EVENT,
                            createDummyZclPayload((uint8_t[]) {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 8),
                            8,
                            false);
    assert_false(validateOtaUpgradeMessage(otaEvent)); // need atleast 9 bytes
    freeOtaUpgradeEvent(otaEvent);

    otaEvent = createDummyOtaEvent(
        ZHAL_OTA_QUERY_NEXT_IMAGE_REQUEST_EVENT,
        createDummyZclPayload((uint8_t[]) {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 9),
        9,
        false);
    assert_true(validateOtaUpgradeMessage(otaEvent)); // need atleast 9 bytes
    freeOtaUpgradeEvent(otaEvent);

    (void) state;
}

static void test_zigbeeDriverCommonVerifyQueryNextImageResponseMessage(void **state)
{
    OtaUpgradeEvent *otaEvent = createDummyOtaEvent(ZHAL_OTA_QUERY_NEXT_IMAGE_RESPONSE_EVENT, NULL, 0, true);
    assert_false(validateOtaUpgradeMessage(otaEvent)); // NULL payload is invalid
    freeOtaUpgradeEvent(otaEvent);

    otaEvent = createDummyOtaEvent(
        ZHAL_OTA_QUERY_NEXT_IMAGE_RESPONSE_EVENT, createDummyZclPayload((uint8_t[]) {0x00}, 1), 1, true);
    assert_true(validateOtaUpgradeMessage(otaEvent)); // need atleast 1 byte
    freeOtaUpgradeEvent(otaEvent);

    (void) state;
}

static void test_zigbeeDriverCommonVerifyUpgradeStartedMessage(void **state)
{
    OtaUpgradeEvent *otaEvent = createDummyOtaEvent(ZHAL_OTA_UPGRADE_STARTED_EVENT, NULL, 0, false);
    assert_true(validateOtaUpgradeMessage(otaEvent)); // NULL payload is valid
    freeOtaUpgradeEvent(otaEvent);

    (void) state;
}

static void test_zigbeeDriverCommonVerifyUpgradeEndRequestMessage(void **state)
{
    OtaUpgradeEvent *otaEvent = createDummyOtaEvent(ZHAL_OTA_UPGRADE_END_REQUEST_EVENT, NULL, 0, false);
    assert_false(validateOtaUpgradeMessage(otaEvent)); // NULL payload is invalid
    freeOtaUpgradeEvent(otaEvent);

    otaEvent =
        createDummyOtaEvent(ZHAL_OTA_UPGRADE_END_REQUEST_EVENT,
                            createDummyZclPayload((uint8_t[]) {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 8),
                            8,
                            false);
    assert_false(validateOtaUpgradeMessage(otaEvent)); // should have atleast 9 bytes
    freeOtaUpgradeEvent(otaEvent);

    otaEvent = createDummyOtaEvent(
        ZHAL_OTA_UPGRADE_END_REQUEST_EVENT,
        createDummyZclPayload((uint8_t[]) {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 9),
        9,
        false);
    assert_true(validateOtaUpgradeMessage(otaEvent)); // should have atleast 9 bytes
    freeOtaUpgradeEvent(otaEvent);

    (void) state;
}

static void test_zigbeeDriverCommonVerifyUpgradeEndResponseMessage(void **state)
{
    OtaUpgradeEvent *otaEvent = createDummyOtaEvent(ZHAL_OTA_UPGRADE_END_RESPONSE_EVENT, NULL, 0, true);
    assert_false(validateOtaUpgradeMessage(otaEvent)); // NULL payload is invalid
    freeOtaUpgradeEvent(otaEvent);

    otaEvent = createDummyOtaEvent(
        ZHAL_OTA_UPGRADE_END_RESPONSE_EVENT,
        createDummyZclPayload(
            (uint8_t[]) {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 15),
        15,
        true);
    assert_false(validateOtaUpgradeMessage(otaEvent)); // should have atleast 16 bytes
    freeOtaUpgradeEvent(otaEvent);

    otaEvent = createDummyOtaEvent(
        ZHAL_OTA_UPGRADE_END_RESPONSE_EVENT,
        createDummyZclPayload(
            (uint8_t[]) {
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
            16),
        16,
        true);
    assert_true(validateOtaUpgradeMessage(otaEvent)); // should have atleast 16 bytes
    freeOtaUpgradeEvent(otaEvent);

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

static OtaUpgradeEvent *
createDummyOtaEvent(zhalOtaEventType eventType, uint8_t *buffer, uint16_t bufferLen, bool isSent)
{
    OtaUpgradeEvent *otaEvent = (OtaUpgradeEvent *) calloc(1, sizeof(OtaUpgradeEvent));
    otaEvent->eventType = eventType;
    otaEvent->eui64 = 0x000000000;
    otaEvent->timestamp = getTimestamp();
    otaEvent->buffer = buffer;
    otaEvent->bufferLen = bufferLen;

    if (isSent)
    {
        otaEvent->sentStatus = (ZHAL_STATUS *) malloc(sizeof(ZHAL_STATUS));
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

    icLinkedList *result = (icLinkedList *) mock();
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
