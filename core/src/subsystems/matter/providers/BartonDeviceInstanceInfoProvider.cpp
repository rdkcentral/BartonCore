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

/*
 * Code adapted from: https://github.com/project-chip/connectedhomeip
 * Modified by Comcast to make all relevant properties read from Barton's
 * BCorePropertyProvider.
 * Copyright 2023 Project CHIP Authors
 * All rights reserved.
 * Licensed under the Apache License, Version 2.0 (the "License")
 */

#include "BartonDeviceInstanceInfoProvider.hpp"
#include "lib/core/CHIPError.h"
#include "platform/CHIPDeviceError.h"
#include "provider/barton-core-property-provider.h"
#include <cstdint>
#include <platform/ConfigurationManager.h>

#define LOG_TAG     "MatterProps"
#define logFmt(fmt) "(%s): " fmt, __func__

extern "C" {
#include "deviceServiceConfiguration.h"
#include "deviceServiceProperties.h"
#include <icLog/logging.h>
}

using namespace chip;
using namespace chip::DeviceLayer;

namespace barton {

bool BartonDeviceInstanceInfoProvider::ValidateProperties()
{
    bool requiredPropertiesAvailable = true;

    // Get each property through the DeviceInstanceInfoProvider interface and print them to icLog

    char vendorName[64];
    if (GetVendorName(vendorName, sizeof(vendorName)) == CHIP_NO_ERROR)
    {
        icInfo("Vendor Name: %s", vendorName);
    }
    else
    {
        icWarn("Vendor Name not available (required property)");
        requiredPropertiesAvailable = false;
    }

    uint16_t vendorId;
    if (GetVendorId(vendorId) == CHIP_NO_ERROR)
    {
        icInfo("Vendor ID: %04x", vendorId);
    }
    else
    {
        icWarn("Vendor ID not available (required property)");
        requiredPropertiesAvailable = false;
    }

    char productName[64];
    if (GetProductName(productName, sizeof(productName)) == CHIP_NO_ERROR)
    {
        icInfo("Product Name: %s", productName);
    }
    else
    {
        icWarn("Product Name not available (required property)");
        requiredPropertiesAvailable = false;
    }

    uint16_t productId;
    if (GetProductId(productId) == CHIP_NO_ERROR)
    {
        icInfo("Product ID: %04x", productId);
    }
    else
    {
        icWarn("Product ID not available (required property)");
        requiredPropertiesAvailable = false;
    }

    uint16_t hardwareVersion;
    if (GetHardwareVersion(hardwareVersion) == CHIP_NO_ERROR)
    {
        icInfo("Hardware Version: %04x", hardwareVersion);
    }
    else
    {
        icWarn("Hardware Version not available (required property)");
        requiredPropertiesAvailable = false;
    }

    char hardwareVersionString[64];
    if (GetHardwareVersionString(hardwareVersionString, sizeof(hardwareVersionString)) == CHIP_NO_ERROR)
    {
        icInfo("Hardware Version String: %s", hardwareVersionString);
    }
    else
    {
        icWarn("Hardware Version String not available (required property)");
        requiredPropertiesAvailable = false;
    }

    char partNumber[64];
    if (GetPartNumber(partNumber, sizeof(partNumber)) == CHIP_NO_ERROR)
    {
        icInfo("Part Number: %s", partNumber);
    }
    else
    {
        icInfo("Part Number not available (optional property)");
    }

    char productURL[256];
    if (GetProductURL(productURL, sizeof(productURL)) == CHIP_NO_ERROR)
    {
        icInfo("Product URL: %s", productURL);
    }
    else
    {
        icInfo("Product URL not available (optional property)");
    }

    char productLabel[64];
    if (GetProductLabel(productLabel, sizeof(productLabel)) == CHIP_NO_ERROR)
    {
        icInfo("Product Label: %s", productLabel);
    }
    else
    {
        icInfo("Product Label not available (optional property)");
    }

    char serialNumber[32];
    if (GetSerialNumber(serialNumber, sizeof(serialNumber)) == CHIP_NO_ERROR)
    {
        icInfo("Serial Number: %s", serialNumber);
    }
    else
    {
        icInfo("Serial Number not available (optional property)");
    }

    uint16_t year = 0;
    uint8_t month = 0;
    uint8_t day = 0;
    if (GetManufacturingDate(year, month, day) == CHIP_NO_ERROR)
    {
        icInfo("Manufacturing Date: %04u-%02u-%02u", year, month, day);
    }
    else
    {
        icInfo("Manufacturing Date not available (optional property)");
    }

    return requiredPropertiesAvailable;
}

CHIP_ERROR BartonDeviceInstanceInfoProvider::GetVendorName(char * buf, size_t bufSize)
{
    g_autoptr(BCorePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();

    g_autofree char * vendorName = b_core_property_provider_get_property_as_string(propertyProvider, B_CORE_BARTON_MATTER_VENDOR_NAME, NULL);
    ReturnErrorCodeIf(vendorName == NULL, CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND);

    ReturnErrorCodeIf(strlen(vendorName) >= bufSize, CHIP_ERROR_BUFFER_TOO_SMALL);
    strcpy(buf, vendorName);

    return CHIP_NO_ERROR;
}

CHIP_ERROR BartonDeviceInstanceInfoProvider::GetVendorId(uint16_t & vendorId)
{
    g_autoptr(BCorePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();

    guint64 val = b_core_property_provider_get_property_as_uint64(
        propertyProvider, B_CORE_BARTON_MATTER_VENDOR_ID, G_MAXUINT64);

    if (val > G_MAXUINT16)
    {
        return CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND;
    }

    vendorId = static_cast<uint16_t>(val);
    return CHIP_NO_ERROR;
}

CHIP_ERROR BartonDeviceInstanceInfoProvider::GetProductName(char *buf, size_t bufSize)
{
    g_autoptr(BCorePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();

    g_autofree char *productName =
        b_core_property_provider_get_property_as_string(propertyProvider, B_CORE_BARTON_MATTER_PRODUCT_NAME, NULL);
    ReturnErrorCodeIf(productName == NULL, CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND);

    ReturnErrorCodeIf(strlen(productName) >= bufSize, CHIP_ERROR_BUFFER_TOO_SMALL);
    strcpy(buf, productName);

    return CHIP_NO_ERROR;
}

CHIP_ERROR BartonDeviceInstanceInfoProvider::GetProductId(uint16_t & productId)
{
    g_autoptr(BCorePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();

    guint64 val = b_core_property_provider_get_property_as_uint64(
        propertyProvider, B_CORE_BARTON_MATTER_PRODUCT_ID, G_MAXUINT64);

    if (val > G_MAXUINT16)
    {
        return CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND;
    }

    productId = static_cast<uint16_t>(val);
    return CHIP_NO_ERROR;
}

CHIP_ERROR BartonDeviceInstanceInfoProvider::GetPartNumber(char *buf, size_t bufSize)
{
    g_autoptr(BCorePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();

    g_autofree char *partNumber =
        b_core_property_provider_get_property_as_string(propertyProvider, B_CORE_BARTON_MATTER_PART_NUMBER, NULL);
    ReturnErrorCodeIf(partNumber == NULL, CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE);

    ReturnErrorCodeIf(strlen(partNumber) >= bufSize, CHIP_ERROR_BUFFER_TOO_SMALL);
    strcpy(buf, partNumber);

    return CHIP_NO_ERROR;
}

CHIP_ERROR BartonDeviceInstanceInfoProvider::GetProductURL(char *buf, size_t bufSize)
{
    g_autoptr(BCorePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();

    g_autofree char *productURL =
        b_core_property_provider_get_property_as_string(propertyProvider, B_CORE_BARTON_MATTER_PRODUCT_URL, NULL);
    ReturnErrorCodeIf(productURL == NULL, CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE);

    ReturnErrorCodeIf(strlen(productURL) >= bufSize, CHIP_ERROR_BUFFER_TOO_SMALL);
    strcpy(buf, productURL);

    return CHIP_NO_ERROR;
}

CHIP_ERROR BartonDeviceInstanceInfoProvider::GetProductLabel(char *buf, size_t bufSize)
{
    g_autoptr(BCorePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();

    g_autofree char *productLabel =
        b_core_property_provider_get_property_as_string(propertyProvider, B_CORE_BARTON_MATTER_PRODUCT_LABEL, NULL);
    ReturnErrorCodeIf(productLabel == NULL, CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE);

    ReturnErrorCodeIf(strlen(productLabel) >= bufSize, CHIP_ERROR_BUFFER_TOO_SMALL);
    strcpy(buf, productLabel);

    return CHIP_NO_ERROR;
}

CHIP_ERROR BartonDeviceInstanceInfoProvider::GetSerialNumber(char *buf, size_t bufSize)
{
    g_autoptr(BCorePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();

    g_autofree char *serialNumber =
        b_core_property_provider_get_property_as_string(propertyProvider, B_CORE_BARTON_MATTER_SERIAL_NUMBER, NULL);
    ReturnErrorCodeIf(serialNumber == NULL, CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE);

    ReturnErrorCodeIf(strlen(serialNumber) >= bufSize, CHIP_ERROR_BUFFER_TOO_SMALL);
    strcpy(buf, serialNumber);

    return CHIP_NO_ERROR;
}

CHIP_ERROR BartonDeviceInstanceInfoProvider::GetManufacturingDate(uint16_t &year, uint8_t &month, uint8_t &day)
{
    g_autoptr(BCorePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();

    g_autofree char *dateStr = b_core_property_provider_get_property_as_string(
        propertyProvider, B_CORE_BARTON_MATTER_MANUFACTURING_DATE, NULL);
    ReturnErrorCodeIf(dateStr == NULL, CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE);

    // Expecting date in "YYYY-MM-DD" format
    g_autoptr(GDate) date = g_date_new();
    g_date_set_parse(date, dateStr);
    if (!g_date_valid(date))
    {
        return CHIP_ERROR_INVALID_ARGUMENT;
    }

    year = g_date_get_year(date);
    month = g_date_get_month(date);
    day = g_date_get_day(date);

    return CHIP_NO_ERROR;
}

CHIP_ERROR BartonDeviceInstanceInfoProvider::GetHardwareVersion(uint16_t &hardwareVersion)
{
    g_autoptr(BCorePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();

    guint64 val =
        b_core_property_provider_get_property_as_uint64(propertyProvider, B_CORE_BARTON_MATTER_HARDWARE_VERSION, G_MAXUINT64);

    if (val > G_MAXUINT16)
    {
        return CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND;
    }

    hardwareVersion = static_cast<uint16_t>(val);

    return CHIP_NO_ERROR;
}

CHIP_ERROR BartonDeviceInstanceInfoProvider::GetHardwareVersionString(char *buf, size_t bufSize)
{
    g_autoptr(BCorePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();

    g_autofree char *hwVersionStr = b_core_property_provider_get_property_as_string(
        propertyProvider, B_CORE_BARTON_MATTER_HARDWARE_VERSION_STRING, NULL);
    ReturnErrorCodeIf(hwVersionStr == NULL, CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND);

    ReturnErrorCodeIf(strlen(hwVersionStr) >= bufSize, CHIP_ERROR_BUFFER_TOO_SMALL);
    strcpy(buf, hwVersionStr);

    return CHIP_NO_ERROR;
}

CHIP_ERROR BartonDeviceInstanceInfoProvider::GetRotatingDeviceIdUniqueId(MutableByteSpan & uniqueIdSpan)
{
#if CHIP_ENABLE_ROTATING_DEVICE_ID && defined(CHIP_DEVICE_CONFIG_ROTATING_DEVICE_ID_UNIQUE_ID)
    if (chip::DeviceLayer::ConfigurationMgr().GetRotatingDeviceIdUniqueId(uniqueIdSpan) != CHIP_NO_ERROR)
    {
        static_assert(ConfigurationManager::kRotatingDeviceIDUniqueIDLength >=
                          ConfigurationManager::kMinRotatingDeviceIDUniqueIDLength,
                      "Length of unique ID for rotating device ID is smaller than minimum.");

        constexpr uint8_t uniqueId[] = CHIP_DEVICE_CONFIG_ROTATING_DEVICE_ID_UNIQUE_ID;

        ReturnErrorCodeIf(sizeof(uniqueId) > uniqueIdSpan.size(), CHIP_ERROR_BUFFER_TOO_SMALL);
        ReturnErrorCodeIf(sizeof(uniqueId) != ConfigurationManager::kRotatingDeviceIDUniqueIDLength, CHIP_ERROR_BUFFER_TOO_SMALL);
        memcpy(uniqueIdSpan.data(), uniqueId, sizeof(uniqueId));
        uniqueIdSpan.reduce_size(sizeof(uniqueId));
    }
    return CHIP_NO_ERROR;
#else
    icError("Rotating Device ID is not enabled or unique ID is not defined.");
    return CHIP_ERROR_WRONG_KEY_TYPE;
#endif
}

} // namespace barton
