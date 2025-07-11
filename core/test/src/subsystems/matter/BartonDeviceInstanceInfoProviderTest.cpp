// ------------------------------ tabstop = 4 ----------------------------------
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
// ------------------------------ tabstop = 4 ----------------------------------

//
// Created by Thomas Lea on 7/10/2025.
// leveraged Raiyan Chowdhury's work on CommissionableDataProviderTest.cpp
//

#include "BartonDeviceInstanceInfoProvider.hpp"
#include "lib/support/Base64.h"
#include "lib/support/Span.h"
#include "platform/CHIPDeviceError.h"
#include <cstdint>
#include <gtest/gtest.h>

// C includes
extern "C" {
#include "MockPropertyProvider.h"
#include "barton-core-properties.h"
#include "deviceServiceConfiguration.h"
#include "provider/barton-core-default-property-provider.h"
#include "provider/barton-core-property-provider.h"
#include "icTypes/icStringHashMap.h"
}

namespace barton {

class BartonDeviceInstanceInfoProviderTest : public ::testing::Test
{
public:
    BartonDeviceInstanceInfoProvider *bartonDeviceInstanceInfoProvider;

    void SetUp() override
    {
        bartonDeviceInstanceInfoProvider = new BartonDeviceInstanceInfoProvider();
        MockPropertyProviderInit();
    }

    void TearDown() override
    {
        delete bartonDeviceInstanceInfoProvider;
        bartonDeviceInstanceInfoProvider = nullptr;
        MockPropertyProviderReset();
    }
};

using namespace ::testing;

#define VENDOR_NAME_VALID "Test Vendor"
#define VENDOR_ID_VALID 1234
#define PRODUCT_NAME_VALID "Test Product"
#define PRODUCT_ID_VALID 4321
#define PART_NUMBER_VALID "Test Part Number"
#define PRODUCT_URL_VALID "https://www.example.com/test-product"
#define PRODUCT_LABEL_VALID "Test Product Label"
#define SERIAL_NUMBER_VALID "Test Serial Number"
#define MANUFACTURING_DATE_VALID "2023-01-01"
#define MANUFACTURING_DATE_VALID_YEAR 2023
#define MANUFACTURING_DATE_VALID_MONTH 1
#define MANUFACTURING_DATE_VALID_DAY 1
#define HARDWARE_VERSION_VALID 1
#define HARDWARE_VERSION_STRING_VALID "Test Hardware Version 1.0"

/**
 * @brief Tests the device info provider's ability to provide valid device information when the client
 *        externally provides all valid values for vendor name, vendor ID, product name, product ID,
 *        part number, product URL, product label, serial number, and manufacturing date,
 *        hardware version, and hardware version string.
 */
TEST_F(BartonDeviceInstanceInfoProviderTest, ProvideValidDataTest)
{
    g_autoptr(BCorePropertyProvider) mockProvider = deviceServiceConfigurationGetPropertyProvider();

    b_core_property_provider_set_property_string(
        mockProvider, B_CORE_BARTON_MATTER_VENDOR_NAME, VENDOR_NAME_VALID);

    b_core_property_provider_set_property_uint16(
        mockProvider, B_CORE_BARTON_MATTER_VENDOR_ID, VENDOR_ID_VALID);

    b_core_property_provider_set_property_string(
        mockProvider, B_CORE_BARTON_MATTER_PRODUCT_NAME, PRODUCT_NAME_VALID);

    b_core_property_provider_set_property_uint16(
        mockProvider, B_CORE_BARTON_MATTER_PRODUCT_ID, PRODUCT_ID_VALID);

    b_core_property_provider_set_property_string(
        mockProvider, B_CORE_BARTON_MATTER_PART_NUMBER, PART_NUMBER_VALID);

    b_core_property_provider_set_property_string(
        mockProvider, B_CORE_BARTON_MATTER_PRODUCT_URL, PRODUCT_URL_VALID);

    b_core_property_provider_set_property_string(
        mockProvider, B_CORE_BARTON_MATTER_PRODUCT_LABEL, PRODUCT_LABEL_VALID);

    b_core_property_provider_set_property_string(
        mockProvider, B_CORE_BARTON_MATTER_SERIAL_NUMBER, SERIAL_NUMBER_VALID);

    b_core_property_provider_set_property_string(
        mockProvider, B_CORE_BARTON_MATTER_MANUFACTURING_DATE, MANUFACTURING_DATE_VALID);

    b_core_property_provider_set_property_uint16(
        mockProvider, B_CORE_BARTON_MATTER_HARDWARE_VERSION, HARDWARE_VERSION_VALID);

    b_core_property_provider_set_property_string(
        mockProvider, B_CORE_BARTON_MATTER_HARDWARE_VERSION_STRING, HARDWARE_VERSION_STRING_VALID);

    // Test that the getter APIs return the expected values.
    char vendorName[64];
    EXPECT_EQ(bartonDeviceInstanceInfoProvider->GetVendorName(vendorName, sizeof(vendorName)), CHIP_NO_ERROR);
    EXPECT_STREQ(vendorName, VENDOR_NAME_VALID);

    uint16_t vendorId;
    EXPECT_EQ(bartonDeviceInstanceInfoProvider->GetVendorId(vendorId), CHIP_NO_ERROR);
    EXPECT_EQ(vendorId, VENDOR_ID_VALID);

    char productName[64];
    EXPECT_EQ(bartonDeviceInstanceInfoProvider->GetProductName(productName, sizeof(productName)), CHIP_NO_ERROR);
    EXPECT_STREQ(productName, PRODUCT_NAME_VALID);

    uint16_t productId;
    EXPECT_EQ(bartonDeviceInstanceInfoProvider->GetProductId(productId), CHIP_NO_ERROR);
    EXPECT_EQ(productId, PRODUCT_ID_VALID);

    char partNumber[64];
    EXPECT_EQ(bartonDeviceInstanceInfoProvider->GetPartNumber(partNumber, sizeof(partNumber)), CHIP_NO_ERROR);
    EXPECT_STREQ(partNumber, PART_NUMBER_VALID);

    char productUrl[256];
    EXPECT_EQ(bartonDeviceInstanceInfoProvider->GetProductURL(productUrl, sizeof(productUrl)), CHIP_NO_ERROR);
    EXPECT_STREQ(productUrl, PRODUCT_URL_VALID);

    char productLabel[64];
    EXPECT_EQ(bartonDeviceInstanceInfoProvider->GetProductLabel(productLabel, sizeof(productLabel)), CHIP_NO_ERROR);
    EXPECT_STREQ(productLabel, PRODUCT_LABEL_VALID);

    char serialNumber[32];
    EXPECT_EQ(bartonDeviceInstanceInfoProvider->GetSerialNumber(serialNumber, sizeof(serialNumber)), CHIP_NO_ERROR);
    EXPECT_STREQ(serialNumber, SERIAL_NUMBER_VALID);

    uint16_t year;
    uint8_t month, day;
    EXPECT_EQ(bartonDeviceInstanceInfoProvider->GetManufacturingDate(year, month, day), CHIP_NO_ERROR);
    EXPECT_EQ(year, MANUFACTURING_DATE_VALID_YEAR);
    EXPECT_EQ(month, MANUFACTURING_DATE_VALID_MONTH);
    EXPECT_EQ(day, MANUFACTURING_DATE_VALID_DAY);

    uint16_t hardwareVersion;
    EXPECT_EQ(bartonDeviceInstanceInfoProvider->GetHardwareVersion(hardwareVersion), CHIP_NO_ERROR);
    EXPECT_EQ(hardwareVersion, HARDWARE_VERSION_VALID);

    char hardwareVersionString[64];
    EXPECT_EQ(bartonDeviceInstanceInfoProvider->GetHardwareVersionString(hardwareVersionString, sizeof(hardwareVersionString)),
              CHIP_NO_ERROR);
    EXPECT_STREQ(hardwareVersionString, HARDWARE_VERSION_STRING_VALID);
}

/**
 * @brief Tests the device info provider's return values when properties are not present.
 *        If a required property is not present, CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND should be returned.
 *        If an optional property is not present, CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE should be returned.
 */
TEST_F(BartonDeviceInstanceInfoProviderTest, MissingPropertiesTest)
{
    // Test required properties
    char vendorName[64];
    EXPECT_EQ(bartonDeviceInstanceInfoProvider->GetVendorName(vendorName, sizeof(vendorName)), CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND);

    uint16_t vendorId;
    EXPECT_EQ(bartonDeviceInstanceInfoProvider->GetVendorId(vendorId), CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND);

    char productName[64];
    EXPECT_EQ(bartonDeviceInstanceInfoProvider->GetProductName(productName, sizeof(productName)), CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND);

    uint16_t productId;
    EXPECT_EQ(bartonDeviceInstanceInfoProvider->GetProductId(productId), CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND);

    uint16_t hardwareVersion;
    EXPECT_EQ(bartonDeviceInstanceInfoProvider->GetHardwareVersion(hardwareVersion), CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND);

    char hardwareVersionString[64];
    EXPECT_EQ(bartonDeviceInstanceInfoProvider->GetHardwareVersionString(hardwareVersionString, sizeof(hardwareVersionString)),
              CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND);

    // Optional properties
    char partNumber[64];
    EXPECT_EQ(bartonDeviceInstanceInfoProvider->GetPartNumber(partNumber, sizeof(partNumber)), CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE);

    char productUrl[256];
    EXPECT_EQ(bartonDeviceInstanceInfoProvider->GetProductURL(productUrl, sizeof(productUrl)), CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE);

    char productLabel[64];
    EXPECT_EQ(bartonDeviceInstanceInfoProvider->GetProductLabel(productLabel, sizeof(productLabel)), CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE);

    char serialNumber[32];
    EXPECT_EQ(bartonDeviceInstanceInfoProvider->GetSerialNumber(serialNumber, sizeof(serialNumber)), CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE);

    uint16_t year;
    uint8_t month, day;
    EXPECT_EQ(bartonDeviceInstanceInfoProvider->GetManufacturingDate(year, month, day), CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE);

}

TEST_F(BartonDeviceInstanceInfoProviderTest, InvalidValuesTest)
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint16_t tmp;

    g_autoptr(BCorePropertyProvider) mockProvider = deviceServiceConfigurationGetPropertyProvider();

    b_core_property_provider_set_property_string(mockProvider, B_CORE_BARTON_MATTER_MANUFACTURING_DATE, "InvalidDate");
    EXPECT_EQ(bartonDeviceInstanceInfoProvider->GetManufacturingDate(year, month, day), CHIP_ERROR_INVALID_ARGUMENT);

    b_core_property_provider_set_property_string(mockProvider, B_CORE_BARTON_MATTER_MANUFACTURING_DATE, "2023-13-01");
    EXPECT_EQ(bartonDeviceInstanceInfoProvider->GetManufacturingDate(year, month, day), CHIP_ERROR_INVALID_ARGUMENT);

    b_core_property_provider_set_property_string(mockProvider, B_CORE_BARTON_MATTER_VENDOR_ID, "0xFFFFF");
    EXPECT_EQ(bartonDeviceInstanceInfoProvider->GetVendorId(tmp), CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND);

    b_core_property_provider_set_property_string(mockProvider, B_CORE_BARTON_MATTER_PRODUCT_ID, "0xFFFFF");
    EXPECT_EQ(bartonDeviceInstanceInfoProvider->GetProductId(tmp), CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND);

    b_core_property_provider_set_property_string(mockProvider, B_CORE_BARTON_MATTER_HARDWARE_VERSION, "0xFFFFF");
    EXPECT_EQ(bartonDeviceInstanceInfoProvider->GetHardwareVersion(tmp), CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND);
}

} // namespace barton
