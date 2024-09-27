//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2015 iControl Networks, Inc.
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
// iControl Networks retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------

/*-----------------------------------------------
 * philipsHue.h
 *
 * Philips Hue Library
 *
 * Author: tlea
 *-----------------------------------------------*/

#ifndef PHILIPS_HUE_H
#define PHILIPS_HUE_H

#include <stdbool.h>
#include <icTypes/icLinkedList.h>
#include <icTypes/icHashMap.h>
#include <icTypes/icStringHashMap.h>

typedef struct
{
    char *id;
    bool isOn;
} PhilipsHueLight;

typedef void (*PhilipsHueBridgeDiscoverCallback)(const char *macAddress, const char *ipAddress, const char *username);

/*
 * Start discovering Philips Hue Bridges on the local network.
 */
bool philipsHueStartDiscoveringBridges(PhilipsHueBridgeDiscoverCallback callback);

/*
 * Stop discovering Philips Hue Bridges on the local network.
 */
bool philipsHueStopDiscoveringBridges();

/*
 * Retrieve the list and state of all lights connected to the bridge
 */
icLinkedList* philipsHueGetLights(const char *ipAddress, const char *username);

/*
 * Turn a light on or off
 */
bool philipsHueSetLight(const char *ipAddress, const char *username, const char *lightId, bool on);

typedef void (*PhilipsHueLightChangedCallback)(const char *macAddress, const char *lightId, bool isOn);
typedef void (*PhilipsHueIpAddressChangedCallback)(const char *macAddress, char *newIpAddress);

/*
 * Start monitoring a bridge for changes and problems
 */
bool philipsHueStartMonitoring(const char *macAddress,
                               const char *ipAddress,
                               const char *username,
                               PhilipsHueLightChangedCallback lightChangedCallback,
                               PhilipsHueIpAddressChangedCallback ipAddressChangedCallback);

/*
 * Stop monitoring a bridge for changes and problems
 */
bool philipsHueStopMonitoring(const char *ipAddress);

/*
 * Release the resources used by the provided light
 */
void philipsHueLightDestroy(PhilipsHueLight *light);

#endif //PHILIPS_HUE_H
