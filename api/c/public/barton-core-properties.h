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
 * MATTER_VID: (value "barton.matter.vid")
 *
 * The Vendor ID of the Matter device.
 * Type: uint16
 */
#define B_CORE_BARTON_MATTER_VID B_CORE_BARTON_PREFIX B_CORE_MATTER_PREFIX "vid"

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

/**
 * MATTER_OPERATIONAL_HOST: (value "barton.matter.operationalHost")
 *
 * The host of the xpki environment to use. eg. "certifier.xpki.io"
 * Type: string
 */
#define B_CORE_BARTON_MATTER_OPERATIONAL_HOST B_CORE_BARTON_PREFIX B_CORE_MATTER_PREFIX "operationalHost"

G_END_DECLS
