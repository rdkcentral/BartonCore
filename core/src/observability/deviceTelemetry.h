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

#ifndef DEVICE_TELEMETRY_H
#define DEVICE_TELEMETRY_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef BARTON_CONFIG_OBSERVABILITY

/**
 * Record device telemetry for a resource update.
 * Inspects resource type and emits appropriate OTel metrics:
 *   - device.resource.value gauge for numeric types (integer, percentage)
 *   - device.resource.state_change counter for boolean types
 *   - device.resource.update counter for all value changes
 *
 * All metrics include rich device attributes (device.id, device.class,
 * endpoint.profile, resource.name, manufacturer, model, hardware/firmware
 * version, driver.type).
 *
 * @param deviceUuid   Device UUID
 * @param endpointId   Endpoint ID (NULL for device-level resources)
 * @param resourceId   Resource name
 * @param resourceType Resource type string (e.g., "com.icontrol.boolean")
 * @param newValue     New resource value (string representation)
 * @param didChange    Whether the resource value actually changed
 */
void deviceTelemetryRecordResourceUpdate(const char *deviceUuid,
                                         const char *endpointId,
                                         const char *resourceId,
                                         const char *resourceType,
                                         const char *newValue,
                                         bool didChange);

/**
 * Invalidate cached device attributes for the given device.
 * Call when a device is removed or firmware version changes.
 *
 * @param deviceUuid  Device UUID to evict from cache (NULL evicts all)
 */
void deviceTelemetryInvalidateCache(const char *deviceUuid);

#else /* !BARTON_CONFIG_OBSERVABILITY */

static inline void deviceTelemetryRecordResourceUpdate(const char *deviceUuid,
                                                       const char *endpointId,
                                                       const char *resourceId,
                                                       const char *resourceType,
                                                       const char *newValue,
                                                       bool didChange)
{
    (void) deviceUuid;
    (void) endpointId;
    (void) resourceId;
    (void) resourceType;
    (void) newValue;
    (void) didChange;
}

static inline void deviceTelemetryInvalidateCache(const char *deviceUuid)
{
    (void) deviceUuid;
}

#endif /* BARTON_CONFIG_OBSERVABILITY */

#ifdef __cplusplus
}
#endif

#endif /* DEVICE_TELEMETRY_H */
