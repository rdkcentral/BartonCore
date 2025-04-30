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

#pragma once


#ifdef BARTON_CONFIG_ZIGBEE

#include <deviceDrivers/zigbeeDriverCommon.h>
#include <subsystems/zigbee/zigbeeSubsystem.h>

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
void iasZoneStatusChangedHelper(uint64_t eui64, uint8_t endpointId, uint16_t zoneStatus, const ZigbeeDriverCommon *ctx);

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
