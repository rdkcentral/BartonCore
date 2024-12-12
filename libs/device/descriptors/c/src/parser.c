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
// Created by Thomas Lea on 7/31/15.
//

#include "parser.h"
#include <deviceDescriptor.h>
#include <glib.h>
#include <icLog/logging.h>
#include <icLog/telemetryMarkers.h>
#include <icTypes/icStringHashMap.h>
#include <icUtil/fileUtils.h>
#include <icUtil/stringUtils.h>
#include <inttypes.h>
#include <libxml/tree.h>
#include <string.h>
#include <xmlHelper/xmlHelper.h>

#define LOG_TAG                                         "libdeviceDescriptorParser"

#define DDL_ROOT_NODE                                   "DeviceDescriptorList"
#define CAMERA_DD_NODE                                  "CameraDeviceDescriptor"
#define ZIGBEE_DD_NODE                                  "DeviceDescriptor"
#define UUID_NODE                                       "uuid"
#define DESCRIPTION_NODE                                "description"
#define CATEGORY_NODE                                   "category"
#define CATEGORY_TYPE_ZIGBEE_NODE                       "zigbee"
#define CATEGORY_TYPE_CAMERA_NODE                       "camera"
#define CATEGORY_TYPE_PHILIPS_HUE_NODE                  "PhilipsHue"
#define CATEGORY_TYPE_MATTER_NODE                       "matter"
#define MANUFACTURER_NODE                               "manufacturer"
#define MODEL_NODE                                      "model"
#define HARDWARE_VERSIONS_NODE                          "hardwareVersions"
#define FIRMWARE_VERSIONS_NODE                          "firmwareVersions"
#define MIN_FIRMWARE_VERSIONS_NODE                      "minSupportedFirmwareVersion"
#define METADATA_LIST_NODE                              "metadataList"
#define METADATA_NODE                                   "metadata"
#define NAME_NODE                                       "name"
#define VALUE_NODE                                      "value"
#define LATEST_FIRMWARE_NODE                            "latestFirmware"
#define LATEST_FIRMWARE_VERSION_NODE                    "version"
#define LATEST_FIRMWARE_FILENAME_NODE                   "filename"
#define LATEST_FIRMWARE_TYPE_NODE                       "type"
#define LATEST_FIRMWARE_POST_UPGRADE_ACTION_NODE        "postUpgradeAction"
#define CASCADE_ADD_DELETE_ENDPOINTS_NODE               "cascadeAddDeleteEndpoints"
#define LATEST_FIRMWARE_TYPE_ZIGBEE_OTA                 "ota"
#define LATEST_FIRMWARE_TYPE_ZIGBEE_LEGACY              "legacy"
#define LATEST_FIRMWARE_POST_UPGRADE_ACTION_NONE        "none"
#define LATEST_FIRMWARE_POST_UPGRADE_ACTION_RECONFIGURE "reconfigure"
#define LATEST_FIRMWARE_CHECKSUM_ATTRIBUTE              "checksum"
#define VERSION_LIST_FORMAT                             "format"
#define PROTOCOL_NODE                                   "protocol"
#define MOTION_NODE                                     "motion"
#define MATTER_NODE                                     "matter"
#define MATTER_VENDOR_ID_NODE                           "vendorId"
#define MATTER_PRODUCT_ID_NODE                          "productId"
#define LIST_NODE                                       "list"
#define ANY_NODE                                        "any"
#define RANGE_NODE                                      "range"
#define ENABLED_NODE                                    "enabled"
#define SENSITIVITY_NODE                                "sensitivityLevel"
#define LOW_NODE                                        "low"
#define MEDIUM_NODE                                     "med"
#define HIGH_NODE                                       "high"
#define DETECTION_NODE                                  "detectionThreshold"
#define REGION_OF_INTEREST_NODE                         "regionOfInterest"
#define WIDTH_NODE                                      "width"
#define HEIGHT_NODE                                     "height"
#define BOTTOM_NODE                                     "bottomCoord"
#define TOP_NODE                                        "topCoord"
#define LEFT_NODE                                       "leftCoord"
#define RIGHT_NODE                                      "rightCoord"
#define FROM_NODE                                       "from"
#define TO_NODE                                         "to"

static char *getTrimmedXmlNodeContentsAsString(xmlNodePtr node);

/*
 * parse the range value from the given version string.
 */
static void parseZigbeeDeviceVersionRange(char *versions, char **fromRange, char **toRange, bool forceDecimal);

/*
 * Parses the Matter node and returns a MatterTechnology struct.
 * Returns NULL if the parsing fails, and a valid pointer otherwise (caller frees).
 */
static MatterTechnology *parseMatterTechnologyNode(xmlNodePtr matterNode);

static void parseDeviceVersionList(xmlNodePtr node, DeviceVersionList *list)
{
    list->format = getXmlNodeAttributeAsString(node, VERSION_LIST_FORMAT, NULL);
    xmlNodePtr loopNode = node->children;
    xmlNodePtr currNode = NULL;
    for (currNode = loopNode; currNode != NULL; currNode = currNode->next)
    {
        // skip comments, blanks, etc
        //
        if (currNode->type != XML_ELEMENT_NODE)
        {
            continue;
        }

        if (strcmp((const char *) currNode->name, LIST_NODE) == 0)
        {
            list->listType = DEVICE_VERSION_LIST_TYPE_LIST;
            list->list.versionList = linkedListCreate();
            xmlNodePtr listNode = currNode->children;
            xmlNodePtr listItemNode = NULL;
            for (listItemNode = listNode; listItemNode != NULL; listItemNode = listItemNode->next)
            {
                // skip comments, blanks, etc
                //
                if (listItemNode->type != XML_ELEMENT_NODE)
                {
                    continue;
                }

                linkedListAppend(list->list.versionList, getTrimmedXmlNodeContentsAsString(listItemNode));
            }
            break;
        }
        else if (strcmp((const char *) currNode->name, ANY_NODE) == 0)
        {
            list->listType = DEVICE_VERSION_LIST_TYPE_WILDCARD;
            break;
        }
        else if (strcmp((const char *) currNode->name, RANGE_NODE) == 0)
        {
            list->listType = DEVICE_VERSION_LIST_TYPE_RANGE;
            xmlNodePtr rangeNode = currNode->children;
            xmlNodePtr rangeItemNode = NULL;
            for (rangeItemNode = rangeNode; rangeItemNode != NULL; rangeItemNode = rangeItemNode->next)
            {
                // skip comments, blanks, etc
                //
                if (rangeItemNode->type != XML_ELEMENT_NODE)
                {
                    continue;
                }


                char *version = getTrimmedXmlNodeContentsAsString(rangeItemNode);
                if (version != NULL)
                {
                    if (strcmp((const char *) rangeItemNode->name, FROM_NODE) == 0)
                    {
                        list->list.versionRange.from = version;
                    }
                    else if (strcmp((const char *) rangeItemNode->name, TO_NODE) == 0)
                    {
                        list->list.versionRange.to = version;
                    }
                    else
                    {
                        // unused
                        free(version);
                    }
                }
            }
            break;
        }
        else
        {
            icLogError(LOG_TAG, "Unexpected device version list type");
            break;
        }
    }
}

// given a version string that could be decimal or hex, convert the string to decimal
static char *versionStringToDecimalString(const char *version)
{
    int value = (int) strtol(version, NULL, 0);
    char buf[20];
    sprintf(buf, "%" PRIu32, value);
    return trimString(buf);
}

static void parseZigbeeDeviceVersion(xmlNodePtr node, DeviceVersionList *list, bool forceDecimal)
{
    char *versions = getXmlNodeContentsAsString(node, NULL);

    list->format = strdup("Zigbee");

    if (strlen(versions) > 0 && versions[0] == '*')
    {
        list->listType = DEVICE_VERSION_LIST_TYPE_WILDCARD;
    }
    else if (strstr(versions, ",") != NULL)
    {
        list->listType = DEVICE_VERSION_LIST_TYPE_LIST;
        list->list.versionList = linkedListCreate();
        char *it = versions;
        char *token;
        while ((token = strsep(&it, ",")) != NULL)
        {
            if (strstr(token, "-") != NULL)
            {
                list->listType = DEVICE_VERSION_LIST_TYPE_LIST_AND_RANGE;
                parseZigbeeDeviceVersionRange(
                    token, &list->list.versionRange.from, &list->list.versionRange.to, forceDecimal);
            }
            else
            {
                char *versionValue = forceDecimal ? versionStringToDecimalString(token) : trimString(token);
                if (forceDecimal == false)
                {
                    stringToLowerCase(versionValue);
                }
                linkedListAppend(list->list.versionList, versionValue);
            }
        }
    }
    else if (strstr(versions, "-") != NULL)
    {
        list->listType = DEVICE_VERSION_LIST_TYPE_RANGE;
        parseZigbeeDeviceVersionRange(
            versions, &list->list.versionRange.from, &list->list.versionRange.to, forceDecimal);
    }
    else
    {
        list->listType = DEVICE_VERSION_LIST_TYPE_LIST;
        list->list.versionList = linkedListCreate();

        char *versionValue = forceDecimal ? versionStringToDecimalString(versions) : trimString(versions);
        linkedListAppend(list->list.versionList, versionValue);
    }

    free(versions);
}

/*
 * parse the range value from the given version string.
 */
static void parseZigbeeDeviceVersionRange(char *versions, char **fromRange, char **toRange, bool forceDecimal)
{
    char *it = versions;
    char *first = strsep(&it, "-");
    char *second = strsep(&it, "-");

    char *minVersionValue = forceDecimal ? versionStringToDecimalString(first) : trimString(first);

    if (forceDecimal == false)
    {
        stringToLowerCase(minVersionValue);
    }

    *fromRange = minVersionValue;

    char *maxVersionValue = forceDecimal ? versionStringToDecimalString(second) : trimString(second);

    if (forceDecimal == false)
    {
        stringToLowerCase(maxVersionValue);
    }

    *toRange = maxVersionValue;
}

static bool parseMetadataList(xmlNodePtr metadataNode, icStringHashMap *map)
{
    bool result = true;

    xmlNodePtr loopNode = metadataNode->children;
    xmlNodePtr currNode = NULL;
    for (currNode = loopNode; currNode != NULL; currNode = currNode->next)
    {
        // skip comments, blanks, etc
        //
        if (currNode->type != XML_ELEMENT_NODE)
        {
            continue;
        }

        if (strcmp((const char *) currNode->name, METADATA_NODE) == 0)
        {
            char *name = getXmlNodeAttributeAsString(currNode, NAME_NODE, NULL);
            char *value = getXmlNodeAttributeAsString(currNode, VALUE_NODE, NULL);

            if (name != NULL && value != NULL)
            {
                stringHashMapPut(map, name, value);
            }
            else
            {
                if (name != NULL)
                {
                    free(name);
                }
                if (value != NULL)
                {
                    free(value);
                }
            }
        }
    }

    return result;
}

static bool parseDescriptorBase(xmlNodePtr ddNode, DeviceDescriptor *dd)
{
    bool result = true;

    xmlNodePtr loopNode = ddNode->children;
    xmlNodePtr currNode = NULL;
    for (currNode = loopNode; currNode != NULL; currNode = currNode->next)
    {
        // skip comments, blanks, etc
        //
        if (currNode->type != XML_ELEMENT_NODE)
        {
            continue;
        }

        if (strcmp((const char *) currNode->name, UUID_NODE) == 0)
        {
            dd->uuid = getXmlNodeContentsAsString(currNode, NULL);
        }
        else if (strcmp((const char *) currNode->name, DESCRIPTION_NODE) == 0)
        {
            dd->description = getXmlNodeContentsAsString(currNode, NULL);
        }
        else if (strcmp((const char *) currNode->name, MANUFACTURER_NODE) == 0)
        {
            dd->manufacturer = getXmlNodeContentsAsString(currNode, NULL);
        }
        else if (strcmp((const char *) currNode->name, MODEL_NODE) == 0)
        {
            dd->model = getXmlNodeContentsAsString(currNode, NULL);
        }
        else if (strcmp((const char *) currNode->name, HARDWARE_VERSIONS_NODE) == 0)
        {
            dd->hardwareVersions = (DeviceVersionList *) calloc(1, sizeof(DeviceVersionList));
            if (dd->hardwareVersions == NULL)
            {
                icLogError(LOG_TAG, "Failed to allocate DeviceVersionList");
                break;
            }
            if (dd->deviceDescriptorType == DEVICE_DESCRIPTOR_TYPE_CAMERA)
            {
                parseDeviceVersionList(currNode, dd->hardwareVersions);
            }
            else
            {
                parseZigbeeDeviceVersion(currNode, dd->hardwareVersions, true);
            }
        }
        else if (strcmp((const char *) currNode->name, FIRMWARE_VERSIONS_NODE) == 0)
        {
            dd->firmwareVersions = (DeviceVersionList *) calloc(1, sizeof(DeviceVersionList));
            if (dd->firmwareVersions == NULL)
            {
                icLogError(LOG_TAG, "Failed to allocate DeviceVersionList");
                break;
            }
            if (dd->deviceDescriptorType == DEVICE_DESCRIPTOR_TYPE_CAMERA)
            {
                parseDeviceVersionList(currNode, dd->firmwareVersions);
            }
            else
            {
                parseZigbeeDeviceVersion(currNode, dd->firmwareVersions, false);
            }
        }
        else if (strcmp((const char *) currNode->name, MIN_FIRMWARE_VERSIONS_NODE) == 0)
        {
            dd->minSupportedFirmwareVersion = getXmlNodeContentsAsString(currNode, NULL);
        }
        else if (strcmp((const char *) currNode->name, METADATA_LIST_NODE) == 0)
        {
            dd->metadata = stringHashMapCreate();
            parseMetadataList(currNode, dd->metadata);
        }
        else if (strcmp((const char *) currNode->name, LATEST_FIRMWARE_NODE) == 0)
        {
            dd->latestFirmware = (DeviceFirmware *) calloc(1, sizeof(DeviceFirmware));
            if (dd->latestFirmware == NULL)
            {
                icLogError(LOG_TAG, "Failed to allocate DeviceFirmware");
            }
            else
            {
                dd->latestFirmware->upgradeAction = POST_UPGRADE_ACTION_NONE;
                if (dd->deviceDescriptorType == DEVICE_DESCRIPTOR_TYPE_CAMERA)
                {
                    dd->latestFirmware->type = DEVICE_FIRMWARE_TYPE_CAMERA;
                }
                else
                {
                    dd->latestFirmware->type = DEVICE_FIRMWARE_TYPE_UNKNOWN;
                }
                xmlNodePtr latestFirmwareLoopNode = currNode->children;
                xmlNodePtr latestFirmwareCurrNode = NULL;
                for (latestFirmwareCurrNode = latestFirmwareLoopNode; latestFirmwareCurrNode != NULL;
                     latestFirmwareCurrNode = latestFirmwareCurrNode->next)
                {
                    if (strcmp((const char *) latestFirmwareCurrNode->name, LATEST_FIRMWARE_VERSION_NODE) == 0)
                    {
                        dd->latestFirmware->version = getTrimmedXmlNodeContentsAsString(latestFirmwareCurrNode);
                    }
                    else if (strcmp((const char *) latestFirmwareCurrNode->name, LATEST_FIRMWARE_FILENAME_NODE) == 0)
                    {
                        if (dd->latestFirmware->fileInfos == NULL)
                        {
                            dd->latestFirmware->fileInfos = linkedListCreate();
                        }

                        DeviceFirmwareFileInfo *fileInfo =
                            (DeviceFirmwareFileInfo *) calloc(1, sizeof(DeviceFirmwareFileInfo));
                        fileInfo->fileName = getTrimmedXmlNodeContentsAsString(latestFirmwareCurrNode);
                        if (fileInfo->fileName != NULL)
                        {
                            fileInfo->checksum = getXmlNodeAttributeAsString(
                                latestFirmwareCurrNode, LATEST_FIRMWARE_CHECKSUM_ATTRIBUTE, NULL);

                            linkedListAppend(dd->latestFirmware->fileInfos, fileInfo);
                        }
                        else
                        {
                            free(fileInfo);
                        }
                    }
                    else if (strcmp((const char *) latestFirmwareCurrNode->name, LATEST_FIRMWARE_TYPE_NODE) == 0)
                    {
                        char *firmwareType = getXmlNodeContentsAsString(latestFirmwareCurrNode, NULL);
                        // Java code uses case insensitive comparison here
                        if (strcasecmp(firmwareType, LATEST_FIRMWARE_TYPE_ZIGBEE_OTA) == 0)
                        {
                            dd->latestFirmware->type = DEVICE_FIRMWARE_TYPE_ZIGBEE_OTA;
                        }
                        else if (strcasecmp(firmwareType, LATEST_FIRMWARE_TYPE_ZIGBEE_LEGACY) == 0)
                        {
                            dd->latestFirmware->type = DEVICE_FIRMWARE_TYPE_ZIGBEE_LEGACY;
                        }
                        free(firmwareType);
                    }
                    else if (strcmp((const char *) latestFirmwareCurrNode->name,
                                    LATEST_FIRMWARE_POST_UPGRADE_ACTION_NODE) == 0)
                    {
                        scoped_generic char *postUpgradeAction =
                            getXmlNodeContentsAsString(latestFirmwareCurrNode, NULL);
                        if (strcasecmp(postUpgradeAction, LATEST_FIRMWARE_POST_UPGRADE_ACTION_NONE) == 0)
                        {
                            dd->latestFirmware->upgradeAction = POST_UPGRADE_ACTION_NONE;
                        }
                        else if (strcasecmp(postUpgradeAction, LATEST_FIRMWARE_POST_UPGRADE_ACTION_RECONFIGURE) == 0)
                        {
                            dd->latestFirmware->upgradeAction = POST_UPGRADE_ACTION_RECONFIGURE;
                        }
                    }
                }
            }
        }
        else if (strcmp((const char *) currNode->name, CASCADE_ADD_DELETE_ENDPOINTS_NODE) == 0)
        {
            dd->cascadeAddDeleteEndpoints = getXmlNodeContentsAsBoolean(currNode, false);
        }
        else if (strcmp((const char *) currNode->name, CATEGORY_NODE) == 0)
        {
            scoped_generic char *categoryValue = getXmlNodeContentsAsString(currNode, NULL);
            if (strcasecmp(categoryValue, CATEGORY_TYPE_ZIGBEE_NODE) == 0)
            {
                dd->category = CATEGORY_TYPE_ZIGBEE;
            }
            else if (strcasecmp(categoryValue, CATEGORY_TYPE_CAMERA_NODE) == 0)
            {
                dd->category = CATEGORY_TYPE_CAMERA;
            }
            else if (strcasecmp(categoryValue, CATEGORY_TYPE_PHILIPS_HUE_NODE) == 0)
            {
                dd->category = CATEGORY_TYPE_PHILIPS_HUE;
            }
            else if (strcasecmp(categoryValue, CATEGORY_TYPE_MATTER_NODE) == 0)
            {
                dd->category = CATEGORY_TYPE_MATTER;
            }
        }
        else if (strcmp((const char *) currNode->name, MATTER_NODE) == 0)
        {
            dd->matter = parseMatterTechnologyNode(currNode);
            if (dd->matter == NULL)
            {
                if (dd->category == CATEGORY_TYPE_MATTER)
                {
                    break;
                }
            }
        }
    }

    if (dd->category == CATEGORY_TYPE_MATTER && dd->matter == NULL)
    {
        icLogError(LOG_TAG, "Cannot parse descriptor. Matter category does not have any valid <matter> node");
        result = false;
    }

    return result;
}

static bool parseCameraMotionNode(xmlNodePtr motionNode, CameraDeviceDescriptor *dd)
{
    bool result = true;
    xmlNodePtr loopNode = motionNode->children;
    xmlNodePtr currNode = NULL;
    for (currNode = loopNode; currNode != NULL; currNode = currNode->next)
    {
        // skip comments, blanks, etc
        //
        if (currNode->type != XML_ELEMENT_NODE)
        {
            continue;
        }

        if (strcmp((const char *) currNode->name, ENABLED_NODE) == 0)
        {
            char *enabled = getXmlNodeContentsAsString(currNode, "false");
            if (strcmp(enabled, "true") == 0)
            {
                dd->defaultMotionSettings.enabled = true;
            }
            else
            {
                dd->defaultMotionSettings.enabled = false;
            }
            free(enabled);
        }
        else if (strcmp((const char *) currNode->name, SENSITIVITY_NODE) == 0)
        {
            xmlNodePtr innerCurrNode = NULL;
            for (innerCurrNode = currNode->children; innerCurrNode != NULL; innerCurrNode = innerCurrNode->next)
            {
                if (innerCurrNode->type != XML_ELEMENT_NODE)
                {
                    continue;
                }

                if (strcmp((const char *) innerCurrNode->name, LOW_NODE) == 0)
                {
                    char *tmp = getXmlNodeContentsAsString(innerCurrNode, NULL);
                    if (tmp != NULL)
                    {
                        dd->defaultMotionSettings.sensitivity.low = atoi(tmp);
                        free(tmp);
                    }
                    else
                    {
                        icLogError(LOG_TAG, "Invalid sensitivityLevel value for low");
                    }
                }
                else if (strcmp((const char *) innerCurrNode->name, MEDIUM_NODE) == 0)
                {
                    char *tmp = getXmlNodeContentsAsString(innerCurrNode, NULL);
                    if (tmp != NULL)
                    {
                        dd->defaultMotionSettings.sensitivity.medium = atoi(tmp);
                        free(tmp);
                    }
                    else
                    {
                        icLogError(LOG_TAG, "Invalid sensitivityLevel value for medium");
                    }
                }
                else if (strcmp((const char *) innerCurrNode->name, HIGH_NODE) == 0)
                {
                    char *tmp = getXmlNodeContentsAsString(innerCurrNode, NULL);
                    if (tmp != NULL)
                    {
                        dd->defaultMotionSettings.sensitivity.high = atoi(tmp);
                        free(tmp);
                    }
                    else
                    {
                        icLogError(LOG_TAG, "Invalid sensitivityLevel value for high");
                    }
                }
            }
        }
        else if (strcmp((const char *) currNode->name, DETECTION_NODE) == 0)
        {
            xmlNodePtr innerCurrNode = NULL;
            for (innerCurrNode = currNode->children; innerCurrNode != NULL; innerCurrNode = innerCurrNode->next)
            {
                if (innerCurrNode->type != XML_ELEMENT_NODE)
                {
                    continue;
                }

                if (strcmp((const char *) innerCurrNode->name, LOW_NODE) == 0)
                {
                    char *tmp = getXmlNodeContentsAsString(innerCurrNode, NULL);
                    if (tmp != NULL)
                    {
                        dd->defaultMotionSettings.detectionThreshold.low = atoi(tmp);
                        free(tmp);
                    }
                    else
                    {
                        icLogError(LOG_TAG, "Invalid detectionThreshold value for low");
                    }
                }
                else if (strcmp((const char *) innerCurrNode->name, MEDIUM_NODE) == 0)
                {
                    char *tmp = getXmlNodeContentsAsString(innerCurrNode, NULL);
                    if (tmp != NULL)
                    {
                        dd->defaultMotionSettings.detectionThreshold.medium = atoi(tmp);
                        free(tmp);
                    }
                    else
                    {
                        icLogError(LOG_TAG, "Invalid detectionThreshold value for medium");
                    }
                }
                else if (strcmp((const char *) innerCurrNode->name, HIGH_NODE) == 0)
                {
                    char *tmp = getXmlNodeContentsAsString(innerCurrNode, NULL);
                    if (tmp != NULL)
                    {
                        dd->defaultMotionSettings.detectionThreshold.high = atoi(tmp);
                        free(tmp);
                    }
                    else
                    {
                        icLogError(LOG_TAG, "Invalid detectionThreshold value for high");
                    }
                }
            }
        }
        else if (strcmp((const char *) currNode->name, REGION_OF_INTEREST_NODE) == 0)
        {
            xmlNodePtr innerCurrNode = NULL;
            for (innerCurrNode = currNode->children; innerCurrNode != NULL; innerCurrNode = innerCurrNode->next)
            {
                if (innerCurrNode->type != XML_ELEMENT_NODE)
                {
                    continue;
                }

                if (strcmp((const char *) innerCurrNode->name, WIDTH_NODE) == 0)
                {
                    char *tmp = getXmlNodeContentsAsString(innerCurrNode, NULL);
                    if (tmp != NULL)
                    {
                        dd->defaultMotionSettings.regionOfInterest.width = atoi(tmp);
                        free(tmp);
                    }
                    else
                    {
                        icLogError(LOG_TAG, "Invalid regionOfInterest value for width");
                    }
                }
                else if (strcmp((const char *) innerCurrNode->name, HEIGHT_NODE) == 0)
                {
                    char *tmp = getXmlNodeContentsAsString(innerCurrNode, NULL);
                    if (tmp != NULL)
                    {
                        dd->defaultMotionSettings.regionOfInterest.height = atoi(tmp);
                        free(tmp);
                    }
                    else
                    {
                        icLogError(LOG_TAG, "Invalid regionOfInterest value for height");
                    }
                }
                else if (strcmp((const char *) innerCurrNode->name, BOTTOM_NODE) == 0)
                {
                    char *tmp = getXmlNodeContentsAsString(innerCurrNode, NULL);
                    if (tmp != NULL)
                    {
                        dd->defaultMotionSettings.regionOfInterest.bottom = atoi(tmp);
                        free(tmp);
                    }
                    else
                    {
                        icLogError(LOG_TAG, "Invalid regionOfInterest value for bottom");
                    }
                }
                else if (strcmp((const char *) innerCurrNode->name, TOP_NODE) == 0)
                {
                    char *tmp = getXmlNodeContentsAsString(innerCurrNode, NULL);
                    if (tmp != NULL)
                    {
                        dd->defaultMotionSettings.regionOfInterest.top = atoi(tmp);
                        free(tmp);
                    }
                    else
                    {
                        icLogError(LOG_TAG, "Invalid regionOfInterest value for top");
                    }
                }
                else if (strcmp((const char *) innerCurrNode->name, LEFT_NODE) == 0)
                {
                    char *tmp = getXmlNodeContentsAsString(innerCurrNode, NULL);
                    if (tmp != NULL)
                    {
                        dd->defaultMotionSettings.regionOfInterest.left = atoi(tmp);
                        free(tmp);
                    }
                    else
                    {
                        icLogError(LOG_TAG, "Invalid regionOfInterest value for left");
                    }
                }
                else if (strcmp((const char *) innerCurrNode->name, RIGHT_NODE) == 0)
                {
                    char *tmp = getXmlNodeContentsAsString(innerCurrNode, NULL);
                    if (tmp != NULL)
                    {
                        dd->defaultMotionSettings.regionOfInterest.right = atoi(tmp);
                        free(tmp);
                    }
                    else
                    {
                        icLogError(LOG_TAG, "Invalid regionOfInterest value for right");
                    }
                }
            }
        }
    }

    return result;
}

static bool parseCameraDescriptor(xmlNodePtr cameraNode, CameraDeviceDescriptor *dd)
{
    bool result = true;
    xmlNodePtr loopNode = cameraNode->children;
    xmlNodePtr currNode = NULL;
    for (currNode = loopNode; currNode != NULL; currNode = currNode->next)
    {
        // skip comments, blanks, etc
        //
        if (currNode->type != XML_ELEMENT_NODE)
        {
            continue;
        }

        if (strcmp((const char *) currNode->name, PROTOCOL_NODE) == 0)
        {
            char *protocol = getXmlNodeContentsAsString(currNode, NULL);
            if (protocol != NULL)
            {
                if (strcmp(protocol, "legacy") == 0)
                {
                    dd->protocol = CAMERA_PROTOCOL_LEGACY;
                }
                else if (strcmp(protocol, "openHome") == 0)
                {
                    dd->protocol = CAMERA_PROTOCOL_OPENHOME;
                }
                else
                {
                    dd->protocol = CAMERA_PROTOCOL_UNKNOWN;
                }

                free(protocol);
            }
        }
        else if (strcmp((const char *) currNode->name, MOTION_NODE) == 0)
        {
            result = parseCameraMotionNode(currNode, dd);
        }
    }

    return result;
}

static MatterTechnology *parseMatterTechnologyNode(xmlNodePtr matterNode)
{
    MatterTechnology *result = NULL;
    MatterTechnology *matter = (MatterTechnology *) calloc(1, sizeof(MatterTechnology));

    bool isValidVID = false;
    bool isValidPID = false;

    for (xmlNodePtr currNode = matterNode->children; currNode != NULL; currNode = currNode->next)
    {
        // skip comments, blanks, etc
        //
        if (currNode->type != XML_ELEMENT_NODE)
        {
            continue;
        }

        if (strcmp((const char *) currNode->name, MATTER_VENDOR_ID_NODE) == 0)
        {
            scoped_generic char *vendorId = getTrimmedXmlNodeContentsAsString(currNode);
            if (vendorId == NULL)
            {
                icLogError(LOG_TAG,
                           "Failed to parse <matter> sub-element at line %" PRIu32 ": '<vendorID>' does not exist",
                           getXmlNodeLineNumber(currNode));
                break;
            }

            uint16_t localResult;
            if (hexStringToUint16(vendorId, &localResult) == false)
            {
                icLogError(LOG_TAG,
                           "Failed to parse <matter> sub-element at line %" PRIu32
                           ": <vendorId> '%s' must be a valid hex number between 0x0000 and 0xFFFF",
                           getXmlNodeLineNumber(currNode),
                           vendorId);
                break;
            }

            matter->vendorId = localResult;
            isValidVID = true;
        }
        else if (strcmp((const char *) currNode->name, MATTER_PRODUCT_ID_NODE) == 0)
        {
            scoped_generic char *productId = getTrimmedXmlNodeContentsAsString(currNode);
            if (productId == NULL)
            {
                icLogError(LOG_TAG,
                           "Failed to parse <matter> sub-element at line %" PRIu32 ": '<productId>' does not exist",
                           getXmlNodeLineNumber(currNode));
                break;
            }

            uint16_t localResult;
            if (hexStringToUint16(productId, &localResult) == false)
            {
                icLogError(LOG_TAG,
                           "Failed to parse <matter> sub-element at line %" PRIu32
                           ": <productId> '%s' must be a valid hex number between 0x0000 and 0xFFFF",
                           getXmlNodeLineNumber(currNode),
                           productId);
                break;
            }

            matter->productId = localResult;
            isValidPID = true;
        }
    }

    if (isValidVID && isValidPID)
    {
        result = matter;
    }
    else
    {
        free(g_steal_pointer(&matter));
    }

    return result;
}

icStringHashMap *getDenylistedUuids(const char *denylistPath)
{
    icStringHashMap *result = NULL;

    if (doesNonEmptyFileExist(denylistPath))
    {
        xmlDocPtr doc = NULL;
        xmlNodePtr topNode = NULL;

        if ((doc = xmlParseFile(denylistPath)) == NULL)
        {
            // log line used for Telemetry do not edit/delete
            //
            icLogError(LOG_TAG, "Denylist Failed to parse, for file %s", denylistPath);
            TELEMETRY_COUNTER(TELEMETRY_MARKER_BL_PARSING_ERROR);

            return result;
        }

        if ((topNode = xmlDocGetRootElement(doc)) == NULL)
        {
            // log line used for Telemetry do not edit/delete
            //
            icLogWarn(LOG_TAG, "Denylist Failed to parse, unable to find contents of %s", DDL_ROOT_NODE);
            TELEMETRY_COUNTER(TELEMETRY_MARKER_BL_PARSING_ERROR);

            xmlFreeDoc(doc);
            return result;
        }

        result = stringHashMapCreate();

        // loop through the children of ROOT
        xmlNodePtr loopNode = topNode->children;
        xmlNodePtr currNode = NULL;
        for (currNode = loopNode; currNode != NULL; currNode = currNode->next)
        {
            if (currNode->type == XML_ELEMENT_NODE && strcmp((const char *) currNode->name, UUID_NODE) == 0)
            {
                if (currNode->children == NULL)
                {
                    icLogWarn(LOG_TAG, "%s: ignoring empty uuid node", __FUNCTION__);
                    continue;
                }
                char *uuid = strdup(currNode->children->content);
                if (stringHashMapPut(result, uuid, NULL) == false)
                {
                    icLogWarn(LOG_TAG, "%s: failed to add %s", __FUNCTION__, uuid);
                    free(uuid);
                }
            }
        }

        xmlFreeDoc(doc);
    }

    return result;
}

icLinkedList *parseDeviceDescriptors(const char *allowlistPath, const char *denylistPath)
{
    icLinkedList *result = NULL;
    xmlDocPtr doc = NULL;
    xmlNodePtr topNode = NULL;

    if (doesNonEmptyFileExist(allowlistPath))
    {
        icLogDebug(LOG_TAG, "Parsing device descriptor list at %s", allowlistPath);
    }
    else
    {
        icLogWarn(LOG_TAG, "Invalid/missing device descriptor list at %s", allowlistPath);
        return result;
    }

    if ((doc = xmlParseFile(allowlistPath)) == NULL)
    {
        // log line used for Telemetry do not edit/delete
        //
        icLogError(LOG_TAG, "Allowlist Failed to parse, for file %s", allowlistPath);
        TELEMETRY_COUNTER(TELEMETRY_MARKER_WL_PARSING_ERROR);

        return result;
    }

    if ((topNode = xmlDocGetRootElement(doc)) == NULL)
    {
        // log line used for Telemetry do not edit/delete
        //
        icLogWarn(LOG_TAG, "Allowlist Failed to parse, unable to find contents of %s", DDL_ROOT_NODE);
        TELEMETRY_COUNTER(TELEMETRY_MARKER_WL_PARSING_ERROR);

        xmlFreeDoc(doc);
        return result;
    }

    // if we have a denylist, go ahead and parse it into a set of uuids
    icStringHashMap *denylistedUuids = getDenylistedUuids(denylistPath);

    // we got this far, so the input looks like something we can work with... allocate our result
    result = linkedListCreate();

    // loop through the children of ROOT
    xmlNodePtr loopNode = topNode->children;
    xmlNodePtr currNode = NULL;
    for (currNode = loopNode; currNode != NULL; currNode = currNode->next)
    {
        DeviceDescriptor *dd = NULL;

        // skip comments, blanks, etc
        if (currNode->type != XML_ELEMENT_NODE)
        {
            continue;
        }

        if (strcmp((const char *) currNode->name, CAMERA_DD_NODE) == 0)
        {
            dd = calloc(1, sizeof(CameraDeviceDescriptor));
            dd->deviceDescriptorType = DEVICE_DESCRIPTOR_TYPE_CAMERA;

            if (parseDescriptorBase(currNode, (DeviceDescriptor *) dd) == false ||
                parseCameraDescriptor(currNode, (CameraDeviceDescriptor *) dd) == false)
            {
                // log line used for Telemetry do not edit/delete
                //
                icLogError(LOG_TAG, "Allowlist Failed to parse, Camera device descriptor problem");
                TELEMETRY_COUNTER(TELEMETRY_MARKER_WL_PARSING_ERROR);

                deviceDescriptorFree((DeviceDescriptor *) dd);
                dd = NULL;
            }
        }
        else if (strcmp((const char *) currNode->name, ZIGBEE_DD_NODE) == 0)
        {
            dd = calloc(1, sizeof(ZigbeeDeviceDescriptor));
            dd->deviceDescriptorType = DEVICE_DESCRIPTOR_TYPE_ZIGBEE;

            if (parseDescriptorBase(currNode, (DeviceDescriptor *) dd) == false)
            {
                // log line used for Telemetry do not edit/delete
                //
                icLogError(LOG_TAG, "Allowlist Failed to parse, Zigbee device descriptor problem");
                TELEMETRY_COUNTER(TELEMETRY_MARKER_WL_PARSING_ERROR);

                deviceDescriptorFree((DeviceDescriptor *) dd);
                dd = NULL;
            }
        }

        if (dd != NULL)
        {
            if (denylistedUuids != NULL && stringHashMapContains(denylistedUuids, dd->uuid) == true)
            {
                icLogInfo(LOG_TAG, "%s: descriptor %s denylisted", __FUNCTION__, dd->uuid);
                deviceDescriptorFree((DeviceDescriptor *) dd);
            }
            else
            {
                linkedListAppend(result, dd);
            }
        }
    }

    stringHashMapDestroy(denylistedUuids, NULL);
    xmlFreeDoc(doc);
    return result;
}

static char *getTrimmedXmlNodeContentsAsString(xmlNodePtr node)
{
    char *result = NULL;

    char *contents = getXmlNodeContentsAsString(node, NULL);
    if (contents != NULL)
    {
        result = trimString(contents);
        free(contents);
    }

    return result;
}
