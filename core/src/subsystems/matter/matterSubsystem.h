//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2021 Comcast Corporation
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

/*
 * Created by Thomas Lea on 3/11/21.
 */

#ifndef ZILKER_MATTERSUBSYSTEM_H
#define ZILKER_MATTERSUBSYSTEM_H

#ifdef __cplusplus
extern "C" {
#endif

#define MATTER_SUBSYSTEM_NAME "matter"
#define MATTER_SUBSYSTEM_VERSION (1U)


#include <stdint.h>

#include <device-driver/device-driver.h>
#include "subsystemManagerCallbacks.h"

/*
typedef struct
{
    void *device;
    uint8_t endpoint;
} MatterDeviceInfo;
 */

/**
 * Locate the commissionable device as specified in the provided setup payload and commission it.
 *
 * @param setupPayload the decoded QR code setup payload
 * @param timeoutSeconds the maximum number of seconds to allow for commissioning to complete
 * @return true if the device is successfully commissioned
 */
bool matterSubsystemCommissionDevice(const char *setupPayload, uint16_t timeoutSeconds);

/**
 * Locate the device identified by nodeId and pair it
 *
 * @param nodeId
 * @param timeoutSeconds the maximum number of seconds to allow for pairing to complete
 * @return true if the device is successfully commissioned
 */
bool matterSubsystemPairDevice(uint64_t nodeId, uint16_t timeoutSeconds);

/*
MatterDeviceInfo *matterSubsystemGetDeviceInfo(const char *uuid);
 */

#ifdef __cplusplus
}
#endif

#endif //ZILKER_MATTERSUBSYSTEM_H
