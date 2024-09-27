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

#ifndef ZILKER_BASICCLUSTER_H
#define ZILKER_BASICCLUSTER_H

#include "zigbeeCluster.h"

#define REBOOT_REASON_DEFAULT 0xFF

static const char* basicClusterRebootReasonLabels[] = {
    "UNKNOWN",
    "BATTERY",
    "BROWNOUT",
    "WATCHDOG",
    "RESET_PIN",
    "MEMORY_HARDWARE_FAULT",
    "SOFTWARE_EXCEPTION",
    "OTA_BOOTLOAD_SUCCESS",
    "SOFTWARE_RESET",
    "POWER_BUTTON",
    "TEMPERATURE",
    "BOOTLOAD_FAILURE"
};
typedef enum {
    UNKNOWN,
    BATTERY,
    BROWNOUT,
    WATCHDOG,
    RESET_PIN,
    MEMORY_HARDWARE_FAULT,
    SOFTWARE_EXCEPTION,
    OTA_BOOTLOAD_SUCCESS,
    SOFTWARE_RESET,
    POWER_BUTTON,
    TEMPERATURE,
    BOOTLOAD_FAILURE
} basicClusterRebootReason;

typedef struct
{
    void (*rebootReasonChanged)(void *ctx, uint64_t eui64, uint8_t endpointId, basicClusterRebootReason reason);
} BasicClusterCallbacks;

ZigbeeCluster *basicClusterCreate(const BasicClusterCallbacks *callbacks, void *callbackContext);

/**
 * Performs a device reboot.  This is a manufacturer specific extension to the Basic cluster and is not
 * available on all devices.
 *
 * @param eui64
 * @param endpointId
 * @param mfgId
 * @return
 */
bool basicClusterRebootDevice(uint64_t eui64, uint8_t endpointId, uint16_t mfgId);

/**
 * Set whether or not to configure reboot reason reports. By default, reports are disabled.
 * @param deviceConfigurationContext the configuration context
 * @param configure true to configure, false otherwise
 */
void basicClusterSetConfigureRebootReason(const DeviceConfigurationContext *deviceConfigurationContext, bool configure);

/**
 * Defaults the reboot reason
 *
 * @param eui64
 * @param endpointId
 *
 * @return 0 if success, failure otherwise
 */
int basicClusterResetRebootReason(uint64_t eui64, uint8_t endPointId);

#endif //ZILKER_BASICCLUSTER_H
