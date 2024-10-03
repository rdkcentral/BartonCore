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

//
// Created by Thomas Lea on 7/29/15.
//

#include <libxml/tree.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "parser.h"
#include <deviceDescriptor.h>
#include <deviceDescriptors.h>
#include <icLog/logging.h>
#include <icTypes/icLinkedList.h>
#include <icUtil/stringUtils.h>
#include <xmlHelper/xmlHelper.h>

static bool parseFiles();

#define LOG_TAG "libdeviceDescriptors"

static pthread_mutex_t dataMutex = PTHREAD_MUTEX_INITIALIZER;

static char allowListPath[PATH_MAX] = {'\0'};

static char denyListPath[PATH_MAX] = {'\0'};

static icLinkedList *deviceDescriptors = NULL;

static bool versionInRange(const char *versionInput, DeviceVersionList *allowedVersions);
static bool versionInAllowedRange(const char *versionInput, const char *minVersionValue, const char *maxVersionValue);
static bool versionInAllowedList(const char *versionInput, icLinkedList *list);

void deviceDescriptorsInit(const char *wlPath, const char *blPath)
{
    icLogDebug(LOG_TAG, "deviceDescriptorsInit:  using AllowList %s, and DenyList %s", wlPath, blPath);
    if (wlPath != NULL)
    {
        strncpy(allowListPath, wlPath, PATH_MAX - 1);
    }

    if (blPath != NULL)
    {
        strncpy(denyListPath, blPath, PATH_MAX - 1);
    }
}

void deviceDescriptorsCleanup()
{
    icLogDebug(LOG_TAG, "deviceDescriptorsCleanup");
    pthread_mutex_lock(&dataMutex);

    if (deviceDescriptors != NULL)
    {
        linkedListDestroy(deviceDescriptors, (linkedListItemFreeFunc) deviceDescriptorFree);
        deviceDescriptors = NULL;
    }

    pthread_mutex_unlock(&dataMutex);
}

/*
 * Retrieve the matching DeviceDescriptor for the provided input or NULL if a matching one doesnt exist.
 */
DeviceDescriptor *deviceDescriptorsGet(const char *manufacturer,
                                       const char *model,
                                       const char *hardwareVersion,
                                       const char *firmwareVersion)
{
    DeviceDescriptor *result = NULL;

    icLogDebug(LOG_TAG,
               "deviceDescriptorsGet: manufacturer=%s, model=%s, hardwareVersion=%s, firmwareVersion=%s",
               manufacturer,
               model,
               hardwareVersion,
               firmwareVersion);

    // manufacturer and model are required
    if (manufacturer == NULL || model == NULL)
    {
        icLogError(LOG_TAG, "deviceDescriptorsGet: invalid arguments");
        return NULL;
    }

    pthread_mutex_lock(&dataMutex);

    if (deviceDescriptors == NULL)
    {
        icLogDebug(LOG_TAG, "no device descriptors loaded yet, attempting parse");
        parseFiles();
    }

    if (deviceDescriptors != NULL && linkedListCount(deviceDescriptors) > 0)
    {
        icLinkedListIterator *iterator = linkedListIteratorCreate(deviceDescriptors);
        while (linkedListIteratorHasNext(iterator))
        {
            DeviceDescriptor *dd = linkedListIteratorGetNext(iterator);

            if (strcmp(manufacturer, dd->manufacturer) != 0)
            {
                continue;
            }

            if (strcmp(model, dd->model) != 0)
            {
                continue;
            }

            if (versionInRange(hardwareVersion, dd->hardwareVersions) == false)
            {
                continue;
            }

            if (versionInRange(firmwareVersion, dd->firmwareVersions) == false)
            {
                continue;
            }

            // if we got here, we have a match!
            // to prevent use after free issues create a copy to hand back to the caller
            result = deviceDescriptorClone(dd);
            break;
        }

        linkedListIteratorDestroy(iterator);
    }
    else
    {
        icLogDebug(LOG_TAG, "no device descriptors available.");
    }

    pthread_mutex_unlock(&dataMutex);

    return result;
}

static bool parseFiles()
{
    bool result = true;

    if (strlen(allowListPath) == 0)
    {
        icLogError(LOG_TAG, "parseFiles: no AllowList path set!");
        return false;
    }

    deviceDescriptors = parseDeviceDescriptors(allowListPath, denyListPath);

    return result;
}

/*
 * return true if the provided versionInput is in the allowedVersions data structure
 */
static bool versionInRange(const char *versionInput, DeviceVersionList *allowedVersions)
{
    bool result = false;
    if (versionInput != NULL && allowedVersions != NULL)
    {
        if (allowedVersions->listType == DEVICE_VERSION_LIST_TYPE_WILDCARD)
        {
            result = true;
        }
        else
        {
            // check to see if the versionInput exactly matches a version in the allowed list (ignoring case)
            //
            result = versionInAllowedList(versionInput, allowedVersions->list.versionList);
            if (result == false)
            {
                result = versionInAllowedRange(
                    versionInput, allowedVersions->list.versionRange.from, allowedVersions->list.versionRange.to);
            }
        }
    }
    return result;
}

/*
 * check if version is available in the given list.
 * @param versionInput device version
 * @param list valid version list
 * @return true if version is available in version list, false otherwise
 */
static bool versionInAllowedList(const char *versionInput, icLinkedList *list)
{
    bool result = false;
    if (list != NULL)
    {
        scoped_icLinkedListIterator *iterator = linkedListIteratorCreate(list);
        while (linkedListIteratorHasNext(iterator))
        {
            char *allowedVersion = linkedListIteratorGetNext(iterator);
            if (stringCompare(allowedVersion, versionInput, true) == 0)
            {
                result = true;
                break;
            }
        }
    }
    return result;
}

/*
 * check if version is within the given range.
 * @param versionInput device version
 * @param minVersionValue minimum value in range(inclusive)
 * @param maxVersionValue maximum value in range(inclusive)
 * @return true if version is within range, false otherwise
 */
static bool versionInAllowedRange(const char *versionInput, const char *minVersionValue, const char *maxVersionValue)
{
    bool result = false;
    uint64_t input = 0;
    if (stringToUint64(versionInput, &input) == false)
    {
        icLogWarn(LOG_TAG, "%s: Unable to convert firmware version: %s", __FUNCTION__, versionInput);
        return false;
    }

    if (minVersionValue != NULL && maxVersionValue != NULL)
    {
        uint64_t minValue = 0;
        uint64_t maxValue = UINT64_MAX;
        bool isValidRange = true;

        if (stringIsEmpty(minVersionValue) == false && stringToUint64(minVersionValue, &minValue) == false)
        {
            icLogWarn(LOG_TAG, "%s: Unable to convert minValue %s to a uint64", __FUNCTION__, minVersionValue);
            isValidRange = false;
        }

        if (stringIsEmpty(maxVersionValue) == false && stringToUint64(maxVersionValue, &maxValue) == false)
        {
            icLogWarn(LOG_TAG, "%s: Unable to convert maxValue %s to a uint64", __FUNCTION__, maxVersionValue);
            isValidRange = false;
        }

        if (isValidRange == true)
        {
            if (input >= minValue && input <= maxValue)
            {
                result = true;
            }
        }
    }
    return result;
}

char *getAllowListPath()
{
    if (strlen(allowListPath) > 0)
    {
        return strdup(allowListPath);
    }
    else
    {
        return NULL;
    }
}

char *getDenyListPath()
{
    if (strlen(denyListPath) > 0)
    {
        return strdup(denyListPath);
    }
    else
    {
        return NULL;
    }
}

/*
 * Check whether a given allow list is valid/parsable.  This function does NOT require deviceDescriptorsInit to be
 * called
 * @param allowListPath the allow list file to check
 * @return true if valid, false otherwise
 */
bool checkAllowListValid(const char *wlPath)
{
    bool valid = false;
    icLinkedList *descriptors = parseDeviceDescriptors(wlPath, NULL);
    if (descriptors != NULL)
    {
        valid = true;
        linkedListDestroy(descriptors, (linkedListItemFreeFunc) deviceDescriptorFree);
    }

    return valid;
}

/*
 * Check whether a given deny list is valid/parsable.  This function does NOT require deviceDescriptorsInit to be
 * called
 * @param denyListPath the deny list file to check
 * @return true if valid, false otherwise
 */
bool checkDenyListValid(const char *blPath)
{
    bool valid = false;

    icStringHashMap *denylistUuids = getDenylistedUuids(blPath);
    if (denylistUuids != NULL)
    {
        valid = true;
        stringHashMapDestroy(denylistUuids, NULL);
    }

    return valid;
}
