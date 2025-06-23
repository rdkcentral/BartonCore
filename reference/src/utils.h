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

/*
 * Created by Munilakshmi Velampati on 29/01/25.
 */

#pragma once

#include "barton-core-device-found-details.h"
#include "barton-core-resource.h"
#include <glib.h>


/**
 * stringifyMode
 * @mode: the mode associated with the resource.
 *
 * @brief Translate resource mode flags into a human-readable string.  The values are positional and take a similar
 * form to the permissions displayed by "ls -l".
 *
 *  Where:
 *      r  Readable
 *      w  Writable
 *      x  Executable
 *      d  Dynamic or dynamic capable
 *      e  Emit events
 *      l  Lazy save next
 *      s  Sensitive
 *
 *  Example:
 *      rwx----  Resource is readable, writable and executable.
 *
 *
 * Returns: (transfer full): gchar* - a string.
 */
const gchar *stringifyMode(guint mode);

/**
 * getResourceValue
 * @mode: the mode associated with the resource.
 * @value: the value associated with the resource.
 *
 * @brief Get the value of a resource, respecting resources marked sensitive.
 *
 * Returns: (transfer full): gchar* - a string.
 */
gchar *getResourceValue(guint resourceMode, gchar *value);

/**
 * printDeviceFoundDetails
 * @printPrefix: prefix value to print.
 * @deviceFoundDetails: the instance of BCoreDeviceFoundDetails.
 *
 * @brief Prints the device related information.
 *
 */
void printDeviceFoundDetails(const char *printPrefix, BCoreDeviceFoundDetails *);

/**
 * getResourceDump
 * @resource: the mode associated with the resource.
 * @value: the value associated with the resource.
 *
 * @brief Dump all of the details about a resource into a newly allocated string.
 *
 * Returns: (transfer full): gchar* - a string.
 */
gchar *getResourceDump(BCoreResource *resource);
