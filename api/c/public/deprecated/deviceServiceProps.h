//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright 2023 Comcast
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

#pragma once

#define DEVICE_PROP_MATTER_NS                      "device.matter."

// TODO: generate this from propertyDefinitions

/**
 * @brief Base64 encoded identity protection key
 * @ref Matter 1.0 4.13.2.6
 * @note this data is sensitive
 */
#define DEVICE_PROP_MATTER_CURRENT_IPK             DEVICE_PROP_MATTER_NS "currentIpk"

/**
 * @brief A string enumeration describing the operational envrionment of the device service matter stack
 */
#define DEVICE_PROP_MATTER_OPERATIONAL_ENVIRONMENT DEVICE_PROP_MATTER_NS "environment"

// ComcastChimeCluster

/**
 * @brief Uri that tells the device to play a siren
 * @see MatterChimeDeviceDriver.cpp's DEFAULT_SIREN_URI for an example of what this might look like
 */
#define DEVICE_PROP_MATTER_SIREN_URI               DEVICE_PROP_MATTER_NS "comcastChime.playUrl.sirenUri"

// WifiNetworkDiagnosticsCluster

/**
 * @brief uint16 min rate limiter for Wifi RSSI subscriptions
 * @ref Matter 1.0 11.14
 */
#define DEVICE_PROP_MATTER_WIFI_DIAGNOSTICS_RSSI_MIN_INTERVAL_SECS                                                     \
    "device.matter.wifiDiagnostics.rssi.minIntervalSeconds"

/**
 * @brief uint16 max rate limiter for Wifi RSSI subscriptions
 * @ref Matter 1.0 11.14
 */
#define DEVICE_PROP_MATTER_WIFI_DIAGNOSTICS_RSSI_MAX_INTERVAL_SECS                                                     \
    "device.matter.wifiDiagnostics.rssi.maxIntervalSeconds"
