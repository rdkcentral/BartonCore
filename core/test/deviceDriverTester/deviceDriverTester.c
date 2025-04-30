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

/*-----------------------------------------------
 * deviceDriverTester.c
 *
 * Tests the device driver callbacks
 *       - *only supports cameras currently*
 *
 * Author: jgleason
 *-----------------------------------------------*/
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "deviceDriver.h"
#include <commonDeviceDefs.h>
#include <device/icDevice.h>
#include <deviceDescriptors.h>
#include <deviceDriver.h>
#include <deviceModelHelper.h>
#include <icLog/logging.h>
#include <inttypes.h>
#include <ipc/deviceEventProducer.h>
#include <openHomeCamera/openHomeCameraDeviceDriver.h>
#include <ssdp/ssdp.h>


/* Define the nodes in XML responses */
#define ROOT_NODE          "root"
#define DEVICE_NODE        "device"
#define FRIENDLY_NAME_NODE "friendlyName"
#define MANUFACTURER_NODE  "manufacturer"
#define MODEL_NAME_NODE    "modelName"
#define MODEL_NUMBER_NODE  "modelNumber"
#define UUID_NODE          "UDN"

#define TWENTY_SECONDS     20 * 1000 * 1000

void deviceFoundCallback(const DeviceDriver *deviceDriver,
                         const char *deviceClass,
                         uint8_t deviceClassVersion,
                         const char *deviceUuid,
                         const char *manufacturer,
                         const char *model,
                         const char *hardwareVersion,
                         const char *firmwareVersion);
void deviceConfiguredCallback(icDevice *device);
icDeviceResource *getResourceCallback(const char *deviceUuid, uint32_t endpointNumber, const char *resourceId);

bool doCameraConfigure = false;
static uint64_t getCurrentGmtTimeMillis();

/*
 * Usage function
 */
static void printUsage()
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  deviceDriverTester <-s|-c|-r>\n");
    fprintf(stderr, "    -s : perform an SSDP Scan looking for cameras\n");
    fprintf(stderr, "    -c : configure the camera (must be discovered first)\n");
    fprintf(stderr, "    -r : reboot the camera after discovering it (must be discovered first)\n");
    fprintf(stderr, "\n");
}

/*
 * Main - CAMERA BOOTSTRAP TEST
 *      1. Camera Discovery using SSDP scan
 *      2. Get Camera Capabilities
 *      3. Configure Camera (TODO)
 *      4. Reboot camera
 *      5. Upgrade Camera (TODO)
 */
int main(int argc, char *argv[])
{
    int retVal = 1;
    int c;
    bool doCameraScan = false;
    bool doCameraReboot = false;

    while ((c = getopt(argc, argv, "scrH")) != -1)
    {
        switch (c)
        {
            case 's':
                doCameraScan = true;
                break;

            case 'c':
                doCameraConfigure = true;
                break;

            case 'r':
                doCameraReboot = true;
                break;

            case 'H':
            default:
            {
                printUsage();
                return retVal;
            }
        }
    }

    /* Initialize the Camera Device Driver & Callbacks */
    DeviceServiceCallbacks deviceServiceInterface;
    memset(&deviceServiceInterface, 0, sizeof(DeviceServiceCallbacks));
    deviceServiceInterface.deviceFound = deviceFoundCallback;
    deviceServiceInterface.deviceConfigured = deviceConfiguredCallback;
    deviceServiceInterface.getResource = getResourceCallback;
    DeviceDriver *driver = openHomeCameraDeviceDriverInitialize(&deviceServiceInterface);

    if (doCameraScan == true)
    {
        printf("\n\nDevice Driver Discover Devices (Cameras)...\n\n");

        bool started = driver->discoverDevices(NULL, CAMERA_DC);
        if (started == false)
        {
            printf("\nFailed to start device discovery.\n");
        }
        else
        {
            printf("\nDevice discovery was successfully started.\n");
            retVal = 0;
        }

        // let it run for 4 minutes
        // while(1)
        for (int i = 0; i < 12; i++)
        {
            usleep(TWENTY_SECONDS);
        }
    }

    /* no errors? */
    return retVal;
}


/*
 * Callback invoked when a device driver finds a device.
 */
void deviceFoundCallback(const DeviceDriver *deviceDriver,
                         const char *deviceClass,
                         uint8_t deviceClassVersion,
                         const char *deviceUuid,
                         const char *manufacturer,
                         const char *model,
                         const char *hardwareVersion,
                         const char *firmwareVersion)
{

    // Create a device instance populated with all required items from the base device class specification
    icDevice *device = createDevice(deviceUuid, deviceClass, deviceClassVersion, deviceDriver->driverName);
    createDeviceResource(
        device, COMMON_DEVICE_RESOURCE_MANUFACTURER, manufacturer, "string", false, CACHING_POLICY_ALWAYS);
    createDeviceResource(device, COMMON_DEVICE_RESOURCE_MODEL, model, "string", false, CACHING_POLICY_ALWAYS);
    createDeviceResource(
        device, COMMON_DEVICE_RESOURCE_HARDWARE_VERSION, hardwareVersion, "string", false, CACHING_POLICY_ALWAYS);
    createDeviceResource(device,
                         COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION,
                         firmwareVersion,
                         "string",
                         false,
                         CACHING_POLICY_ALWAYS); // the device driver will update after firmware upgrade

    char dateBuf[14]; // timestamps in millis are 13 digits plus \0: 1442408555212
    sprintf(dateBuf, "%" PRIu64, getCurrentGmtTimeMillis());
    createDeviceResource(device, COMMON_DEVICE_RESOURCE_DATE_ADDED, dateBuf, "integer", false, CACHING_POLICY_ALWAYS);
    createDeviceResource(
        device, COMMON_DEVICE_RESOURCE_DATE_LAST_CONTACTED, dateBuf, "integer", false, CACHING_POLICY_ALWAYS);
    createDeviceResource(
        device, COMMON_DEVICE_RESOURCE_COMM_FAIL_TROUBLE, "false", "boolean", false, CACHING_POLICY_ALWAYS);
    createDeviceResource(device, COMMON_DEVICE_FUNCTION_RESET_TO_FACTORY, NULL, "function", true, CACHING_POLICY_NEVER);

    DeviceDescriptor *dd = deviceDescriptorsGet(manufacturer, model, hardwareVersion, firmwareVersion);

    if (deviceDriver->configureDevice(device, dd) == false)
    {
        printf("%s failed to configure\n", deviceUuid);
        deviceDestroy(device);
    }

    if (doCameraConfigure == true)
    {
        bool configFlag = deviceDriver->configureDevice(device, NULL);
    }
}

/*
 * Callback invoked when the device is configured
 */
void deviceConfiguredCallback(icDevice *device)
{
    printf("The device %s was configured successfully\n", device->uuid);
}

static uint64_t getCurrentGmtTimeMillis()
{
    uint64_t result;

#ifdef CONFIG_OS_DARWIN
    // do this the Mac way
    //
    struct timeval now;
    gettimeofday(&now, NULL);
    result = (uint64_t) ((uint64_t) now.tv_sec * 1000 + ((uint64_t) now.tv_usec / 1000));
#else
    // standard linux, use real (not monotonic) clock
    //
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    result = (uint64_t) ((uint64_t) now.tv_sec * 1000 + ((uint64_t) now.tv_nsec / 1000000));
#endif

    return result;
}

icDeviceResource *getResourceCallback(const char *deviceUuid, uint32_t endpointNumber, const char *resourceId)
{
    return NULL;
}
