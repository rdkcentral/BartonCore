//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2018 Comcast
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
// iControl Networks retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------

//
// Created by Micah Koch on 5/02/18.
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
#include <errno.h>
#include <icLog/logging.h>
#include <icUtil/fileUtils.h>
#include <resourceTypes.h>
#include <stdio.h>
#include <string.h>
#include <subsystems/zigbee/zigbeeSubsystem.h>
#include <sys/stat.h>
#include <zhal/zhal.h>


#define LOG_TAG                 "zigbeeSubsystemTest"
#define DUMMY_OTA_FIRMWARE_FILE "dummy.ota"
#define LEGACY_FIRMWARE_FILE    "dummy.ebl"

static int counter = 0;
static char templateTempDir[255] = "/tmp/testDirXXXXXX";
static char *dynamicDir;

icLinkedList *__wrap_deviceServiceGetDevicesBySubsystem(const char *subsystem);
DeviceDescriptor *__wrap_deviceServiceGetDeviceDescriptorForDevice(icDevice *device);
gchar *__wrap_deviceServiceConfigurationGetFirmwareFileDir();
static icDevice *createDummyDevice(const char *manufacturer,
                                   const char *model,
                                   const char *hardwareVersion,
                                   const char *firmwareVersion);
static DeviceDescriptor *createDeviceDescriptor(const char *manufacturer,
                                                const char *model,
                                                const char *harwareVersion,
                                                const char *firmareVersion);
static void createDummyFirmwareFile(DeviceFirmwareType);
static char *getDummyFirmwareFilePath(DeviceFirmwareType firmwareType);

// ******************************
// Tests
// ******************************

static void test_zigbeeSubsystemCleanupFirmwareFiles(void **state)
{
    // Setup dummy device and cooresponding device descriptor
    icLinkedList *devices = linkedListCreate();
    linkedListAppend(devices, createDummyDevice("dummy", "dummy", "1", "0x00000001"));
    will_return(__wrap_deviceServiceGetDevicesBySubsystem, devices);
    DeviceDescriptor *dd = createDeviceDescriptor("dummy", "dummy", "1", "0x00000001");
    will_return(__wrap_deviceServiceGetDeviceDescriptorForDevice, dd);
    // Create some dummy firmware files to remove
    createDummyFirmwareFile(DEVICE_FIRMWARE_TYPE_ZIGBEE_OTA);
    createDummyFirmwareFile(DEVICE_FIRMWARE_TYPE_ZIGBEE_LEGACY);

    // Make the call
    zigbeeSubsystemCleanupFirmwareFiles();

    // Check if the files are gone like expected
    char *otaPath = getDummyFirmwareFilePath(DEVICE_FIRMWARE_TYPE_ZIGBEE_OTA);
    struct stat statBuf;
    assert_int_not_equal(stat(otaPath, &statBuf), 0);
    assert_int_equal(errno, ENOENT);
    free(otaPath);

    char *legacyPath = getDummyFirmwareFilePath(DEVICE_FIRMWARE_TYPE_ZIGBEE_OTA);
    assert_int_not_equal(stat(legacyPath, &statBuf), 0);
    assert_int_equal(errno, ENOENT);
    free(legacyPath);

    (void) state;
}

static void test_zigbeeSubsystemCleanupFirmwareFilesDoNothingIfFirmwareNeeded(void **state)
{
    // Setup dummy device and cooresponding device descriptor
    icLinkedList *devices = linkedListCreate();
    linkedListAppend(devices, createDummyDevice("dummy", "dummy", "1", "0x00000001"));
    will_return(__wrap_deviceServiceGetDevicesBySubsystem, devices);
    DeviceDescriptor *dd = createDeviceDescriptor("dummy", "dummy", "1", "0x00000001");
    // Add a newer version as latest with the dummy firmware file
    dd->latestFirmware = (DeviceFirmware *) calloc(1, sizeof(DeviceFirmware));
    dd->latestFirmware->type = DEVICE_FIRMWARE_TYPE_ZIGBEE_OTA;
    dd->latestFirmware->version = strdup("0x00000002");
    dd->latestFirmware->fileInfos = linkedListCreate();
    DeviceFirmwareFileInfo *info = (DeviceFirmwareFileInfo *) calloc(1, sizeof(DeviceFirmwareFileInfo));
    info->fileName = strdup(DUMMY_OTA_FIRMWARE_FILE);
    info->checksum = strdup("F5571D40A3D5F5B4A7901E2C58DCC0B2");
    linkedListAppend(dd->latestFirmware->fileInfos, info);
    // Add the firmware file
    will_return(__wrap_deviceServiceGetDeviceDescriptorForDevice, dd);
    // Create some dummy firmware files
    createDummyFirmwareFile(DEVICE_FIRMWARE_TYPE_ZIGBEE_OTA);

    // Make the call
    zigbeeSubsystemCleanupFirmwareFiles();

    // Check if the files are still there
    char *otaPath = getDummyFirmwareFilePath(DEVICE_FIRMWARE_TYPE_ZIGBEE_OTA);
    struct stat statBuf;
    assert_return_code(stat(otaPath, &statBuf), errno);
    free(otaPath);

    (void) state;
}

static void
addDummyDiscoveredClusterDetails(IcDiscoveredClusterDetails *clusterDetails, uint16_t clusterId, bool isServer)
{
    if (clusterDetails)
    {
        clusterDetails->isServer = isServer;
        clusterDetails->clusterId = clusterId;
        clusterDetails->numAttributeIds = 1;
        clusterDetails->attributeIds = (uint16_t *) calloc(1, sizeof(uint16_t));
        clusterDetails->attributeIds[0] = 3;
    }
}

static void addDummyDiscoveredEndpointDetails(IcDiscoveredEndpointDetails *endpointDetails)
{
    static uint8_t endpointId = 1;

    if (endpointDetails)
    {
        endpointDetails->endpointId = endpointId++;
        endpointDetails->appDeviceId = 4;
        endpointDetails->appDeviceVersion = 3;
        endpointDetails->appProfileId = 7;

        endpointDetails->numServerClusterDetails = 1;
        endpointDetails->serverClusterDetails =
            (IcDiscoveredClusterDetails *) calloc(1, sizeof(IcDiscoveredClusterDetails));
        addDummyDiscoveredClusterDetails(endpointDetails->serverClusterDetails, 0x0b05, true);

        endpointDetails->numClientClusterDetails = 1;
        endpointDetails->clientClusterDetails =
            (IcDiscoveredClusterDetails *) calloc(1, sizeof(IcDiscoveredClusterDetails));
        addDummyDiscoveredClusterDetails(endpointDetails->clientClusterDetails, 2, false);
    }
}

static IcDiscoveredDeviceDetails *createDummyDiscoveredDeviceDetails(uint32_t endpointCount)
{
    IcDiscoveredDeviceDetails *details = (IcDiscoveredDeviceDetails *) calloc(1, sizeof(IcDiscoveredDeviceDetails));

    details->eui64 = 0x1234567887654321;
    details->manufacturer = strdup("acme");
    details->model = strdup("rocket");
    details->hardwareVersion = 0x722222222222;
    details->firmwareVersion = 0x333333333333;
    details->appVersion = 43;
    details->powerSource = powerSourceBattery;
    details->deviceType = deviceTypeEndDevice;

    details->numEndpoints = endpointCount;
    details->endpointDetails =
        (IcDiscoveredEndpointDetails *) calloc(endpointCount, sizeof(IcDiscoveredEndpointDetails));
    for (uint32_t index = 0; index < endpointCount; index++)
    {
        addDummyDiscoveredEndpointDetails(&details->endpointDetails[index]);
    }

    return details;
}

static void test_encodeDecodeIcDiscoveredDeviceDetails(void **state)
{
    IcDiscoveredDeviceDetails *details = createDummyDiscoveredDeviceDetails(1);
    cJSON *detailsJson = icDiscoveredDeviceDetailsToJson(details);

    IcDiscoveredDeviceDetails *details2 = icDiscoveredDeviceDetailsFromJson(detailsJson);

    // now see if details1 and details2 are equal!
    assert_int_equal(details->eui64, details2->eui64);
    assert_string_equal(details->manufacturer, details2->manufacturer);
    assert_string_equal(details->model, details2->model);
    assert_int_equal(details->hardwareVersion, details2->hardwareVersion);
    assert_int_equal(details->firmwareVersion, details2->firmwareVersion);
    assert_int_equal(details->appVersion, details2->appVersion);
    assert_int_equal(details->powerSource, details2->powerSource);
    assert_int_equal(details->deviceType, details2->deviceType);
    assert_int_equal(details->numEndpoints, details2->numEndpoints);

    assert_int_equal(details->endpointDetails[0].endpointId, details2->endpointDetails[0].endpointId);
    assert_int_equal(details->endpointDetails[0].appProfileId, details2->endpointDetails[0].appProfileId);
    assert_int_equal(details->endpointDetails[0].appDeviceVersion, details2->endpointDetails[0].appDeviceVersion);
    assert_int_equal(details->endpointDetails[0].appDeviceId, details2->endpointDetails[0].appDeviceId);
    assert_int_equal(details->endpointDetails[0].numServerClusterDetails,
                     details2->endpointDetails[0].numServerClusterDetails);
    assert_int_equal(details->endpointDetails[0].numClientClusterDetails,
                     details2->endpointDetails[0].numClientClusterDetails);

    assert_int_equal(details->endpointDetails[0].serverClusterDetails[0].clusterId,
                     details2->endpointDetails[0].serverClusterDetails[0].clusterId);
    assert_int_equal(details->endpointDetails[0].serverClusterDetails[0].isServer,
                     details2->endpointDetails[0].serverClusterDetails[0].isServer);
    assert_int_equal(details->endpointDetails[0].serverClusterDetails[0].attributeIds[0],
                     details2->endpointDetails[0].serverClusterDetails[0].attributeIds[0]);

    assert_int_equal(details->endpointDetails[0].clientClusterDetails[0].clusterId,
                     details2->endpointDetails[0].clientClusterDetails[0].clusterId);
    assert_int_equal(details->endpointDetails[0].clientClusterDetails[0].isServer,
                     details2->endpointDetails[0].clientClusterDetails[0].isServer);
    assert_int_equal(details->endpointDetails[0].clientClusterDetails[0].attributeIds[0],
                     details2->endpointDetails[0].clientClusterDetails[0].attributeIds[0]);

    freeIcDiscoveredDeviceDetails(details);
    freeIcDiscoveredDeviceDetails(details2);
    cJSON_Delete(detailsJson);
}

static void test_icDiscoveredDeviceDetailsGetClusterEndpoint(void **state)
{
    IcDiscoveredDeviceDetails *details = createDummyDiscoveredDeviceDetails(2);
    uint8_t endpointId = 0;
    // look for diagnostic cluster, since both endpoints has this cluster,
    // the result should be first endpoint
    icDiscoveredDeviceDetailsGetClusterEndpoint(details, 0x0b05, &endpointId);
    // assert that the found endpoint ID matches the endpoint ID of first endpoint
    assert_int_equal(endpointId, details->endpointDetails[0].endpointId);
    freeIcDiscoveredDeviceDetails(details);
}

static void test_icDiscoveredDeviceDetailsGetAttributeEndpoint(void **state)
{
    IcDiscoveredDeviceDetails *details = createDummyDiscoveredDeviceDetails(2);
    uint8_t endpointId = 0;
    // look for a attribute in diagnostic cluster
    // since both endpoints has this cluster, the result should be
    // first endpoint
    icDiscoveredDeviceDetailsGetAttributeEndpoint(details, 0x0b05, 3, &endpointId);
    // assert that the found endpoint ID matches the endpoint ID of first endpoint
    assert_int_equal(endpointId, details->endpointDetails[0].endpointId);
    freeIcDiscoveredDeviceDetails(details);
}

// ******************************
// Setup/Teardown
// ******************************

static int dynamicDirSetup(void **state)
{
    dynamicDir = mkdtemp(templateTempDir);

    assert_non_null(dynamicDir);

    (void) state;

    return 0;
}

static int dynamicDirTeardown(void **state)
{
    if (dynamicDir != NULL)
    {
        deleteDirectory(dynamicDir);
        dynamicDir = NULL;
    }

    (void) state;

    return 0;
}

// ******************************
// Helpers
// ******************************

static icDevice *
createDummyDevice(const char *manufacturer, const char *model, const char *hardwareVersion, const char *firmwareVersion)
{
    ++counter;
    char uuid[255];
    sprintf(uuid, "device%d", counter);
    icDevice *device = createDevice(uuid, "dummy", 1, "dummy", NULL);
    createDeviceResource(device,
                         COMMON_DEVICE_RESOURCE_MANUFACTURER,
                         manufacturer,
                         RESOURCE_TYPE_STRING,
                         RESOURCE_MODE_READABLE,
                         CACHING_POLICY_ALWAYS);
    createDeviceResource(device,
                         COMMON_DEVICE_RESOURCE_MODEL,
                         model,
                         RESOURCE_TYPE_STRING,
                         RESOURCE_MODE_READABLE,
                         CACHING_POLICY_ALWAYS);
    createDeviceResource(device,
                         COMMON_DEVICE_RESOURCE_HARDWARE_VERSION,
                         hardwareVersion,
                         RESOURCE_TYPE_VERSION,
                         RESOURCE_MODE_READABLE,
                         CACHING_POLICY_ALWAYS);
    createDeviceResource(device,
                         COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION,
                         firmwareVersion,
                         RESOURCE_TYPE_VERSION,
                         RESOURCE_MODE_READABLE,
                         CACHING_POLICY_ALWAYS);
    return device;
}

static DeviceDescriptor *createDeviceDescriptor(const char *manufacturer,
                                                const char *model,
                                                const char *hardwareVersion,
                                                const char *firmwareVersion)
{
    ++counter;
    char uuid[255];
    sprintf(uuid, "dd%d", counter);
    DeviceDescriptor *dd = (DeviceDescriptor *) calloc(1, sizeof(DeviceDescriptor));
    dd->uuid = strdup(uuid);
    dd->model = strdup(model);
    dd->manufacturer = strdup(manufacturer);
    dd->firmwareVersions = (DeviceVersionList *) calloc(1, sizeof(DeviceVersionList));
    dd->firmwareVersions->listType = DEVICE_VERSION_LIST_TYPE_LIST;
    dd->firmwareVersions->list.versionList = linkedListCreate();
    linkedListAppend(dd->firmwareVersions->list.versionList, strdup(firmwareVersion));
    dd->hardwareVersions = (DeviceVersionList *) calloc(1, sizeof(DeviceVersionList));
    dd->hardwareVersions->listType = DEVICE_VERSION_LIST_TYPE_LIST;
    dd->hardwareVersions->list.versionList = linkedListCreate();
    linkedListAppend(dd->hardwareVersions->list.versionList, strdup(hardwareVersion));

    return dd;
}

static char *getDummyFirmwareFilePath(DeviceFirmwareType firmwareType)
{
    char pathBuf[255];
    char *dir = zigbeeSubsystemGetAndCreateFirmwareFileDirectory(firmwareType);
    if (firmwareType == DEVICE_FIRMWARE_TYPE_ZIGBEE_OTA)
    {
        sprintf(pathBuf, "%s/" DUMMY_OTA_FIRMWARE_FILE, dir);
    }
    else if (firmwareType == DEVICE_FIRMWARE_TYPE_ZIGBEE_LEGACY)
    {
        sprintf(pathBuf, "%s/" LEGACY_FIRMWARE_FILE, dir);
    }
    else
    {
        // Failure
        assert_false(true);
    }
    free(dir);

    return strdup(pathBuf);
}

static void createDummyFirmwareFile(DeviceFirmwareType firmwareType)
{
    assert_non_null(dynamicDir);

    scoped_generic char *path = getDummyFirmwareFilePath(firmwareType);
    createMarkerFile(path);
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

DeviceDescriptor *__wrap_deviceServiceGetDeviceDescriptorForDevice(icDevice *device)
{
    icLogDebug(LOG_TAG, "%s: device UUID=%s", __FUNCTION__, device->uuid);

    DeviceDescriptor *result = (DeviceDescriptor *) mock();
    return result;
}

gchar *__wrap_deviceServiceConfigurationGetFirmwareFileDir()
{
    assert_non_null(dynamicDir);

    icLogDebug(LOG_TAG, "%s = %s", __FUNCTION__, dynamicDir);

    return strdup(dynamicDir);
}

int main(int argc, const char **argv)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_zigbeeSubsystemCleanupFirmwareFiles),
        cmocka_unit_test(test_zigbeeSubsystemCleanupFirmwareFilesDoNothingIfFirmwareNeeded),
        cmocka_unit_test(test_encodeDecodeIcDiscoveredDeviceDetails),
        cmocka_unit_test(test_icDiscoveredDeviceDetailsGetClusterEndpoint),
        cmocka_unit_test(test_icDiscoveredDeviceDetailsGetAttributeEndpoint)};

    int retval = cmocka_run_group_tests(tests, dynamicDirSetup, dynamicDirTeardown);

    return retval;
}
