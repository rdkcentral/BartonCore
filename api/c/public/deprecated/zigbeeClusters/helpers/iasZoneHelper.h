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

#ifndef ZILKER_IASZONEHELPER_H
#define ZILKER_IASZONEHELPER_H


#ifdef BARTON_CONFIG_ZIGBEE

#include <subsystems/zigbee/zigbeeSubsystem.h>
#include <deviceDrivers/zigbeeDriverCommon.h>

/**
 * Cluster helper for handling a zone status changed event.
 * @param eui64
 * @param endpointId
 * @param status
 * @param alarm1Resource The name for the "alarm 1" boolean resource.
 * @param alarm2Resource The name for the "alarm 2" boolean resource.
 * @param ctx
 * @see ZCL specifications for alarm1/alarm2 meaning by zone type
 */
void iasZoneStatusChangedHelper(uint64_t eui64,
                                uint8_t endpointId,
                                uint16_t zoneStatus,
                                const ZigbeeDriverCommon *ctx);

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
                                       icInitialResourceValues *initialResourceValues);

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
                              icInitialResourceValues *initialResourceValues);

/**
 * Helper to get the name equivalent for a zone type value
 * @param zoneType the zone type value
 * @return the name
 */
const char *iasZoneHelperGetZoneTypeName(uint16_t zoneType);

/**
 * Helper to get the trouble resource id
 * @param deviceClass the device class of the device
 * @param zoneTypeName the zone type name
 * @return the trouble resource name, or NULL if none
 */
const char *iasZoneHelperGetTroubleResource(const char *deviceClass, const char *zoneTypeName);

/**
 * Helper to sync zone status
 * @param uuid - the uuid of the device
 * @param endpointId - the device endpoint id
 * @ctx - the callbackContext value provided by Zigbee Common Driver
 */
void iasZoneHelperSyncZoneStatus(const char *uuid, const uint8_t endpointId, ZigbeeDriverCommon *ctx);

#endif // BARTON_CONFIG_ZIGBEE
#endif //ZILKER_IASZONEHELPER_H
