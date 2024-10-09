//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2019 Comcast
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
// Comcast retains all ownership rights.
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
