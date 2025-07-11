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
 * Created by Christian Leithner on 9/26/2024.
 */

#pragma once

#include <glib-2.0/glib.h>

G_BEGIN_DECLS

/*
 * A collection of public property keys
 *
 * Property values are always stores as strings, regardles of
 * the actual type of the property data.
 *
 * Properties whose keys are defined here will document the
 * expected type of the property value. If a type is not
 * specified, the property value is expected to be a string.
 *
 * For properties that stores numerical values, the base
 * of the value can be flexible and auto-detected. However, correct
 * base prefixes should be used when setting the property
 * (e.g. "0x" for hexidecimal)
 */

// Prefix for all barton properties
#define B_CORE_BARTON_PREFIX             "barton."
// Prefix for matter properties
#define B_CORE_MATTER_PREFIX             "matter."
// Prefix for thread properties
#define B_CORE_THREAD_PREFIX             "thread."
// Prefix for 802.15.4 related properties
#define B_CORE_FIFTEEN_FOUR_PREFIX       "fifteenfour."

/**
 * FIFTEEN_FOUR_EUI64: (value "barton.fifteenfour.eui64")
 *
 * The local 802.15.4 eui64 address of this installation.
 * Type: uint64
 */
#define B_CORE_BARTON_FIFTEEN_FOUR_EUI64 B_CORE_BARTON_PREFIX B_CORE_FIFTEEN_FOUR_PREFIX "eui64"

/**
 * MATTER_VENDOR_NAME (value "barton.matter.vendorName")
 *
 * The Vendor Name of the Matter device. Maximum length is 32 characters.
 * This property is required.
 * Type: string
 */
#define B_CORE_BARTON_MATTER_VENDOR_NAME B_CORE_BARTON_PREFIX B_CORE_MATTER_PREFIX "vendorName"

/**
 * MATTER_VID: (value "barton.matter.vid")
 *
 * The Vendor ID of the Matter device.
 * This property is required.
 * Type: uint16
 */
#define B_CORE_BARTON_MATTER_VENDOR_ID B_CORE_BARTON_PREFIX B_CORE_MATTER_PREFIX "vid"

/**
 * MATTER_PRODUCT_NAME (value "barton.matter.productName")
 *
 * The Product Name of the Matter device. Maximum length is 32 characters.
 * This property is required.
 * Type: string
 */
#define B_CORE_BARTON_MATTER_PRODUCT_NAME B_CORE_BARTON_PREFIX B_CORE_MATTER_PREFIX "productName"

/**
 * MATTER_PID: (value "barton.matter.pid")
 *
 * The Product ID of the Matter device.
 * This property is required.
 * Type: uint16
 */
#define B_CORE_BARTON_MATTER_PRODUCT_ID B_CORE_BARTON_PREFIX B_CORE_MATTER_PREFIX "pid"

/**
 * MATTER_PART_NUMBER: (value "barton.matter.partNumber")
 *
 * The Part Number of the Matter device. Maximum length is 32 characters.
 * This property is optional.
 * Type: string
 */
#define B_CORE_BARTON_MATTER_PART_NUMBER                                                                     \
    B_CORE_BARTON_PREFIX B_CORE_MATTER_PREFIX "partNumber"

/**
 * MATTER_PRODUCT_URL: (value "barton.matter.productURL")
 *
 * The Product URL of the Matter device. Maximum length is 256 characters.
 * This property is optional.
 * Type: string
 */
#define B_CORE_BARTON_MATTER_PRODUCT_URL                                                                     \
    B_CORE_BARTON_PREFIX B_CORE_MATTER_PREFIX "productURL"

/**
 * MATTER_PRODUCT_LABEL: (value "barton.matter.productLabel")
 *
 * The Product Label of the Matter device. Maximum length is 64 characters.
 * This property is optional.
 * Type: string
 */
#define B_CORE_BARTON_MATTER_PRODUCT_LABEL                                                                   \
    B_CORE_BARTON_PREFIX B_CORE_MATTER_PREFIX "productLabel"

/**
 * MATTER_SERIAL_NUMBER: (value "barton.matter.serialNumber")
 *
 * The Serial Number of the Matter device. Maximum length is 32 characters.
 * This property is optional.
 * Type: string
 */
#define B_CORE_BARTON_MATTER_SERIAL_NUMBER                                                                   \
    B_CORE_BARTON_PREFIX B_CORE_MATTER_PREFIX "serialNumber"

/**
 * MATTER_MANUFACTURING_DATE: (value "barton.matter.manufacturingDate")
 *
 * The Manufacturing Date of the Matter device in "YYYY-MM-DD" format.
 * This property is optional.
 * Type: string
 */
#define B_CORE_BARTON_MATTER_MANUFACTURING_DATE                                                              \
    B_CORE_BARTON_PREFIX B_CORE_MATTER_PREFIX "manufacturingDate"

/**
 * MATTER_HARDWARE_VERSION: (value "barton.matter.hardwareVersion")
 *
 * The Hardware Version of the Matter device.
 * This property is required.
 * Type: uint16
 */
#define B_CORE_BARTON_MATTER_HARDWARE_VERSION                                                                \
    B_CORE_BARTON_PREFIX B_CORE_MATTER_PREFIX "hardwareVersion"

/**
 * MATTER_HARDWARE_VERSION_STRING: (value "barton.matter.hardwareVersionString")
 *
 * The Hardware Version String of the Matter device. 1 to 64 characters.
 * This property is required.
 * Type: string
 */
#define B_CORE_BARTON_MATTER_HARDWARE_VERSION_STRING                                                         \
    B_CORE_BARTON_PREFIX B_CORE_MATTER_PREFIX "hardwareVersionString"

/**
 * DEFAULT_THREAD_NETWORK_NAME: (value "barton.thread.defaultNetworkName")
 *
 * The default name of the Thread network, provided by the client. This is the name
 * that will be used if we (Barton) create the Thread network ourselves. The end user
 * can still change this later.
 * Type: string
 */
#define B_CORE_BARTON_DEFAULT_THREAD_NETWORK_NAME                                                            \
    B_CORE_BARTON_PREFIX B_CORE_THREAD_PREFIX "defaultNetworkName"

/**
 * MATTER_SETUP_DISCRIMINATOR: (value "barton.matter.setupDiscriminator")
 *
 * The setup discriminator for the Matter device.
 * Type: uint16
 */
#define B_CORE_BARTON_MATTER_SETUP_DISCRIMINATOR                                                             \
    B_CORE_BARTON_PREFIX B_CORE_MATTER_PREFIX "setupDiscriminator"

/**
 * MATTER_SPAKE2P_ITERATION_COUNT: (value "barton.matter.spake2pIterationCount")
 *
 * The iteration count for the Matter device's spake2p verifier.
 * Type: uint32
 */
#define B_CORE_BARTON_MATTER_SPAKE2P_ITERATION_COUNT                                                         \
    B_CORE_BARTON_PREFIX B_CORE_MATTER_PREFIX "spake2pIterationCount"

/**
 * MATTER_SPAKE2P_SALT: (value "barton.matter.spake2pSalt")
 *
 * The base64-encoded salt for the Matter device's spake2p verifier.
 * Type: string
 */
#define B_CORE_BARTON_MATTER_SPAKE2P_SALT                                                                    \
    B_CORE_BARTON_PREFIX B_CORE_MATTER_PREFIX "spake2pSalt"

/**
 * MATTER_SPAKE2P_VERIFIER: (value "barton.matter.spake2pVerifier")
 *
 * The Matter device's base64-encoded spake2p verifier.
 * Type: string
 */
#define B_CORE_BARTON_MATTER_SPAKE2P_VERIFIER                                                                \
    B_CORE_BARTON_PREFIX B_CORE_MATTER_PREFIX "spake2pVerifier"

/**
 * MATTER_SETUP_PASSCODE: (value "barton.matter.setupPasscode")
 *
 * The setup passcode for the Matter device.
 * Type: uint32
 */
#define B_CORE_BARTON_MATTER_SETUP_PASSCODE                                                                  \
    B_CORE_BARTON_PREFIX B_CORE_MATTER_PREFIX "setupPasscode"


G_END_DECLS
