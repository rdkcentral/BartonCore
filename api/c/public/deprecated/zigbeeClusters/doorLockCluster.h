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

//
// Created by tlea on 2/20/19.
//

#ifndef ZILKER_DOORLOCKCLUSTER_H
#define ZILKER_DOORLOCKCLUSTER_H


#ifdef BARTON_CONFIG_ZIGBEE

#include "zigbeeCluster.h"

// something beyond reasonable
#define DOOR_LOCK_CLUSTER_MAX_SUPPORTED_PIN_LENGTH 16

typedef struct
{
    uint16_t userId;
    uint8_t  userStatus;
    uint8_t  userType;
    char     pin[DOOR_LOCK_CLUSTER_MAX_SUPPORTED_PIN_LENGTH];
} DoorLockClusterUser;

typedef struct
{
    void (*lockedStateChanged)(uint64_t eui64,
                               uint8_t endpointId,
                               bool isLocked,
                               const char *source,
                               uint16_t userId,
                               const void *ctx);

    void (*jammedStateChanged)(uint64_t eui64,
                                uint8_t endpointId,
                                bool isJammed,
                                const void *ctx);

    void (*tamperedStateChanged)(uint64_t eui64,
                                 uint8_t endpointId,
                                 bool isTampered,
                                 const void *ctx);

    void (*invalidCodeEntryLimitChanged)(uint64_t eui64,
                                         uint8_t endpointId,
                                         bool limitExceeded,
                                         const void *ctx);

    void (*clearAllPinCodesResponse)(uint64_t eui64,
                                     uint8_t endpointId,
                                     bool success,
                                     const void *ctx);

    void (*getPinCodeResponse)(uint64_t eui64,
                               uint8_t endpointId,
                               const DoorLockClusterUser *userDetails,
                               const void *ctx);

    void (*clearPinCodeResponse)(uint64_t eui64,
                                 uint8_t endpointId,
                                 bool success,
                                 const void *ctx);

    void (*setPinCodeResponse)(uint64_t eui64,
                               uint8_t endpointId,
                               uint8_t result,
                               const void *ctx);

    void (*keypadProgrammingEventNotification)(uint64_t eui64,
                                               uint8_t endpointId,
                                               uint8_t programmingEventCode,
                                               uint16_t userId,
                                               const char *pin,
                                               uint8_t userType,
                                               uint8_t userStatus,
                                               uint32_t localTime,
                                               const char *data,
                                               const void *ctx);

    void (*autoRelockTimeChanged)(uint64_t eui64,
                                  uint8_t endpointId,
                                  uint32_t autoRelockSeconds,
                                  const void *ctx);

} DoorLockClusterCallbacks;


ZigbeeCluster *doorLockClusterCreate(const DoorLockClusterCallbacks *callbacks, void *callbackContext);

bool doorLockClusterIsLocked(uint64_t eui64, uint8_t endpointId, bool *isLocked);

bool doorLockClusterSetLocked(uint64_t eui64, uint8_t endpointId, bool isLocked);

bool doorLockClusterGetInvalidLockoutTimeSecs(uint64_t eui64, uint8_t endpointId, uint8_t *lockoutTimeSecs);

bool doorLockClusterGetMaxPinCodeLength(uint64_t eui64, uint8_t endpointId, uint8_t *length);

bool doorLockClusterGetMinPinCodeLength(uint64_t eui64, uint8_t endpointId, uint8_t *length);

bool doorLockClusterGetMaxPinCodeUsers(uint64_t eui64, uint8_t endpointId, uint16_t *numUsers);

bool doorLockClusterGetAutoRelockTime(uint64_t eui64, uint8_t endpointId, uint32_t *autoRelockSeconds);

bool doorLockClusterSetAutoRelockTime(uint64_t eui64, uint8_t endpointId, uint32_t autoRelockSeconds);

/*
 * The result is sent via an async clearAllPinCodesResponse callback
 */
bool doorLockClusterClearAllPinCodes(uint64_t eui64, uint8_t endpointId);

/*
 * The result is sent via an async getPinCodeResponse callback
 */
bool doorLockClusterGetPinCode(uint64_t eui64, uint8_t endpointId, uint16_t userId);

/*
 * The result is sent via an async clearPinCodeResponse callback
 */
bool doorLockClusterClearPinCode(uint64_t eui64, uint8_t endpointId, uint16_t userId);

/*
 * The result is sent via an async setPinCodeResponse callback
 */
bool doorLockClusterSetPinCode(uint64_t eui64,
                               uint8_t endpointId,
                               const DoorLockClusterUser *user);

#endif // BARTON_CONFIG_ZIGBEE

#endif //ZILKER_DOORLOCKCLUSTER_H
