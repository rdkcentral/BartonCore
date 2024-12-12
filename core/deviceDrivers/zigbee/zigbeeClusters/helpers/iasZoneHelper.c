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


#ifdef BARTON_CONFIG_ZIGBEE

#include "zigbeeClusters/helpers/iasZoneHelper.h"
#include "commonDeviceDefs.h"
#include "zigbeeClusters/helpers/comcastBatterySavingHelper.h"
#include <device/deviceModelHelper.h>
#include <deviceServicePrivate.h>
#include <icLog/logging.h>
#include <icTime/timeUtils.h>
#include <icUtil/stringUtils.h>
#include <jsonHelper/jsonHelper.h>
#include <resourceTypes.h>
#include <string.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>
#include <zigbeeClusters/iasZoneCluster.h>

#define LOG_TAG "iasZoneHelper"

/* Private functions */
static bool fetchInitialResourceValues(icDevice *device,
                                       const char *endpoint,
                                       const char *endpointProfile,
                                       const uint8_t endpointId,
                                       icInitialResourceValues *initialResourceValues);
static bool registerResources(icDevice *device,
                              icDeviceEndpoint *endpoint,
                              const uint8_t endpointId,
                              icInitialResourceValues *initialResourceValues);

static bool fetchInitialSensorResourceValues(icDevice *device,
                                             const char *epName,
                                             const uint16_t zoneStatus,
                                             const uint16_t zoneType,
                                             icInitialResourceValues *initialResourceValues);
static bool
registerSensorResources(icDevice *device, icDeviceEndpoint *endpoint, icInitialResourceValues *initialResourceValues);

static const char *getProfileForDeviceClass(const char *deviceClass);
static const char *getTamperResourceForProfile(const char *profileName);
static const char *getTypeResourceForProfile(const char *profileName);
static uint8_t getSensorTypeModeForDeviceClass(const char *deviceClass);

static inline cJSON *createTestFaultMetadata(bool test);

static inline cJSON *createTestFaultMetadata(bool test)
{
    cJSON *metadata = cJSON_CreateObject();
    cJSON_AddBoolToObject(metadata, SENSOR_PROFILE_METADATA_TEST, test);
    return metadata;
}

void iasZoneStatusChangedHelper(uint64_t eui64, uint8_t endpointId, uint16_t zoneStatus, const ZigbeeDriverCommon *ctx)
{
    AUTO_CLEAN(free_generic__auto) const char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);
    AUTO_CLEAN(free_generic__auto) const char *endpointName = zigbeeSubsystemEndpointIdAsString(endpointId);

    AUTO_CLEAN(resourceDestroy__auto)
    icDeviceResource *batteryLow = deviceServiceGetResourceById(deviceUuid, NULL, COMMON_DEVICE_RESOURCE_BATTERY_LOW);
    AUTO_CLEAN(endpointDestroy__auto) icDeviceEndpoint *ep = deviceServiceGetEndpointById(deviceUuid, endpointName);
    AUTO_CLEAN(deviceDestroy__auto) icDevice *device = deviceServiceGetDevice(deviceUuid);
    const bool isBatteryPowered = batteryLow != NULL;
    const char *endpointType = NULL;
    char valStr[21]; // Largest output: 20-char millisecond timestamp + \0

    if (!ep)
    {
        icLogWarn(LOG_TAG, "%s: Unable to get endpoint %s/%s", __FUNCTION__, deviceUuid, endpointName);
        return;
    }

    if (strcmp(ep->profile, SENSOR_PROFILE) == 0)
    {
        AUTO_CLEAN(resourceDestroy__auto)
        icDeviceResource *zoneTypeResource =
            deviceServiceGetResourceById(deviceUuid, endpointName, SENSOR_PROFILE_RESOURCE_TYPE);
        if (zoneTypeResource)
        {
            endpointType = zoneTypeResource->value;
        }
        else
        {
            icLogWarn(LOG_TAG,
                      "%s: unable to get resource %s on device %s.%s",
                      __FUNCTION__,
                      SENSOR_PROFILE_RESOURCE_TYPE,
                      deviceUuid,
                      endpointName);
        }

        const char *troubleResourceId = iasZoneHelperGetTroubleResource(device->deviceClass, endpointType);
        if (troubleResourceId != NULL)
        {
            updateResource(deviceUuid,
                           endpointName,
                           troubleResourceId,
                           (zoneStatus & (uint16_t) IAS_ZONE_STATUS_TROUBLE) != 0 ? "true" : "false",
                           NULL);
        }

        // check if we need to handle the test fault or test fault restore.
        // fill the metadata with required value.
        //
        cJSON *resourceFaultedMetadataJson = NULL;

        if ((zoneStatus & (uint16_t) IAS_ZONE_STATUS_TEST) != 0)
        {
            if ((zoneStatus & (uint16_t) IAS_ZONE_STATUS_ALARM1) != 0)
            {
                resourceFaultedMetadataJson = createTestFaultMetadata(true);
            }
            else
            {
                resourceFaultedMetadataJson = createTestFaultMetadata(false);
            }
        }
        updateResource(deviceUuid,
                       endpointName,
                       SENSOR_PROFILE_RESOURCE_FAULTED,
                       stringValueOfBool((zoneStatus & (uint16_t) IAS_ZONE_STATUS_ALARM1) != 0),
                       resourceFaultedMetadataJson);

        cJSON_Delete(resourceFaultedMetadataJson);
    }

    updateResource(deviceUuid,
                   endpointName,
                   getTamperResourceForProfile(ep->profile),
                   (zoneStatus & IAS_ZONE_STATUS_TAMPER) != 0 ? "true" : "false",
                   NULL);

    // For battery backed up devices, we don't get their low battery status from ias zone information
    if (isBatteryPowered && zigbeeDriverCommonIsBatteryBackedUp(ctx) == false)
    {
        updateResource(deviceUuid,
                       NULL,
                       COMMON_DEVICE_RESOURCE_BATTERY_LOW,
                       (zoneStatus & (uint16_t) IAS_ZONE_STATUS_BATTERY_LOW) != 0 ? "true" : "false",
                       NULL);
    }

    snprintf(valStr, sizeof(valStr), "%" PRIu64, getCurrentUnixTimeMillis());
    updateResource(deviceUuid, NULL, COMMON_DEVICE_RESOURCE_LAST_USER_INTERACTION_DATE, valStr, NULL);
}

/**
 * Driver helper for fetching intial zone resource values
 * @param device
 * @param endpoint If NULL, any endpoints that have an IAS Zone server cluster will have resource values create.
 *                 Otherwise, resources values will be create on this endpoint if it has an IAS Zone server cluster.
 * @param endpointProfile if endpoint is not NULL, the profile for the endpoint.  Otherwise the endpoint profile will
 *                        be derived from the device class
 * @param endpointId the device endpoint id
 * @param discoveredDeviceDetails
 * @param initialResourceValues
 */
bool iasZoneFetchInitialResourceValues(icDevice *device,
                                       const char *endpoint,
                                       const char *endpointProfile,
                                       uint8_t endpointId,
                                       IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                                       icInitialResourceValues *initialResourceValues)
{
    bool result = true;
    bool foundOne = false;

    icLogDebug(LOG_TAG, "%s: uuid=%s", __FUNCTION__, device->uuid);

    if (endpoint != NULL)
    {
        if (icDiscoveredDeviceDetailsEndpointHasCluster(discoveredDeviceDetails, endpointId, IAS_ZONE_CLUSTER_ID, true))
        {
            foundOne = result =
                fetchInitialResourceValues(device, endpoint, endpointProfile, endpointId, initialResourceValues);
        }
    }
    else
    {
        for (int i = 0; i < discoveredDeviceDetails->numEndpoints; i++)
        {
            IcDiscoveredEndpointDetails endpointDetails = discoveredDeviceDetails->endpointDetails[i];
            if (icDiscoveredDeviceDetailsEndpointHasCluster(
                    discoveredDeviceDetails, endpointDetails.endpointId, IAS_ZONE_CLUSTER_ID, true))
            {
                const char *profileName = getProfileForDeviceClass(device->deviceClass);
                AUTO_CLEAN(free_generic__auto)
                const char *epName = zigbeeSubsystemEndpointIdAsString(endpointDetails.endpointId);

                result &= fetchInitialResourceValues(
                    device, epName, profileName, endpointDetails.endpointId, initialResourceValues);
                foundOne |= result;
            }
        }
    }

    initialResourceValuesPutDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_LAST_USER_INTERACTION_DATE, NULL);

    return result && foundOne;
}

/**
 * Driver helper for registering zone resources
 * @param device
 * @param endpoint If NULL, an endpoint will be created on any endpoints that have an IAS Zone server cluster.
 *                 Otherwise, resources will be registered on this endpoint if it has an IAS Zone server cluster.
 * @param endpointId the device endpoint id
 * @param discoveredDeviceDetails
 * @param initialResourceValues
 */
bool iasZoneRegisterResources(icDevice *device,
                              icDeviceEndpoint *endpoint,
                              uint8_t endpointId,
                              IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                              icInitialResourceValues *initialResourceValues)
{
    bool registered = true;
    bool registeredOne = false;

    icLogDebug(LOG_TAG, "%s: uuid=%s", __FUNCTION__, device->uuid);

    if (endpoint != NULL)
    {
        if (icDiscoveredDeviceDetailsEndpointHasCluster(discoveredDeviceDetails, endpointId, IAS_ZONE_CLUSTER_ID, true))
        {
            registeredOne = registered = registerResources(device, endpoint, endpointId, initialResourceValues);
        }
    }
    else
    {
        for (int i = 0; i < discoveredDeviceDetails->numEndpoints; i++)
        {
            IcDiscoveredEndpointDetails endpointDetails = discoveredDeviceDetails->endpointDetails[i];
            if (icDiscoveredDeviceDetailsEndpointHasCluster(
                    discoveredDeviceDetails, endpointDetails.endpointId, IAS_ZONE_CLUSTER_ID, true))
            {
                const char *profileName = getProfileForDeviceClass(device->deviceClass);
                AUTO_CLEAN(free_generic__auto)
                const char *epName = zigbeeSubsystemEndpointIdAsString(endpointDetails.endpointId);

                endpoint = createEndpoint(device, epName, profileName, true);
                registered &= registerResources(device, endpoint, endpointDetails.endpointId, initialResourceValues);
                registeredOne |= registered;

                if (registered)
                {
                    zigbeeDriverCommonSetEndpointNumber(endpoint, endpointDetails.endpointId);
                }
            }
        }
    }

    return registered && registeredOne;
}

static bool fetchInitialResourceValues(icDevice *device,
                                       const char *endpoint,
                                       const char *endpointProfile,
                                       const uint8_t endpointId,
                                       icInitialResourceValues *initialResourceValues)
{
    uint64_t tmp = 0;
    uint16_t zoneStatus = 0;
    uint16_t zoneType = 0;
    uint64_t eui64 = zigbeeSubsystemIdToEui64(device->uuid);

    bool registered = true;
    bool isSensor = strcmp(endpointProfile, SENSOR_PROFILE) == 0;

    icLogDebug(LOG_TAG, "Fetch initial values %s on endpoint id %s", endpointProfile, endpoint);

    if (zigbeeSubsystemReadNumber(eui64, endpointId, IAS_ZONE_CLUSTER_ID, true, IAS_ZONE_STATUS_ATTRIBUTE_ID, &tmp) !=
        0)
    {
        icLogError(LOG_TAG, "Unable to read zone status from %" PRIu64 "%s", eui64, endpoint);
        return false;
    }
    zoneStatus = (uint16_t) tmp;

    if (zigbeeSubsystemReadNumber(eui64, endpointId, IAS_ZONE_CLUSTER_ID, true, IAS_ZONE_TYPE_ATTRIBUTE_ID, &tmp) != 0)
    {
        icLogError(LOG_TAG, "Unable to read zone type from %" PRIu64 "%s", eui64, endpoint);
        return false;
    }

    zoneType = (uint16_t) tmp;

    if (isSensor)
    {
        registered = fetchInitialSensorResourceValues(device, endpoint, zoneStatus, zoneType, initialResourceValues);
    }
    const char *zoneTypeName = iasZoneHelperGetZoneTypeName(zoneType);
    initialResourceValuesPutEndpointValue(
        initialResourceValues, endpoint, getTypeResourceForProfile(endpointProfile), zoneTypeName);
    initialResourceValuesPutEndpointValue(initialResourceValues,
                                          endpoint,
                                          getTamperResourceForProfile(endpointProfile),
                                          (zoneStatus & IAS_ZONE_STATUS_TAMPER) != 0 ? "true" : "false");

    return registered;
}

void iasZoneHelperSyncZoneStatus(const char *uuid, const uint8_t endpointId, ZigbeeDriverCommon *ctx)
{
    uint64_t tmp = 0;
    uint16_t zoneStatus = 0;
    uint64_t eui64 = zigbeeSubsystemIdToEui64(uuid);
    if (zigbeeSubsystemReadNumber(eui64, endpointId, IAS_ZONE_CLUSTER_ID, true, IAS_ZONE_STATUS_ATTRIBUTE_ID, &tmp) !=
        0)
    {
        icLogError(LOG_TAG, "Unable to read zone status from %" PRIu64, eui64);
        return;
    }

    zoneStatus = (uint16_t) tmp;
    iasZoneStatusChangedHelper(eui64, endpointId, zoneStatus, ctx);
}

static bool registerResources(icDevice *device,
                              icDeviceEndpoint *endpoint,
                              const uint8_t endpointId,
                              icInitialResourceValues *initialResourceValues)
{
    bool registered = true;
    bool isSensor = strcmp(endpoint->profile, SENSOR_PROFILE) == 0;

    icLogDebug(LOG_TAG, "Registering %s resources on endpoint id %s", endpoint->profile, endpoint->id);

    if (isSensor)
    {
        registered = registerSensorResources(device, endpoint, initialResourceValues);
    }

    uint8_t sensorTypeMode = getSensorTypeModeForDeviceClass(device->deviceClass);

    const char *resourceType = RESOURCE_TYPE_SENSOR_TYPE;
    if (strcmp(endpoint->profile, SECURITY_CONTROLLER_PROFILE) == 0)
    {
        resourceType = RESOURCE_TYPE_SECURITY_CONTROLLER_TYPE;

        // need to set the LPM flag for Keypads and Keyfobs
        // ZILKER-700
        //
        if (createDeviceMetadata(device, LPM_POLICY_METADATA, lpmPolicyPriorityLabels[LPM_POLICY_ALWAYS]) == NULL)
        {
            icLogError(LOG_TAG, "Unable to register device metadata %s", LPM_POLICY_METADATA);
        }
    }

    registered &= createEndpointResourceIfAvailable(endpoint,
                                                    getTypeResourceForProfile(endpoint->profile),
                                                    initialResourceValues,
                                                    resourceType,
                                                    sensorTypeMode,
                                                    CACHING_POLICY_ALWAYS) != NULL;

    registered &=
        createEndpointResourceIfAvailable(endpoint,
                                          getTamperResourceForProfile(endpoint->profile),
                                          initialResourceValues,
                                          RESOURCE_TYPE_BOOLEAN,
                                          RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_EMIT_EVENTS,
                                          CACHING_POLICY_ALWAYS) != NULL;

    return registered;
}

static bool fetchInitialSensorResourceValues(icDevice *device,
                                             const char *epName,
                                             const uint16_t zoneStatus,
                                             const uint16_t zoneType,
                                             icInitialResourceValues *initialResourceValues)
{
    const char *troubleResourceId =
        iasZoneHelperGetTroubleResource(device->deviceClass, iasZoneHelperGetZoneTypeName(zoneType));
    if (troubleResourceId != NULL)
    {
        initialResourceValuesPutEndpointValue(initialResourceValues,
                                              epName,
                                              troubleResourceId,
                                              (zoneStatus & IAS_ZONE_STATUS_TROUBLE) != 0 ? "true" : "false");
    }

    initialResourceValuesPutEndpointValue(initialResourceValues,
                                          epName,
                                          SENSOR_PROFILE_RESOURCE_FAULTED,
                                          (zoneStatus & IAS_ZONE_STATUS_ALARM1) != 0 ? "true" : "false");

    initialResourceValuesPutEndpointValue(initialResourceValues, epName, SENSOR_PROFILE_RESOURCE_QUALIFIED, "true");
    initialResourceValuesPutEndpointValue(initialResourceValues, epName, SENSOR_PROFILE_RESOURCE_BYPASSED, "false");

    // TODO: motion types' sensitivity
    return true;
}

static bool
registerSensorResources(icDevice *device, icDeviceEndpoint *endpoint, icInitialResourceValues *initialResourceValues)
{

    bool registered = true;

    // These are optional
    createEndpointResourceIfAvailable(endpoint,
                                      SENSOR_PROFILE_RESOURCE_END_OF_LINE_FAULT,
                                      initialResourceValues,
                                      RESOURCE_TYPE_BOOLEAN,
                                      RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_EMIT_EVENTS,
                                      CACHING_POLICY_ALWAYS);

    createEndpointResourceIfAvailable(endpoint,
                                      SENSOR_PROFILE_RESOURCE_DIRTY,
                                      initialResourceValues,
                                      RESOURCE_TYPE_BOOLEAN,
                                      RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_EMIT_EVENTS,
                                      CACHING_POLICY_ALWAYS);

    createEndpointResourceIfAvailable(endpoint,
                                      SENSOR_PROFILE_RESOURCE_END_OF_LIFE,
                                      initialResourceValues,
                                      RESOURCE_TYPE_BOOLEAN,
                                      RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_EMIT_EVENTS,
                                      CACHING_POLICY_ALWAYS);

    registered &=
        createEndpointResourceIfAvailable(endpoint,
                                          SENSOR_PROFILE_RESOURCE_FAULTED,
                                          initialResourceValues,
                                          RESOURCE_TYPE_BOOLEAN,
                                          RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_EMIT_EVENTS,
                                          CACHING_POLICY_ALWAYS) != NULL;

    registered &= createEndpointResourceIfAvailable(endpoint,
                                                    SENSOR_PROFILE_RESOURCE_QUALIFIED,
                                                    initialResourceValues,
                                                    RESOURCE_TYPE_BOOLEAN,
                                                    RESOURCE_MODE_READABLE,
                                                    CACHING_POLICY_ALWAYS) != NULL;

    registered &= createEndpointResourceIfAvailable(endpoint,
                                                    SENSOR_PROFILE_RESOURCE_BYPASSED,
                                                    initialResourceValues,
                                                    RESOURCE_TYPE_BOOLEAN,
                                                    RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_EMIT_EVENTS,
                                                    CACHING_POLICY_ALWAYS) != NULL;

    // TODO: motion types' sensitivity
    return registered;
}

const char *iasZoneHelperGetZoneTypeName(uint16_t zoneType)
{
    const char *zoneTypeName = NULL;

    switch (zoneType)
    {
        case IAS_ZONE_TYPE_CONTACT_SWITCH:
            zoneTypeName = SENSOR_PROFILE_CONTACT_SWITCH_TYPE;
            break;
        case IAS_ZONE_TYPE_MOTION_SENSOR:
            zoneTypeName = SENSOR_PROFILE_MOTION_TYPE;
            break;
        case IAS_ZONE_TYPE_FIRE_SENSOR:
            zoneTypeName = SENSOR_PROFILE_SMOKE;
            break;
        case IAS_ZONE_TYPE_WATER_SENSOR:
            zoneTypeName = SENSOR_PROFILE_WATER;
            break;
        case IAS_ZONE_TYPE_CO_SENSOR:
            zoneTypeName = SENSOR_PROFILE_CO;
            break;
        case IAS_ZONE_TYPE_PERSONAL_EMERGENCY_DEVICE:
            zoneTypeName = SENSOR_PROFILE_PERSONAL_EMERGENCY;
            break;
        case IAS_ZONE_TYPE_VIBRATION_MOVEMENT_SENSOR:
            zoneTypeName = SENSOR_PROFILE_VIBRATION;
            break;
        case IAS_ZONE_TYPE_REMOTE_CONTROL:
            zoneTypeName = SENSOR_PROFILE_REMOTE_CONTROL;
            break;
        case IAS_ZONE_TYPE_GLASS_BREAK_SENSOR:
            zoneTypeName = SENSOR_PROFILE_GLASS_BREAK;
            break;
        case IAS_ZONE_TYPE_KEYPAD:
            zoneTypeName = SECURITY_CONTROLLER_PROFILE_KEYPAD_TYPE;
            break;
        case IAS_ZONE_TYPE_KEYFOB:
            zoneTypeName = SECURITY_CONTROLLER_PROFILE_KEYFOB_TYPE;
            break;
        default:
            icLogWarn(LOG_TAG, "Unknown IAS zone type %" PRIx16, zoneType);
            zoneTypeName = SENSOR_PROFILE_UNKNOWN_TYPE;
            break;
    }

    return zoneTypeName;
}

/* TODO: This belongs to the profile */
const char *iasZoneHelperGetTroubleResource(const char *deviceClass, const char *zoneTypeName)
{
    const char *troubleResource = NULL;
    if (strcmp(deviceClass, PRM_DC) == 0 || strcmp(deviceClass, PIM_DC) == 0)
    {
        troubleResource = SENSOR_PROFILE_RESOURCE_END_OF_LINE_FAULT;
    }
    else
    {
        if (zoneTypeName == NULL)
        {
            icLogWarn(LOG_TAG, "%s: received NULL endpoint type!", __FUNCTION__);
        }
        else if (strcmp(zoneTypeName, SENSOR_PROFILE_SMOKE) == 0)
        {
            troubleResource = SENSOR_PROFILE_RESOURCE_DIRTY;
        }
        else if (strcmp(zoneTypeName, SENSOR_PROFILE_CO) == 0)
        {
            troubleResource = SENSOR_PROFILE_RESOURCE_END_OF_LIFE;
        }
        // Otherwise there is no trouble resource, don't log or every regular zigbee d/w sensor fault would log
    }

    return troubleResource;
}

static const char *getProfileForDeviceClass(const char *deviceClass)
{
    const char *profileName = NULL;
    if (strcmp(deviceClass, KEYPAD_DC) == 0 || strcmp(deviceClass, KEYFOB_DC) == 0)
    {
        profileName = SECURITY_CONTROLLER_PROFILE;
    }
    else if (strcmp(deviceClass, SENSOR_DC) == 0 || strcmp(deviceClass, PRM_DC))
    {
        profileName = SENSOR_PROFILE;
    }
    else
    {
        icLogError(LOG_TAG, "Device class %s not supported", deviceClass);
    }

    return profileName;
}

/*
 * TODO: Replace this with proper profiles
 */
static const char *getTamperResourceForProfile(const char *profileName)
{
    const char *tamperResName = NULL;
    if (strcmp(profileName, SENSOR_PROFILE) == 0)
    {
        tamperResName = SENSOR_PROFILE_RESOURCE_TAMPERED;
    }
    else if (strcmp(profileName, SECURITY_CONTROLLER_PROFILE) == 0)
    {
        tamperResName = SECURITY_CONTROLLER_PROFILE_RESOURCE_TAMPERED;
    }

    return tamperResName;
}

static const char *getTypeResourceForProfile(const char *profileName)
{
    const char *tamperResName = NULL;
    if (strcmp(profileName, SENSOR_PROFILE) == 0)
    {
        tamperResName = SENSOR_PROFILE_RESOURCE_TYPE;
    }
    else if (strcmp(profileName, SECURITY_CONTROLLER_PROFILE) == 0)
    {
        tamperResName = SECURITY_CONTROLLER_PROFILE_RESOURCE_TYPE;
    }

    return tamperResName;
}

static uint8_t getSensorTypeModeForDeviceClass(const char *deviceClass)
{
    if (strcmp(deviceClass, PRM_DC) == 0)
    {
        return RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_EMIT_EVENTS;
    }
    else
    {
        return RESOURCE_MODE_READABLE;
    }
}

#endif // BARTON_CONFIG_ZIGBEE
