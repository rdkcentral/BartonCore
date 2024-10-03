//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2015 iControl Networks, Inc.
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

/*
 * This is a simple device driver that supports the philips hue lights
 *
 * Created by Thomas Lea on 7/28/15.
 */

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/deviceModelHelper.h"
#include <commonDeviceDefs.h>
#include <deviceDriver.h>
#include <icLog/logging.h>
#include <icUtil/macAddrUtils.h>
#include <inttypes.h>
#include <philipsHue/philipsHue.h>
#include <resourceTypes.h>

#define LOG_TAG             "PHueDD"
#define DEVICE_DRIVER_NAME  "PHueDD"
#define DEVICE_CLASS_NAME   "light"
#define DEVICE_PROFILE_NAME "light"

#define MANUFACTURER        "Philips"
#define MODEL               "PhilipsHue"

#define USERNAME_RESOURCE   "username"

#ifdef BARTON_CONFIG_PHILIPS_HUE

static DeviceServiceCallbacks *deviceServiceCallbacks = NULL;

static DeviceDriver *deviceDriver = NULL;

static bool discoverStart(void *ctx, const char *deviceClass);

static void discoverStop(void *ctx, const char *deviceClass);

static bool configureDevice(void *ctx, icDevice *device, DeviceDescriptor *descriptor);

static bool readResource(void *ctx, icDeviceResource *resource, char **value);

static bool writeResource(void *ctx, icDeviceResource *resource, const char *previousValue, const char *newValue);

static void startMonitoringBridge(const char *macAddress, const char *ipAddress, const char *username);
static void stopMonitoringBridge(const char *uuid);

static void startup(void *ctx);
static void shutdown(void *ctx);
static void deviceRemoved(void *ctx, icDevice *device);
static PhilipsHueLight *getLight(const char *deviceUuid, const char *endpointId);

typedef struct
{
    char *username;
    char *ipAddress;
    char *macAddress;
} PendingBridge;

static pthread_mutex_t pendingBridgeMutex = PTHREAD_MUTEX_INITIALIZER;
icHashMap *pendingBridges = NULL;

static pthread_mutex_t monitoringDevicesMutex = PTHREAD_MUTEX_INITIALIZER;
icHashMap *monitoringDevices = NULL;

// zigbee device driver registration order matters, so we pick constructor priority carefully
__attribute__((constructor(220))) static void driverRegister(void)
{
    icLogDebug(LOG_TAG, "philipsHueDeviceDriverInitialize");

    deviceDriver = (DeviceDriver *) calloc(1, sizeof(DeviceDriver));
    if (deviceDriver != NULL)
    {
        deviceDriver->driverName = strdup(DEVICE_DRIVER_NAME);
        deviceDriver->startup = startup;
        deviceDriver->shutdown = shutdown;
        deviceDriver->discoverDevices = discoverStart;
        deviceDriver->stopDiscoveringDevices = discoverStop;
        deviceDriver->configureDevice = configureDevice;
        deviceDriver->readResource = readResource;
        deviceDriver->writeResource = writeResource;
        deviceDriver->deviceRemoved = deviceRemoved;

        deviceDriver->supportedDeviceClasses = linkedListCreate();
        linkedListAppend(deviceDriver->supportedDeviceClasses, strdup(DEVICE_CLASS_NAME));

        deviceServiceCallbacks = deviceService;
    }
    else
    {
        icLogError(LOG_TAG, "failed to allocate DeviceDriver!");
    }

    deviceDriverManagerRegisterDriver(deviceDriver);
}

// Start monitoring the already configured bridges
static void startup(void *ctx)
{
    pthread_mutex_lock(&monitoringDevicesMutex);
    monitoringDevices = hashMapCreate();
    pthread_mutex_unlock(&monitoringDevicesMutex);

    icLinkedList *devices = deviceServiceCallbacks->getDevicesByDeviceDriver(DEVICE_DRIVER_NAME);
    icLinkedListIterator *iterator = linkedListIteratorCreate(devices);
    while (linkedListIteratorHasNext(iterator))
    {
        icDevice *device = (icDevice *) linkedListIteratorGetNext(iterator);
        icDeviceResource *macAddressResource =
            deviceServiceCallbacks->getResource(device->uuid, 0, COMMON_DEVICE_RESOURCE_MAC_ADDRESS);
        icDeviceResource *ipAddressResource =
            deviceServiceCallbacks->getResource(device->uuid, 0, COMMON_DEVICE_RESOURCE_IP_ADDRESS);
        icDeviceResource *usernameResource = deviceServiceCallbacks->getResource(device->uuid, 0, USERNAME_RESOURCE);

        startMonitoringBridge(macAddressResource->value, ipAddressResource->value, usernameResource->value);
    }
    linkedListIteratorDestroy(iterator);
    linkedListDestroy(devices, (linkedListItemFreeFunc) deviceDestroy);
}

static void shutdown(void *ctx)
{
    icLogDebug(LOG_TAG, "shutdown");

    icLinkedList *devices = deviceServiceCallbacks->getDevicesByDeviceDriver(DEVICE_DRIVER_NAME);
    icLinkedListIterator *iterator = linkedListIteratorCreate(devices);
    while (linkedListIteratorHasNext(iterator))
    {
        icDevice *device = (icDevice *) linkedListIteratorGetNext(iterator);
        stopMonitoringBridge(device->uuid);
    }
    linkedListIteratorDestroy(iterator);
    linkedListDestroy(devices, (linkedListItemFreeFunc) deviceDestroy);

    pthread_mutex_lock(&monitoringDevicesMutex);
    hashMapDestroy(monitoringDevices, NULL);
    monitoringDevices = NULL;
    pthread_mutex_unlock(&monitoringDevicesMutex);

    if (deviceDriver != NULL)
    {
        free(deviceDriver->driverName);
        linkedListDestroy(deviceDriver->supportedDeviceClasses, NULL);
        free(deviceDriver);
        deviceDriver = NULL;
    }
    deviceServiceCallbacks = NULL;
}

static void bridgeFoundCallback(const char *macAddress, const char *ipAddress, const char *username)
{
    icLogDebug(LOG_TAG, "bridge found: %s, %s: user: %s\n", macAddress, ipAddress, username);

    pthread_mutex_lock(&pendingBridgeMutex);
    if (pendingBridges != NULL)
    {
        PendingBridge *bridge = (PendingBridge *) calloc(1, sizeof(PendingBridge));

        bridge->macAddress = strdup(macAddress);
        bridge->ipAddress = strdup(ipAddress);
        bridge->username = strdup(username);

        char *uuid = (char *) calloc(1, 13); // 12 plus null
        macAddrToUUID(uuid, macAddress);
        hashMapPut(pendingBridges, (void *) uuid, (uint16_t) (strlen(uuid) + 1), bridge);
        pthread_mutex_unlock(&pendingBridgeMutex);

        deviceServiceCallbacks->deviceFound(deviceDriver, LIGHT_DC, 1, strdup(uuid), MANUFACTURER, MODEL, "1", "1");
    }
    else
    {
        pthread_mutex_unlock(&pendingBridgeMutex);
    }
}

static bool discoverStart(void *ctx, const char *deviceClass)
{
    icLogDebug(LOG_TAG, "discoverStart: deviceClass=%s", deviceClass);

    if (deviceServiceCallbacks == NULL)
    {
        icLogError(LOG_TAG, "Device driver not yet initialized!");
        return false;
    }

    pthread_mutex_lock(&pendingBridgeMutex);
    pendingBridges = hashMapCreate();
    pthread_mutex_unlock(&pendingBridgeMutex);

    philipsHueStartDiscoveringBridges(bridgeFoundCallback);

    return true;
}

static void pendingBridgeFreeFunc(void *key, void *val)
{
    PendingBridge *bridge = (PendingBridge *) val;
    free(bridge->username);
    free(bridge->ipAddress);
    free(bridge->macAddress);
    free(bridge);

    free(key);
}

static void discoverStop(void *ctx, const char *deviceClass)
{
    icLogDebug(LOG_TAG, "discoverStop");

    philipsHueStopDiscoveringBridges();

    pthread_mutex_lock(&pendingBridgeMutex);
    hashMapDestroy(pendingBridges, pendingBridgeFreeFunc);
    pendingBridges = NULL;
    pthread_mutex_unlock(&pendingBridgeMutex);
}

static bool configureDevice(void *ctx, icDevice *device, DeviceDescriptor *descriptor)
{
    icLogDebug(LOG_TAG, "configureDevice: uuid=%s", device->uuid);

    pthread_mutex_lock(&pendingBridgeMutex);
    PendingBridge *bridge =
        (PendingBridge *) hashMapGet(pendingBridges, device->uuid, (uint16_t) (strlen(device->uuid) + 1));
    pthread_mutex_unlock(&pendingBridgeMutex);

    if (bridge == NULL)
    {
        icLogError(LOG_TAG, "configureDevice: uuid %s not found in pending list", device->uuid);
        return false;
    }

    createDeviceResource(device,
                         COMMON_DEVICE_RESOURCE_MAC_ADDRESS,
                         bridge->macAddress,
                         RESOURCE_TYPE_MAC_ADDRESS,
                         RESOURCE_MODE_READABLE,
                         CACHING_POLICY_ALWAYS);
    createDeviceResource(device,
                         COMMON_DEVICE_RESOURCE_IP_ADDRESS,
                         bridge->ipAddress,
                         RESOURCE_TYPE_IP_ADDRESS,
                         RESOURCE_MODE_READABLE,
                         CACHING_POLICY_ALWAYS);
    createDeviceResource(device,
                         USERNAME_RESOURCE,
                         bridge->username,
                         RESOURCE_TYPE_USER_ID,
                         RESOURCE_MODE_READABLE,
                         CACHING_POLICY_ALWAYS);

    icLinkedList *lights = philipsHueGetLights(bridge->ipAddress, bridge->username);
    if (lights != NULL)
    {
        icLinkedListIterator *iterator = linkedListIteratorCreate(lights);
        while (linkedListIteratorHasNext(iterator))
        {
            PhilipsHueLight *light = (PhilipsHueLight *) linkedListIteratorGetNext(iterator);

            icDeviceEndpoint *endpoint = createEndpoint(device, light->id, LIGHT_PROFILE, true);
            createEndpointResource(endpoint,
                                   COMMON_ENDPOINT_RESOURCE_LABEL,
                                   NULL,
                                   RESOURCE_TYPE_LABEL,
                                   RESOURCE_MODE_READWRITEABLE,
                                   CACHING_POLICY_ALWAYS);
            createEndpointResource(endpoint,
                                   LIGHT_PROFILE_RESOURCE_IS_ON,
                                   light->isOn ? "true" : "false",
                                   RESOURCE_TYPE_BOOLEAN,
                                   RESOURCE_MODE_READWRITEABLE,
                                   CACHING_POLICY_NEVER);
        }
        linkedListIteratorDestroy(iterator);

        linkedListDestroy(lights, NULL);
    }
    deviceServiceCallbacks->deviceConfigured(device);

    startMonitoringBridge(bridge->macAddress, bridge->ipAddress, bridge->username);

    return true;
}

static bool readResource(void *ctx, icDeviceResource *resource, char **value)
{
    bool result = false;

    if (resource == NULL || value == NULL)
    {
        return false;
    }

    icLogDebug(LOG_TAG, "readResource %s", resource->id);

    if (resource->endpointId == NULL)
    {
        // this resource is on our root device, where everything is cached...
    }
    else
    {
        // this resource is on an endpoint
        if (strcmp(resource->id, LIGHT_PROFILE_RESOURCE_IS_ON) == 0)
        {
            PhilipsHueLight *light = getLight(resource->deviceUuid, resource->endpointId);
            if (light != NULL)
            {
                *value = (light->isOn == true) ? strdup("true") : strdup("false");
                philipsHueLightDestroy(light);
                result = true;
            }
        }
    }

    return result;
}

static bool writeResource(void *ctx, icDeviceResource *resource, const char *previousValue, const char *newValue)
{
    bool result = false;

    if (resource == NULL)
    {
        icLogDebug(LOG_TAG, "writeResource: invalid arguments");
        return false;
    }

    if (resource->endpointId == NULL)
    {
        icLogDebug(LOG_TAG,
                   "writeResource on device: id=%s, previousValue=%s, newValue=%s",
                   resource->id,
                   previousValue,
                   newValue);
    }
    else
    {
        icLogDebug(LOG_TAG,
                   "writeResource on endpoint %s: id=%s, previousValue=%s, newValue=%s",
                   resource->endpointId,
                   resource->id,
                   previousValue,
                   newValue);

        icDeviceResource *ipAddressResource =
            deviceServiceCallbacks->getResource(resource->deviceUuid, 0, COMMON_DEVICE_RESOURCE_IP_ADDRESS);
        icDeviceResource *usernameResource =
            deviceServiceCallbacks->getResource(resource->deviceUuid, 0, USERNAME_RESOURCE);

        if (strcmp(resource->id, LIGHT_PROFILE_RESOURCE_IS_ON) == 0)
        {
            if (strcmp("true", newValue) == 0)
            {
                result =
                    philipsHueSetLight(ipAddressResource->value, usernameResource->value, resource->endpointId, true);
            }
            else
            {
                result =
                    philipsHueSetLight(ipAddressResource->value, usernameResource->value, resource->endpointId, false);
            }
        }
        else if (strcmp(resource->id, COMMON_ENDPOINT_RESOURCE_LABEL) == 0)
        {
            result = true; // the updateResource call below will handle the change.
        }

        resourceDestroy(ipAddressResource);
        resourceDestroy(usernameResource);
    }

    deviceServiceCallbacks->updateResource(
        resource->deviceUuid, resource->endpointId, resource->id, newValue, updateResourceEventChanged);

    return result;
}

static void lightChangedCallback(const char *macAddress, const char *lightId, bool isOn)
{
    icLogDebug(LOG_TAG, "lightChanged: %s.%s is now %s\n", macAddress, lightId, (isOn == true) ? "on" : "off");

    char *uuid = (char *) calloc(1, 13); // 12 plus null
    macAddrToUUID(uuid, macAddress);
    deviceServiceCallbacks->updateResource(
        uuid, lightId, LIGHT_PROFILE_RESOURCE_IS_ON, (isOn == true) ? "true" : "false", updateResourceEventChanged);
    free(uuid);
}

static void ipAddressChangedCallback(const char *macAddress, char *newIpAddress)
{
    icLogDebug(LOG_TAG, "ipAddressChanged: %s is now at %s", macAddress, newIpAddress);

    char *uuid = (char *) calloc(1, 13); // 12 plus null
    macAddrToUUID(uuid, macAddress);
    deviceServiceCallbacks->updateResource(
        uuid, NULL, COMMON_DEVICE_RESOURCE_IP_ADDRESS, newIpAddress, updateResourceEventChanged);
    free(uuid);
}

static void startMonitoringBridge(const char *macAddress, const char *ipAddress, const char *username)
{
    char *uuid = (char *) calloc(1, 13); // 12 plus null
    macAddrToUUID(uuid, macAddress);
    pthread_mutex_lock(&monitoringDevicesMutex);
    hashMapPut(monitoringDevices, (void *) uuid, (uint16_t) (strlen(uuid) + 1), strdup(ipAddress));
    pthread_mutex_unlock(&monitoringDevicesMutex);

    philipsHueStartMonitoring(macAddress, ipAddress, username, lightChangedCallback, ipAddressChangedCallback);
}

static void stopMonitoringBridge(const char *uuid)
{
    pthread_mutex_lock(&monitoringDevicesMutex);
    char *ipAddress = (char *) hashMapGet(monitoringDevices, (void *) uuid, (uint16_t) (strlen(uuid) + 1));
    pthread_mutex_unlock(&monitoringDevicesMutex);

    philipsHueStopMonitoring(ipAddress);
}

static void deviceRemoved(void *ctx, icDevice *device)
{
    if (device != NULL && device->uuid != NULL)
    {
        stopMonitoringBridge(device->uuid);
    }
}

static PhilipsHueLight *getLight(const char *deviceUuid, const char *endpointId)
{
    PhilipsHueLight *result = NULL;

    icDeviceResource *ipAddressResource =
        deviceServiceCallbacks->getResource(deviceUuid, 0, COMMON_DEVICE_RESOURCE_IP_ADDRESS);
    icDeviceResource *usernameResource = deviceServiceCallbacks->getResource(deviceUuid, 0, USERNAME_RESOURCE);

    icLinkedList *lights = philipsHueGetLights(ipAddressResource->value, usernameResource->value);
    icLinkedListIterator *iterator = linkedListIteratorCreate(lights);
    while (linkedListIteratorHasNext(iterator))
    {
        PhilipsHueLight *item = (PhilipsHueLight *) linkedListIteratorGetNext(iterator);
        if (strcmp(item->id, endpointId) == 0)
        {
            result = item;
        }
        else
        {
            philipsHueLightDestroy(item);
        }
    }
    linkedListIteratorDestroy(iterator);
    linkedListDestroy(lights, standardDoNotFreeFunc);

    resourceDestroy(ipAddressResource);
    resourceDestroy(usernameResource);

    return result;
}

#endif
