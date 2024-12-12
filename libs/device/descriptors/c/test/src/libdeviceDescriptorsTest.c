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
// Created by Thomas Lea on 7/30/15.
//

#include "../../src/parser.h"
#include <deviceDescriptors.h>
#include <icLog/logging.h>
#include <icUtil/stringUtils.h>
#include <stdio.h>
#include <versionUtils.h>

// cmocka and its dependencies
#include <setjmp.h>
#include <cmocka.h>

/*
 * This tests the internal parser to ensure it parses all device descriptors from AllowList.xml
 */
static void test_load_all_descriptors(void **state)
{
    icLinkedList *descriptors = parseDeviceDescriptors("data/AllowList.xml", NULL);

    assert_non_null(descriptors);

    printf("loaded %d descriptors\n", linkedListCount(descriptors));
    linkedListIterate(descriptors, (linkedListIterateFunc) deviceDescriptorPrint, NULL);
    assert_int_equal(linkedListCount(descriptors), 129);

    linkedListDestroy(descriptors, (linkedListItemFreeFunc) deviceDescriptorFree);

    (void) state; // unused
}

/*
 * This tests the internal parser to ensure it parses all 9 camera device descriptors from AllowList-9CameraDDs.xml
 */
static void test_load_camera_descriptors(void **state)
{
    icLinkedList *descriptors = parseDeviceDescriptors("data/AllowList-9CameraDDs.xml", NULL);

    assert_non_null(descriptors);

    printf("loaded %d descriptors\n", linkedListCount(descriptors));
    linkedListIterate(descriptors, (linkedListIterateFunc) deviceDescriptorPrint, NULL);
    assert_int_equal(linkedListCount(descriptors), 9);

    linkedListDestroy(descriptors, (linkedListItemFreeFunc) deviceDescriptorFree);

    (void) state; // unused
}

/*
 * This tests the internal parser to ensure it parses all matter device descriptors from AllowList-MatterDDs.xml
 */
static void test_load_matter_descriptors(void **state)
{
    icLinkedList *descriptors = parseDeviceDescriptors("data/AllowList-MatterDDs.xml", NULL);

    assert_non_null(descriptors);

    printf("loaded %d descriptors\n", linkedListCount(descriptors));
    linkedListIterate(descriptors, (linkedListIterateFunc) deviceDescriptorPrint, NULL);
    assert_int_equal(linkedListCount(descriptors), 4); // Only 4 valid DDLs, others invalid

    scoped_icLinkedListIterator *iterator = linkedListIteratorCreate(descriptors);
    while (linkedListIteratorHasNext(iterator))
    {
        DeviceDescriptor *dd = linkedListIteratorGetNext(iterator);

        if (stringCompare(dd->uuid, "663e661e-9c6b-4184-b4d3-8739130070a5", false) == 0)
        {
            // First valid entry is for the xfinity chime, so let's validate the (vendorId, productId) tuple
            assert_true(stringCompare(dd->manufacturer, "COMCAST", true) == 0);
            assert_true(stringCompare(dd->model, "XFINITY_CHIME", true) == 0);

            assert_int_equal(dd->category, CATEGORY_TYPE_MATTER);

            assert_non_null(dd->matter);
            assert_int_equal(dd->matter->vendorId, 0x111D);
            assert_int_equal(dd->matter->productId, 0x8001);
        }
        else if (stringCompare(dd->uuid, "663e661e-9c6b-4184-b4d3-8739130070e5", false) == 0)
        {
            // First valid entry for chime: Check category and PID/VID
            assert_int_equal(dd->category, CATEGORY_TYPE_UNKNOWN);

            assert_non_null(dd->matter);
            assert_int_equal(dd->matter->vendorId, 0x111D);
            assert_int_equal(dd->matter->productId, 0x8001);
        }
        else if (stringCompare(dd->uuid, "663e661e-9c6b-4184-b4d3-8739130070c3", false) == 0)
        {
            // Second valid entry for chime: Check ranges
            assert_int_equal(dd->category, CATEGORY_TYPE_MATTER);
            assert_non_null(dd->matter);
            assert_int_equal(dd->matter->vendorId, 0x111D);
            assert_int_equal(dd->matter->productId, 0xFFFF);
        }
        else if (stringCompare(dd->uuid, "663e661e-9c6b-4184-b4d3-8739130040d4", false) == 0)
        {
            // Third valid entry for chime: Check ranges
            assert_int_equal(dd->category, CATEGORY_TYPE_MATTER);
            assert_non_null(dd->matter);
            assert_int_equal(dd->matter->vendorId, 0xFFFF);
            assert_int_equal(dd->matter->productId, 0xFFFF);
        }
    }

    linkedListDestroy(descriptors, (linkedListItemFreeFunc) deviceDescriptorFree);

    (void) state; // unused
}

/*
 * This tests the internal parser to ensure it parses all 122 zigbee device descriptors from AllowList-ZigbeeDDs.xml
 */
static void test_load_zigbee_descriptors(void **state)
{
    icLinkedList *descriptors = parseDeviceDescriptors("data/AllowList-ZigbeeDDs.xml", NULL);

    assert_non_null(descriptors);

    printf("loaded %d descriptors\n", linkedListCount(descriptors));
    linkedListIterate(descriptors, (linkedListIterateFunc) deviceDescriptorPrint, NULL);
    assert_int_equal(linkedListCount(descriptors), 122);

    linkedListDestroy(descriptors, (linkedListItemFreeFunc) deviceDescriptorFree);

    (void) state; // unused
}

/*
 * use the proper public interface to the library and locate a camera DD by
 * model, manufacturer, hardware version, and a firmware version that is a lookup
 * in a *list* of supported firmware versions.
 */
static void test_can_locate_camera_descriptor_from_version_list(void **state)
{
    deviceDescriptorsInit("data/AllowList-9CameraDDs.xml", NULL);

    DeviceDescriptor *dd = deviceDescriptorsGet("iControl", "RC8026", "1", "3.0.01.28");

    assert_non_null(dd);

    deviceDescriptorFree(dd);

    deviceDescriptorsCleanup();

    (void) state; // unused
}

/*
 * use the proper public interface to the library and locate a zigbee DD by
 * model, manufacturer, hardware version, and a firmware version that is a lookup
 * in a *list* of supported firmware versions.
 */
static void test_can_locate_zigbee_descriptor_from_version_list(void **state)
{
    deviceDescriptorsInit("data/AllowList-ZigbeeDDs.xml", NULL);

    DeviceDescriptor *dd = deviceDescriptorsGet("Bosch", "ISW-ZPR1-WP13", "1", "0x02030201");

    assert_non_null(dd);

    deviceDescriptorFree(dd);

    // zigbee descriptors can have hardware versions in decimal or hexidecimal.
    //   The library handles this by internally converting to decimal strings
    //   if required.
    dd = deviceDescriptorsGet("Sercomm Corp.", "SZ-DWS04", "18", "0x23005121");

    assert_non_null(dd);

    deviceDescriptorFree(dd);

    deviceDescriptorsCleanup();

    (void) state; // unused
}

/*
 * use the proper public interface to the library and locate a zigbee DD by
 * model, manufacturer, hardware version, and a firmware version that is a lookup
 * in a *range* of supported firmware versions.
 */
static void test_can_locate_zigbee_descriptor_from_range(void **state)
{
    deviceDescriptorsInit("data/AllowList-ZigbeeDDs.xml", NULL);

    DeviceDescriptor *dd = deviceDescriptorsGet("Yale", "YRD210 PB DB", "17", "0x00750546");

    assert_non_null(dd);

    deviceDescriptorFree(dd);

    deviceDescriptorsCleanup();

    (void) state; // unused
}

/*
 * use the proper public interface to the library and locate a zigbee DD by
 * model, manufacturer, hardware version, and a firmware version that is a lookup
 * in a *range with only minimum* supported firmware version.
 */
static void test_can_locate_zigbee_descriptor_with_only_minimum_in_range(void **state)
{
    deviceDescriptorsInit("data/AllowList-ZigbeeDDs.xml", NULL);

    DeviceDescriptor *dd = deviceDescriptorsGet("Technicolor", "TCHU1AL0", "1", "0x6113a31a");

    assert_non_null(dd);

    deviceDescriptorFree(dd);


    dd = deviceDescriptorsGet("Technicolor", "TCHU1AL0", "1", "0x60077076");

    assert_non_null(dd);

    deviceDescriptorFree(dd);


    deviceDescriptorsCleanup();

    (void) state; // unused
}

/*
 * use the proper public interface to the library and locate a zigbee DD by
 * model, manufacturer, hardware version, and a firmware version that is a lookup
 * in a *range with only minimum* supported firmware version.
 */
static void test_cant_locate_zigbee_descriptor_with_only_minimum_in_range(void **state)
{
    deviceDescriptorsInit("data/AllowList-ZigbeeDDs.xml", NULL);

    DeviceDescriptor *dd = deviceDescriptorsGet("Technicolor", "TCHU1AL0", "1", "0x4c13a31a");

    assert_null(dd);

    deviceDescriptorsCleanup();

    (void) state; // unused
}

/*
 * use the proper public interface to the library and locate a zigbee DD by
 * model, manufacturer, hardware version, and a firmware version that is a lookup
 * in a *range with only minimum* supported firmware version.
 */
static void test_can_locate_zigbee_descriptor_with_only_maximum_in_range(void **state)
{
    deviceDescriptorsInit("data/AllowList-ZigbeeDDs.xml", NULL);

    DeviceDescriptor *dd = deviceDescriptorsGet("Comcast", "m1lte", "1", "0x00000014");

    assert_non_null(dd);

    deviceDescriptorFree(dd);


    dd = deviceDescriptorsGet("Comcast", "m1lte", "1", "0x00000015");

    assert_non_null(dd);

    deviceDescriptorFree(dd);


    deviceDescriptorsCleanup();

    (void) state; // unused
}

/*
 * use the proper public interface to the library and locate a zigbee DD by
 * model, manufacturer, hardware version, and a firmware version that is a lookup
 * in a *range with only minimum* supported firmware version.
 */
static void test_cant_locate_zigbee_descriptor_with_only_maximum_in_range(void **state)
{
    deviceDescriptorsInit("data/AllowList-ZigbeeDDs.xml", NULL);

    DeviceDescriptor *dd = deviceDescriptorsGet("Comcast", "m1lte", "1", "0x00000016");

    assert_null(dd);

    deviceDescriptorsCleanup();

    (void) state; // unused
}

/*
 * use the proper public interface to the library and locate a zigbee DD by
 * model, manufacturer, hardware version, and a firmware version that is a lookup
 * in a *range* of supported firmware versions.
 */
static void test_cant_locate_zigbee_descriptor_from_outside_range(void **state)
{
    deviceDescriptorsInit("data/AllowList-ZigbeeDDs.xml", NULL);

    // Uses range 0x00750545-0x00840850

    // Just one after
    DeviceDescriptor *dd = deviceDescriptorsGet("Yale", "YRD210 PB DB", "17", "0x00840851");

    assert_null(dd);

    // Well outside with a hex digit
    dd = deviceDescriptorsGet("Yale", "YRD210 PB DB", "17", "0x0084085a");

    assert_null(dd);

    // Well outside with a hex digit
    dd = deviceDescriptorsGet("Yale", "YRD210 PB DB", "17", "0x0084a850");

    assert_null(dd);

    // Just one before
    dd = deviceDescriptorsGet("Yale", "YRD210 PB DB", "17", "0x00750544");

    assert_null(dd);

    // Well before
    dd = deviceDescriptorsGet("Yale", "YRD210 PB DB", "17", "0x0065a544");

    assert_null(dd);

    deviceDescriptorsCleanup();

    (void) state; // unused
}

/*
 * use the proper public interface to the library and locate a zigbee DD by
 * model, manufacturer, hardware version, and a firmware version that is a lookup
 * in wildcard of supported firmware versions.
 */
static void test_can_locate_zigbee_descriptor_with_wildcard(void **state)
{
    deviceDescriptorsInit("data/AllowList-ZigbeeDDs.xml", NULL);

    DeviceDescriptor *dd = deviceDescriptorsGet("ACCTON", "SMCDW30-Z", "1", "0x00");

    assert_non_null(dd);

    deviceDescriptorFree(dd);

    deviceDescriptorsCleanup();

    (void) state; // unused
}

/*
 * use the proper public interface to the library and locate a zigbee DD by
 * model, manufacturer, hardware version, and a firmware version that is a lookup
 * in *list and range* of supported firmware versions.
 */
static void test_can_locate_zigbee_descriptor_with_list_and_range(void **state)
{
    deviceDescriptorsInit("data/AllowList-ZigbeeDDs.xml", NULL);

    // Uses 0x02020201,0x02030201-0x02090202

    // For list
    DeviceDescriptor *dd = deviceDescriptorsGet("Bosch", "ISW-ZDL1-WP11G", "1", "0x02020201");
    assert_non_null(dd);

    // Verify firmware version type
    assert_int_equal(dd->firmwareVersions->listType, DEVICE_VERSION_LIST_TYPE_LIST_AND_RANGE);
    deviceDescriptorFree(dd);

    // Just one after in range
    dd = deviceDescriptorsGet("Bosch", "ISW-ZDL1-WP11G", "1", "0x02030202");
    assert_non_null(dd);

    // Verify firmware version type
    assert_int_equal(dd->firmwareVersions->listType, DEVICE_VERSION_LIST_TYPE_LIST_AND_RANGE);
    deviceDescriptorFree(dd);

    deviceDescriptorsCleanup();

    (void) state; // unused
}

/*
 * use the proper public interface to the library and locate a zigbee DD by
 * model, manufacturer, hardware version, and a firmware version that is a lookup
 * in a *list and range* of supported firmware versions.
 */
static void test_cant_locate_zigbee_descriptor_with_list_and_range(void **state)
{
    deviceDescriptorsInit("data/AllowList-ZigbeeDDs.xml", NULL);

    // Uses 0x02020201,0x02030201-0x02090202

    // Just one after
    DeviceDescriptor *dd = deviceDescriptorsGet("Bosch", "ISW-ZDL1-WP11G", "1", "0x02090203");
    assert_null(dd);

    // Well outside with a hex digit
    dd = deviceDescriptorsGet("Bosch", "ISW-ZDL1-WP11G", "1", "0x00B87B5A");
    assert_null(dd);

    // Just one before
    dd = deviceDescriptorsGet("Bosch", "ISW-ZDL1-WP11G", "1", "0x02030200");
    assert_null(dd);

    // Well before
    dd = deviceDescriptorsGet("Bosch", "ISW-ZDL1-WP11G", "1", "0x000FB839");
    assert_null(dd);

    deviceDescriptorsCleanup();

    (void) state; // unused
}

/*
 * use the proper public interface to the library and locate a zigbee DD by
 * model, manufacturer, hardware version, and a firmware version that is a lookup
 * in a *list and range with minimum and maximum* of supported firmware versions.
 */
static void test_can_locate_zigbee_descriptor_with_list_and_range_for_minimum_and_maximum_in_range(void **state)
{
    deviceDescriptorsInit("data/AllowList-ZigbeeDDs.xml", NULL);

    // Check the firmware version value of the list and range (range contains only maximum value).
    // Uses -0x22115401,0x23125402

    // For list
    DeviceDescriptor *dd = deviceDescriptorsGet("Bosch", "RFPR-ZB-MS", "19", "0x23125402");
    assert_non_null(dd);

    // Verify firmware version type
    assert_int_equal(dd->firmwareVersions->listType, DEVICE_VERSION_LIST_TYPE_LIST_AND_RANGE);
    deviceDescriptorFree(dd);

    // Just one before in range
    dd = deviceDescriptorsGet("Bosch", "RFPR-ZB-MS", "19", "0x22115400");
    assert_non_null(dd);

    // Verify firmware version type
    assert_int_equal(dd->firmwareVersions->listType, DEVICE_VERSION_LIST_TYPE_LIST_AND_RANGE);
    deviceDescriptorFree(dd);

    // Check the firmware version value of the list and range (range contains only minimum value).
    // Uses 0x21115401,0x22115401-

    // Just one after in range
    dd = deviceDescriptorsGet("Bosch", "RFDL-ZB-MS", "19", "0x22115402");
    assert_non_null(dd);

    // Verify firmware version type
    assert_int_equal(dd->firmwareVersions->listType, DEVICE_VERSION_LIST_TYPE_LIST_AND_RANGE);
    deviceDescriptorFree(dd);

    // Just start in range
    dd = deviceDescriptorsGet("Bosch", "RFDL-ZB-MS", "19", "0x22115401");
    assert_non_null(dd);

    // Verify firmware version type
    assert_int_equal(dd->firmwareVersions->listType, DEVICE_VERSION_LIST_TYPE_LIST_AND_RANGE);
    deviceDescriptorFree(dd);

    deviceDescriptorsCleanup();

    (void) state; // unused
}

/*
 * use the proper public interface to the library and locate a zigbee DD by
 * model, manufacturer, hardware version, and a firmware version that is a lookup
 * in a *list and range with minimum and maximum* of supported firmware versions.
 */
static void test_cant_locate_zigbee_descriptor_with_list_and_range_for_minimum_and_maximum_in_range(void **state)
{
    deviceDescriptorsInit("data/AllowList-ZigbeeDDs.xml", NULL);

    // Check the firmware version value of the list and range (range contains only maximum value).
    // Uses -0x22115401,0x23125402

    // Just one after in range
    DeviceDescriptor *dd = deviceDescriptorsGet("Bosch", "RFPR-ZB-MS", "19", "0x22115402");
    assert_null(dd);

    // Check the firmware version value of the list and range (range contains only minimum value).
    // Uses 0x21115401,0x22115401-

    // Just one before in range
    dd = deviceDescriptorsGet("Bosch", "RFDL-ZB-MS", "19", "0x22115400");
    assert_null(dd);

    deviceDescriptorsCleanup();

    (void) state; // unused
}

static void test_cant_locate_zigbee_descriptor_with_bad_input_versions(void **state)
{
    deviceDescriptorsInit("data/AllowList-ZigbeeDDs.xml", NULL);

    // For some bad inputs
    DeviceDescriptor *dd = deviceDescriptorsGet("Bosch", "ISW-ZDL1-WP11G", "1", "0x02030jkl");
    assert_null(dd);

    dd = deviceDescriptorsGet("Bosch", "ISW-ZDL1-WP11G", "1", "0x02030#$$");
    assert_null(dd);

    deviceDescriptorsCleanup();

    (void) state; // unused
}

static void printArray(uint32_t *array, uint16_t len)
{
    int i = 0;
    for (i = 0; i < len; i++)
    {
        printf("array[%d]=%d\n", i, array[i]);
    }
}

/*
 * test version parsing
 */
static void test_version_parsing(void **state)
{
    uint16_t arrayLen = 0;
    uint32_t *array = NULL;

    // simple version "1.2.3.4"
    //
    printf("parsing '1.2.3.4'\n");
    array = versionStringToInt("1.2.3.4", &arrayLen);
    assert_int_equal(arrayLen, 4);
    printArray(array, arrayLen);
    free(array);

    // version with alpha "1.2.R34"
    //
    printf("parsing '1.2.R34'\n");
    array = versionStringToInt("1.2.R34", &arrayLen);
    assert_int_equal(arrayLen, 3);
    printArray(array, arrayLen);
    free(array);

    // version with leading zeros "1.2.034"
    //
    printf("parsing '1.2.034'\n");
    array = versionStringToInt("1.2.034", &arrayLen);
    assert_int_equal(arrayLen, 3);
    printArray(array, arrayLen);
    free(array);

    // version with multiple crap chars "1.R2.X3__4010231_"
    //
    printf("parsing '1.R2.X3__4010231'\n");
    array = versionStringToInt("1.R2.X3__4010231", &arrayLen);
    assert_int_equal(arrayLen, 4);
    printArray(array, arrayLen);
    free(array);

    // version with no digits "RXJABC"
    //
    printf("parsing crap 'RXJABC'\n");
    array = versionStringToInt("RXJABC", &arrayLen);
    assert_int_equal(arrayLen, 0);
    free(array);

    // zigbee versions
    printf("parsing '0x00000001'\n");
    array = versionStringToInt("0x00000001", &arrayLen);
    assert_int_equal(arrayLen, 1);
    printArray(array, arrayLen);
    assert_int_equal(array[0], 1);
    free(array);

    printf("parsing '0X00000001'\n");
    array = versionStringToInt("0X00000001", &arrayLen);
    assert_int_equal(arrayLen, 1);
    printArray(array, arrayLen);
    assert_int_equal(array[0], 1);
    free(array);

    (void) state; // unused
}

/*
 * test version compare
 */
static void test_version_compare(void **state)
{
    int8_t val = 0;

    // check equal "1.2.3 == 1.2.3"
    //
    printf("comparing '1.2.3' vs '1.2.3'\n");
    val = compareVersionStrings("1.2.3", "1.2.3");
    assert_int_equal(val, 0);

    // left is greater "1.2.4 == 1.2.3"
    //
    printf("comparing '1.2.4' vs '1.2.3'\n");
    val = compareVersionStrings("1.2.4", "1.2.3");
    assert_int_equal(val, -1);

    // right is greater "1.2.3 == 1.2.4"
    //
    printf("comparing '1.2.3' vs '1.2.4'\n");
    val = compareVersionStrings("1.2.3", "1.2.4");
    assert_int_equal(val, 1);

    // special cases:  left longer
    //
    printf("comparing '1.2.3.4' vs '1.2.3'\n");
    val = compareVersionStrings("1.2.3.4", "1.2.3");
    assert_int_equal(val, -1);

    // special cases:  right longer
    //
    printf("comparing '1.2.3' vs '1.2.3.4'\n");
    val = compareVersionStrings("1.2.3", "1.2.3.4");
    assert_int_equal(val, 1);

    // special cases:  right longer with 0
    //
    printf("comparing '1.2.3' vs '1.2.3.0.00'\n");
    val = compareVersionStrings("1.2.3", "1.2.3.0.00");
    assert_int_equal(val, 0);

    // zigbee versions
    //
    printf("comparing '0x00750545' vs '0x00750546'\n");
    val = compareVersionStrings("0x00750545", "0x00750546");
    assert_int_equal(val, 1);

    printf("comparing '0x00840851' vs '0x00840850'\n");
    val = compareVersionStrings("0x00840851", "0x00840850");
    assert_int_equal(val, -1);

    printf("comparing '0x0084089a' vs '0x00840899'\n");
    val = compareVersionStrings("0x0084089a", "0x00840899");
    assert_int_equal(val, -1);

    (void) state; // unused
}

/*
 * Verify that a device descriptor can be found without a denylist, but cannot be found when one is used that
 * references the device's descriptor.
 */
static void test_denylist(void **state)
{
    // first confirm that we can find it without a denylist
    deviceDescriptorsInit("data/AllowList.xml", NULL);
    DeviceDescriptor *dd = deviceDescriptorsGet("SMC", "SMCCO10-Z", "1", "1");
    assert_non_null(dd);
    deviceDescriptorFree(dd);
    deviceDescriptorsCleanup();

    // now confirm that our denylist, which excludes this device, prevents us from finding the same descriptor
    deviceDescriptorsInit("data/AllowList.xml", "data/DenyList.xml");
    dd = deviceDescriptorsGet("SMC", "SMCCO10-Z", "1", "1");
    assert_null(dd);
    deviceDescriptorsCleanup();

    (void) state; // unused
}

/*
 * Verify proper handling of empty uuid nodes in denylist.
 */
static void test_denylist_empty_uuid(void **state)
{
    deviceDescriptorsInit("data/AllowList.xml", "data/DenyList-EmptyUUID.xml");
    DeviceDescriptor *dd = deviceDescriptorsGet("SMC", "SMCCO10-Z", "1", "1");
    // don't care about the descriptors, just that parsing didn't crash the program
    deviceDescriptorFree(dd);
    deviceDescriptorsCleanup();

    (void) state; // unused
}

/*
 * Verify proper handling of missing uuid nodes in denylist.
 */
static void test_denylist_missing_uuid(void **state)
{
    deviceDescriptorsInit("data/AllowList.xml", "data/DenyList-MissingUUID.xml");
    DeviceDescriptor *dd = deviceDescriptorsGet("SMC", "SMCCO10-Z", "1", "1");
    // don't care about the descriptors, just that parsing didn't crash the program
    deviceDescriptorFree(dd);
    deviceDescriptorsCleanup();

    (void) state; // unused
}

/*
 * Verify we don't crash when getting a device descriptor using null firmware/hardware versions.
 */
static void test_null_version_compare(void **state)
{
    // Get a basic allowlist/denylist
    deviceDescriptorsInit("data/AllowList.xml", "data/DenyList.xml");
    // Fetch a descriptor with null firmware version
    DeviceDescriptor *dd = deviceDescriptorsGet("SMC", "SMCCO10-Z", "1", NULL);
    assert_null(dd);
    // Fetch a descriptor with null hardware version
    dd = deviceDescriptorsGet("SMC", "SMCCO10-Z", NULL, "1");
    assert_null(dd);

    deviceDescriptorsCleanup();

    (void) state;
}

int main(int argc, char **argv)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_load_camera_descriptors),
        cmocka_unit_test(test_load_matter_descriptors),
        cmocka_unit_test(test_load_zigbee_descriptors),
        cmocka_unit_test(test_load_all_descriptors),
        cmocka_unit_test(test_can_locate_camera_descriptor_from_version_list),
        cmocka_unit_test(test_can_locate_zigbee_descriptor_from_version_list),
        cmocka_unit_test(test_can_locate_zigbee_descriptor_with_wildcard),
        cmocka_unit_test(test_can_locate_zigbee_descriptor_from_range),
        cmocka_unit_test(test_can_locate_zigbee_descriptor_with_only_minimum_in_range),
        cmocka_unit_test(test_cant_locate_zigbee_descriptor_with_only_minimum_in_range),
        cmocka_unit_test(test_can_locate_zigbee_descriptor_with_only_maximum_in_range),
        cmocka_unit_test(test_cant_locate_zigbee_descriptor_with_only_maximum_in_range),
        cmocka_unit_test(test_cant_locate_zigbee_descriptor_from_outside_range),
        cmocka_unit_test(test_can_locate_zigbee_descriptor_with_list_and_range),
        cmocka_unit_test(test_cant_locate_zigbee_descriptor_with_list_and_range),
        cmocka_unit_test(test_can_locate_zigbee_descriptor_with_list_and_range_for_minimum_and_maximum_in_range),
        cmocka_unit_test(test_cant_locate_zigbee_descriptor_with_list_and_range_for_minimum_and_maximum_in_range),
        cmocka_unit_test(test_cant_locate_zigbee_descriptor_with_bad_input_versions),
        cmocka_unit_test(test_version_parsing),
        cmocka_unit_test(test_version_compare),
        cmocka_unit_test(test_denylist),
        cmocka_unit_test(test_denylist_empty_uuid),
        cmocka_unit_test(test_denylist_missing_uuid),
        cmocka_unit_test(test_null_version_compare)};

    int rc = cmocka_run_group_tests(tests, NULL, NULL);
    return rc;
}
