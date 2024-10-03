//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2019 Comcast Corporation
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
// Comcast Corporation retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------
//

#include "device-driver/device-driver-manager.h"
#include "device/deviceModelHelper.h"
#include "deviceDrivers/zigbeeDriverCommon.h"
#include "deviceService.h"
#include "deviceService/resourceModes.h"
#include "resourceTypes.h"
#include "subsystems/zigbee/zigbeeEventTracker.h"
#include <commonDeviceDefs.h>
#include <device/icDeviceResource.h>
#include <deviceDescriptors.h>
#include <deviceServicePrivate.h>
#include <errno.h>
#include <icUtil/array.h>
#include <jsonHelper/jsonHelper.h>
#include <math.h>
#include <memory.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>
#include <subsystems/zigbee/zigbeeIO.h>
#include <zigbeeClusters/helpers/comcastBatterySavingHelper.h>
#include <zigbeeClusters/helpers/iasZoneHelper.h>
#include <zigbeeClusters/iasZoneCluster.h>
#include <zigbeeClusters/pollControlCluster.h>
#include <zigbeeClusters/powerConfigurationCluster.h>

#define LOG_TAG     "ZigBeeSensorDD"
#define logFmt(fmt) "%s: " fmt, __func__
#include <icLog/logging.h>

#define DEVICE_DRIVER_NAME        "ZigBeeSensorDD"
#define MY_DC_VERSION             1
#define MY_SENSOR_PROFILE_VERSION 2

static inline icDeviceResource *createMagFieldStrengthResource(icDeviceEndpoint *endpoint)
{
    return createEndpointResource(endpoint,
                                  SENSOR_PROFILE_RESOURCE_MAGNETIC_FIELD_STRENGTH,
                                  NULL,
                                  RESOURCE_TYPE_MAGNETIC_FIELD_STRENGTH,
                                  RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_EMIT_EVENTS |
                                      RESOURCE_MODE_LAZY_SAVE_NEXT,
                                  CACHING_POLICY_ALWAYS);
}

static inline icDeviceResource *createMagFieldHealthResource(icDeviceEndpoint *endpoint)
{
    return createEndpointResource(endpoint,
                                  SENSOR_PROFILE_RESOURCE_MAGNETIC_FIELD_HEALTH,
                                  NULL,
                                  RESOURCE_TYPE_STRING,
                                  RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_LAZY_SAVE_NEXT,
                                  CACHING_POLICY_ALWAYS);
}

/* ZigbeeDriverCommonCallbacks */
static bool
getDiscoveredDeviceMetadata(ZigbeeDriverCommon *ctx, IcDiscoveredDeviceDetails *details, icStringHashMap *metadata);
static void processDeviceDescriptorMetadata(ZigbeeDriverCommon *ctx, icDevice *device, icStringHashMap *metadata);
static const char *mapDeviceIdToProfile(ZigbeeDriverCommon *ctx, uint16_t deviceId);
static bool
preConfigureCluster(ZigbeeDriverCommon *ctx, ZigbeeCluster *cluster, DeviceConfigurationContext *deviceConfigContext);

static bool fetchInitialResourceValues(ZigbeeDriverCommon *ctx,
                                       icDevice *device,
                                       IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                                       icInitialResourceValues *initialResourceValues);

static void handlePollControlCheckin(uint64_t eui64,
                                     uint8_t endpointId,
                                     const ComcastBatterySavingData *batterySavingData,
                                     const void *ctx);

static bool registerResources(ZigbeeDriverCommon *ctx,
                              icDevice *device,
                              IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                              icInitialResourceValues *initialResourceValues);

/* IASZoneClusterCallbacks */
static void onZoneStatusChanged(uint64_t eui64,
                                uint8_t endpointId,
                                const IASZoneStatusChangedNotification *notification,
                                const ComcastBatterySavingData *batterySavingData,
                                const void *driverCtx);
/* Magnetic field strength */
static inline bool isMagneticFieldStrengthSupported(const char *deviceUuid);
static inline char *batterySavingDataGetMagneticStrengthMilliString(uint16_t magStrengthMicroTesla);
static inline const char *batterySavingDataMagneticFieldStrengthToHealth(uint16_t magStrengthMicroTesla);
static bool isZoneClosed(const char *deviceUuid);
static bool createMagneticFieldResources(icDevice *device, bool isPairing);
static void
updateMagneticFieldResources(const char *deviceUuid, uint8_t, const ComcastBatterySavingData *batterySavingData);

// Ranges of magnetic field strength defined for magnetic field health
#define MAGNETIC_FIELD_POOR_HEALTH_MIN 0
#define MAGNETIC_FIELD_FAIR_HEALTH_MIN 7000
#define MAGNETIC_FIELD_GOOD_HEALTH_MIN 10000
#define MAGNETIC_FIELD_GOOD_HEALTH_MAX 21460

static const uint16_t myDeviceIds[] = {SENSOR_DEVICE_ID};

// zigbee device driver registration order matters, so we pick constructor priority carefully
__attribute__((constructor(260))) static void driverRegister(void)
{
    static const ZigbeeDriverCommonCallbacks myHooks = {.fetchInitialResourceValues = fetchInitialResourceValues,
                                                        .registerResources = registerResources,
                                                        .mapDeviceIdToProfile = mapDeviceIdToProfile,
                                                        .getDiscoveredDeviceMetadata = getDiscoveredDeviceMetadata,
                                                        .processDeviceDescriptorMetadata =
                                                            processDeviceDescriptorMetadata,
                                                        .preConfigureCluster = preConfigureCluster};

    static const IASZoneClusterCallbacks iasZoneClusterCallbacks = {
        .onZoneEnrollRequested = NULL,
        .onZoneStatusChanged = onZoneStatusChanged,
        .processComcastBatterySavingData = zigbeeDriverCommonComcastBatterySavingUpdateResources};

    static const PollControlClusterCallbacks pollControlClusterCallbacks = {.checkin = handlePollControlCheckin};

    DeviceDriver *myDriver = zigbeeDriverCommonCreateDeviceDriver(DEVICE_DRIVER_NAME,
                                                                  SENSOR_DC,
                                                                  MY_DC_VERSION,
                                                                  myDeviceIds,
                                                                  ARRAY_LENGTH(myDeviceIds),
                                                                  RX_MODE_SLEEPY,
                                                                  &myHooks,
                                                                  false);

    DRIVER_REGISTER_PROFILE_VERSION(myDriver, SENSOR_PROFILE, MY_SENSOR_PROFILE_VERSION);

    zigbeeDriverCommonAddCluster(myDriver, iasZoneClusterCreate(&iasZoneClusterCallbacks, myDriver));

    ZigbeeCluster *cluster = zigbeeDriverCommonGetCluster((ZigbeeDriverCommon *) myDriver, POLL_CONTROL_CLUSTER_ID);
    pollControlClusterAddCallback(cluster, &pollControlClusterCallbacks, myDriver);

    deviceDriverManagerRegisterDriver(myDriver);
}

static bool fetchInitialResourceValues(ZigbeeDriverCommon *ctx,
                                       icDevice *device,
                                       IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                                       icInitialResourceValues *initialResourceValues)
{
    return iasZoneFetchInitialResourceValues(device, NULL, NULL, 0, discoveredDeviceDetails, initialResourceValues);
}

static bool registerResources(ZigbeeDriverCommon *ctx,
                              icDevice *device,
                              IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                              icInitialResourceValues *initialResourceValues)
{
    bool registered = true;
    const char *optionalDataType = NULL;

    registered = iasZoneRegisterResources(device, NULL, 0, discoveredDeviceDetails, initialResourceValues);

    if (registered)
    {
        registered = endpointsSetProfileVersion(device->endpoints, MY_SENSOR_PROFILE_VERSION);
    }

    if (registered && (optionalDataType = deviceGetMetadata(device, OPTIONAL_SENSOR_DATA_TYPE_METADATA)) != NULL)
    {
        if (stringCompare(optionalDataType, OPTIONAL_SENSOR_DATA_TYPE_MAGNETIC, false) == 0)
        {
            registered = createMagneticFieldResources(device, true);
        }
    }

    return registered;
}

static void onZoneStatusChanged(uint64_t eui64,
                                uint8_t endpointId,
                                const IASZoneStatusChangedNotification *notification,
                                const ComcastBatterySavingData *batterySavingData,
                                const void *driverCtx)
{
    // if this message was delayed, lets print some details that telemetry could pick up on
    if (notification->delayQS > 0)
    {
        icLogInfo(LOG_TAG,
                  "%s: delayed status received: delay=%" PRIu16 "s, retries=%" PRIu32 ", status=0x%" PRIx16,
                  __func__,
                  notification->delayQS / 4, /* units are quarter seconds */
                  batterySavingData != NULL ? batterySavingData->retries : 0,
                  notification->zoneStatus);
    }

    iasZoneStatusChangedHelper(eui64, endpointId, notification->zoneStatus, driverCtx);

    if (batterySavingData)
    {
        scoped_generic char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);
        if (isMagneticFieldStrengthSupported(deviceUuid) && isZoneClosed(deviceUuid))
        {
            updateMagneticFieldResources(deviceUuid, endpointId, batterySavingData);
        }
    }
}

static bool
getDiscoveredDeviceMetadata(ZigbeeDriverCommon *ctx, IcDiscoveredDeviceDetails *details, icStringHashMap *metadata)
{
    bool ok = true;

    for (int i = 0; i < details->numEndpoints; i++)
    {
        uint8_t endpointId = details->endpointDetails[i].endpointId;
        if (icDiscoveredDeviceDetailsEndpointHasCluster(details, endpointId, IAS_ZONE_CLUSTER_ID, true))
        {
            AUTO_CLEAN(cJSON_Delete__auto) cJSON *endpoints = cJSON_CreateArray();

            cJSON_AddItemToArray(endpoints, cJSON_CreateNumber(endpointId));
            if (!stringHashMapPut(metadata, strdup(SENSOR_PROFILE_ENDPOINT_ID_LIST), cJSON_PrintUnformatted(endpoints)))
            {
                icLogWarn(LOG_TAG, "%s: Unable to write sensor zone endpoint number", __FUNCTION__);
                ok = false;
            }
            break;
        }
    }

    if (!stringHashMapPut(metadata, strdup(SENSOR_PROFILE_RESOURCE_QUALIFIED), strdup("true")))
    {
        icLogWarn(LOG_TAG, "%s: Unable to write sensor qualified flag", __FUNCTION__);
        ok = false;
    }

    return ok;
}

static void processDeviceDescriptorMetadata(ZigbeeDriverCommon *ctx, icDevice *device, icStringHashMap *metadata)
{
    const char *optionalDataType = NULL;
    bool magResourcesRequired = false;

    if ((optionalDataType = stringHashMapGet(metadata, OPTIONAL_SENSOR_DATA_TYPE_METADATA)) != NULL)
    {
        magResourcesRequired = stringCompare(optionalDataType, OPTIONAL_SENSOR_DATA_TYPE_MAGNETIC, false) == 0;
    }

    if (magResourcesRequired)
    {
        if (!createMagneticFieldResources(device, false))
        {
            icLogError(
                LOG_TAG, "%s: Failed to create magnetic field resources required due to metadata updates!", __func__);
        }
    }
    else
    {
        // metadata was updated and the magnetic field strength resources are not required.  Check to see if we
        // previously created
        //  them and if so, set the related values to NULL.  Note we have to do this for all endpoints since it could be
        //  on multiple.
        scoped_icLinkedListIterator *iterator = linkedListIteratorCreate(device->endpoints);
        while (linkedListIteratorHasNext(iterator))
        {
            icDeviceEndpoint *endpoint = (icDeviceEndpoint *) linkedListIteratorGetNext(iterator);
            scoped_icDeviceResource *strength = deviceServiceGetResourceById(
                device->uuid, endpoint->id, SENSOR_PROFILE_RESOURCE_MAGNETIC_FIELD_STRENGTH);
            scoped_icDeviceResource *health =
                deviceServiceGetResourceById(device->uuid, endpoint->id, SENSOR_PROFILE_RESOURCE_MAGNETIC_FIELD_HEALTH);

            if (strength || health)
            {
                icLogInfo(LOG_TAG, "%s: clearing magnetic field resources since they are no longer needed", __func__);
            }

            if (strength)
            {
                updateResource(device->uuid, endpoint->id, SENSOR_PROFILE_RESOURCE_MAGNETIC_FIELD_STRENGTH, NULL, NULL);
            }

            if (health)
            {
                updateResource(device->uuid, endpoint->id, SENSOR_PROFILE_RESOURCE_MAGNETIC_FIELD_HEALTH, NULL, NULL);
            }
        }
    }
}

static bool
preConfigureCluster(ZigbeeDriverCommon *ctx, ZigbeeCluster *cluster, DeviceConfigurationContext *deviceConfigContext)
{
    if (cluster->clusterId == POWER_CONFIGURATION_CLUSTER_ID)
    {
        powerConfigurationClusterSetConfigureBatteryAlarmState(deviceConfigContext, false);
    }

    if (cluster->clusterId == POLL_CONTROL_CLUSTER_ID)
    {
        // 5 * 60 * 4 == 5 minutes in quarter seconds
        stringHashMapPutCopy(deviceConfigContext->configurationMetadata, LONG_POLL_INTERVAL_QS_METADATA, "1200");

        // 2 == half second in quarter seconds
        stringHashMapPutCopy(deviceConfigContext->configurationMetadata, SHORT_POLL_INTERVAL_QS_METADATA, "2");

        // 10 * 4 == 10 seconds in quarter seconds
        stringHashMapPutCopy(deviceConfigContext->configurationMetadata, FAST_POLL_TIMEOUT_QS_METADATA, "40");

        // 27 * 60 * 4 == 27 minutes in quarter seconds
        stringHashMapPutCopy(deviceConfigContext->configurationMetadata, CHECK_IN_INTERVAL_QS_METADATA, "6480");
    }

    return true;
}

static const char *mapDeviceIdToProfile(ZigbeeDriverCommon *ctx, uint16_t deviceId)
{
    const char *profile = NULL;

    switch (deviceId)
    {
        case SENSOR_DEVICE_ID:
            profile = SENSOR_PROFILE;
            break;
        default:
            break;
    }

    return profile;
}

static bool isZoneClosed(const char *deviceUuid)
{
    bool isClosed = false;
    scoped_icDeviceResource *zoneFaultedResource = NULL;
    scoped_icDevice *device = deviceServiceGetDevice(deviceUuid);

    scoped_icLinkedListIterator *iterator = linkedListIteratorCreate(device->endpoints);
    while (linkedListIteratorHasNext(iterator) && zoneFaultedResource == NULL)
    {
        icDeviceEndpoint *endpoint = (icDeviceEndpoint *) linkedListIteratorGetNext(iterator);
        zoneFaultedResource = deviceServiceGetResourceById(deviceUuid, endpoint->id, SENSOR_PROFILE_RESOURCE_FAULTED);
    }

    if (zoneFaultedResource == NULL)
    {
        icLogError(LOG_TAG, "%s: unable to get sensor zone status uuid: %s", __func__, deviceUuid);
    }
    else
    {
        isClosed = !stringToBool(zoneFaultedResource->value);
    }

    return isClosed;
}

static void handlePollControlCheckin(uint64_t eui64,
                                     uint8_t endpointId,
                                     const ComcastBatterySavingData *batterySavingData,
                                     const void *ctx)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    scoped_generic char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);

    if (batterySavingData)
    {
        if (isMagneticFieldStrengthSupported(deviceUuid) && isZoneClosed(deviceUuid))
        {
            scoped_icDeviceResource *modelResource =
                deviceServiceGetResourceById(deviceUuid, NULL, COMMON_DEVICE_RESOURCE_MODEL);
            if (modelResource != NULL && modelResource->value != NULL)
            {
                scoped_generic char *magStrMilliTeslaString =
                    batterySavingDataGetMagneticStrengthMilliString(batterySavingData->optionalData.magStrMicroTesla);
                char *markerValue = stringBuilder("%s,%s", modelResource->value, magStrMilliTeslaString);
                zigbeeEventTrackerAddMagneticStrengthReport(deviceUuid, markerValue);
                markerValue = NULL;
            }
        }
    }
}

static inline char *batterySavingDataGetMagneticStrengthMilliString(uint16_t magStrengthMicroTesla)
{
    // convert micro Tesla to milli Tesla string
    return stringBuilder("%d.%03d", magStrengthMicroTesla / 1000, magStrengthMicroTesla % 1000);
}

static inline bool isMagneticFieldStrengthSupported(const char *deviceUuid)
{
    // only driver specific optional data is handled here
    scoped_generic char *optionalDataType = getMetadata(deviceUuid, NULL, OPTIONAL_SENSOR_DATA_TYPE_METADATA);

    if (stringCompare(optionalDataType, OPTIONAL_SENSOR_DATA_TYPE_MAGNETIC, false) == 0)
    {
        return true;
    }

    return false;
}

static inline const char *batterySavingDataMagneticFieldStrengthToHealth(uint16_t magStrengthMicroTesla)
{
    if (magStrengthMicroTesla >= MAGNETIC_FIELD_POOR_HEALTH_MIN &&
        magStrengthMicroTesla < MAGNETIC_FIELD_FAIR_HEALTH_MIN)
    {
        return "POOR";
    }
    else if (magStrengthMicroTesla >= MAGNETIC_FIELD_FAIR_HEALTH_MIN &&
             magStrengthMicroTesla < MAGNETIC_FIELD_GOOD_HEALTH_MIN)
    {
        return "FAIR";
    }
    else if (magStrengthMicroTesla >= MAGNETIC_FIELD_GOOD_HEALTH_MIN &&
             magStrengthMicroTesla <= MAGNETIC_FIELD_GOOD_HEALTH_MAX)
    {
        return "GOOD";
    }

    return "UNKNOWN";
}

/**
 * If we are creating the resources during pairing we use createEndpointResource which only modifies the provided
 * icDevice structure.  If we are creating these resources after the device has already been paired, we have to
 * use addNewResource which persists the modified structure.
 */
static bool createMagneticFieldResources(icDevice *device, bool isPairing)
{
    bool registered = true;

    icDebug();

    scoped_icLinkedListIterator *iter = linkedListIteratorCreate(device->endpoints);
    while (linkedListIteratorHasNext(iter))
    {
        icDeviceEndpoint *endpoint = linkedListIteratorGetNext(iter);
        // Magnetic field resources are only relevant when fault resource is on an endpoint, as they have
        // 1:1 mapping.
        if (icDeviceResourceGetValueById(endpoint->resources, SENSOR_PROFILE_RESOURCE_FAULTED))
        {
            scoped_icDeviceResource *strength = deviceServiceGetResourceById(
                device->uuid, endpoint->id, SENSOR_PROFILE_RESOURCE_MAGNETIC_FIELD_STRENGTH);
            if (!strength)
            {
                if (isPairing)
                {
                    registered = createMagFieldStrengthResource(endpoint) != NULL;
                }
                else
                {
                    icDeviceResource *res = createMagFieldStrengthResource(endpoint);
                    registered = addNewResource(endpoint->uri, res);
                }
            }

            if (registered) // only proceed if the previous resource created successfully
            {
                scoped_icDeviceResource *health = deviceServiceGetResourceById(
                    device->uuid, endpoint->id, SENSOR_PROFILE_RESOURCE_MAGNETIC_FIELD_HEALTH);
                if (!health)
                {
                    if (isPairing)
                    {
                        registered = createMagFieldHealthResource(endpoint) != NULL;
                    }
                    else
                    {
                        icDeviceResource *res = createMagFieldHealthResource(endpoint);
                        registered = addNewResource(endpoint->uri, res);
                    }
                }

                if (!registered)
                {
                    icLogError(LOG_TAG,
                               "Unable to register endpoint resource %s",
                               SENSOR_PROFILE_RESOURCE_MAGNETIC_FIELD_HEALTH);
                }
            }
            else
            {
                icLogError(LOG_TAG,
                           "Unable to register endpoint resource %s",
                           SENSOR_PROFILE_RESOURCE_MAGNETIC_FIELD_STRENGTH);
            }
        }
    }

    return registered;
}

static void updateMagneticFieldResources(const char *deviceUuid,
                                         uint8_t endpointId,
                                         const ComcastBatterySavingData *batterySavingData)
{
    double lastMagneticFieldStrengthMilliTesla = 0.0;
    const char *lastMagneticFieldHealth = NULL;

    scoped_generic const char *endpointName = zigbeeSubsystemEndpointIdAsString(endpointId);
    scoped_icDeviceResource *magneticFieldStrengthResource =
        deviceServiceGetResourceById(deviceUuid, endpointName, SENSOR_PROFILE_RESOURCE_MAGNETIC_FIELD_STRENGTH);
    if (magneticFieldStrengthResource)
    {
        stringToDouble(magneticFieldStrengthResource->value, &lastMagneticFieldStrengthMilliTesla);
    }

    scoped_icDeviceResource *magneticFieldHealthResource =
        deviceServiceGetResourceById(deviceUuid, endpointName, SENSOR_PROFILE_RESOURCE_MAGNETIC_FIELD_HEALTH);
    if (magneticFieldHealthResource)
    {
        lastMagneticFieldHealth = magneticFieldHealthResource->value;
    }

    const char *magneticFieldHealth =
        batterySavingDataMagneticFieldStrengthToHealth(batterySavingData->optionalData.magStrMicroTesla);
    const uint16_t diff = (uint16_t) abs((int32_t) (lastMagneticFieldStrengthMilliTesla * 1000) -
                                         batterySavingData->optionalData.magStrMicroTesla);

    if (diff >= 1000 || stringCompare(lastMagneticFieldHealth, magneticFieldHealth, false) != 0)
    {
        scoped_generic char *magneticFieldStrengthMilliString =
            batterySavingDataGetMagneticStrengthMilliString(batterySavingData->optionalData.magStrMicroTesla);
        // Event is generated for magnetic strength only and magnetic health is added as additional detail to the event.
        scoped_cJSON *magneticStrengthDetails = cJSON_CreateObject();
        cJSON_AddItemToObjectCS(magneticStrengthDetails,
                                SENSOR_PROFILE_RESOURCE_MAGNETIC_FIELD_HEALTH,
                                cJSON_CreateString(magneticFieldHealth));

        updateResource(deviceUuid,
                       endpointName,
                       SENSOR_PROFILE_RESOURCE_MAGNETIC_FIELD_STRENGTH,
                       magneticFieldStrengthMilliString,
                       magneticStrengthDetails);

        updateResource(
            deviceUuid, endpointName, SENSOR_PROFILE_RESOURCE_MAGNETIC_FIELD_HEALTH, magneticFieldHealth, NULL);
    }
}
