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
// Created by mkoch201 on 4/15/19.
//

#pragma once

/**
 * Represents the set of initial resource values for both device and endpoint values.  A resource value should be
 * populated with NULL for resources that have unknown initial values.  This set of values is also used to determine
 * which resources should be created, so its important to populate these NULL values, since if there is no initial value
 * the resource will not be created.
 */

typedef struct _icInitialResourceValues icInitialResourceValues;

/**
 * Create a new instance
 * @return the created instance
 */
icInitialResourceValues *initialResourceValuesCreate();

/**
 * Destroy an instance
 * @param initialResourceValues the instance to destroy
 */
void initialResourceValuesDestroy(icInitialResourceValues *initialResourceValues);

/**
 * Put/Replace an initial value for a device resource
 *
 * @param values the set of values
 * @param resourceId the resource id
 * @param value the value
 * @return true if successful, false otherwise(invalid inputs)
 */
bool initialResourceValuesPutDeviceValue(icInitialResourceValues *values, const char *resourceId, const char *value);

/**
 * Put an initial value for a device resource if none already exists
 * @param values the value to put
 * @param resourceId the resource id
 * @param value the value
 * @return true if value was put, false otherwise
 */
bool initialResourceValuesPutDeviceValueIfNotExists(icInitialResourceValues *values,
                                                    const char *resourceId,
                                                    const char *value);

/**
 * Put/Replace an initial value for an endpoint resource
 *
 * @param values the set of values
 * @param endpointId the endpoint id
 * @param resourceId the resource id
 * @param value the value
 * @return true if successful, false otherwise(invalid inputs)
 */
bool initialResourceValuesPutEndpointValue(icInitialResourceValues *values,
                                           const char *endpointId,
                                           const char *resourceId,
                                           const char *value);

/**
 * Put an initial value for an endpoint resource if none already exists
 *
 * @param values the set of values
 * @param endpointId the endpoint id
 * @param resourceId the resource id
 * @param value the value
 * @return true if value was put, false otherwise
 */
bool initialResourceValuesPutEndpointValueIfNotExists(icInitialResourceValues *values,
                                                      const char *endpointId,
                                                      const char *resourceId,
                                                      const char *value);

/**
 * Check if an initial value exists for a device resource
 * @param values the set of values
 * @param resourceId the resource id
 * @return true if an entry for the initial value exists(even if the value is NULL), false if it does not exist
 */
bool initialResourceValuesHasDeviceValue(icInitialResourceValues *values, const char *resourceId);

/**
 * Check if an initial value exists for an endpoint resource
 * @param values the set of values
 * @param resourceId the resource id
 * @return true if an entry for the initial value exists(even if the value is NULL), false if it does not exist
 */
bool initialResourceValuesHasEndpointValue(icInitialResourceValues *values,
                                           const char *endpointId,
                                           const char *resourceId);

/**
 * Get the initial value for a device resource.  Using this function you cannot distinguish between a non-existant
 * value and NULL value.
 * @param values the set of values
 * @param resourceId the resource id
 * @return the initial value, which may be NULL
 */
const char *initialResourceValuesGetDeviceValue(icInitialResourceValues *values, const char *resourceId);

/**
 * Get the initial value for an endpoint resource.  Using this function you cannot distinguish between a non-existant
 * value and NULL value.
 * @param values the set of values
 * @param endpointId the endpoint id
 * @param resourceId the resource id
 * @return the initial value, which may be NULL
 */
const char *
initialResourceValuesGetEndpointValue(icInitialResourceValues *values, const char *endpointId, const char *resourceId);

/**
 * Log all the initial resources values that have been set
 * @param values the initial values
 */
void initialResourcesValuesLogValues(icInitialResourceValues *values);
