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
// Created by tlea on 2/15/19.
//

#ifndef ZILKER_POLLCONTROLCLUSTER_H
#define ZILKER_POLLCONTROLCLUSTER_H


#ifdef BARTON_CONFIG_ZIGBEE

#include "zigbeeCluster.h"
#include "comcastBatterySaving.h"

#define SHORT_POLL_INTERVAL_QS_METADATA "pollControl.shortPollIntervalQS"
#define LONG_POLL_INTERVAL_QS_METADATA "pollControl.longPollIntervalQS"
#define FAST_POLL_TIMEOUT_QS_METADATA "pollControl.fastPollTimeoutQS"
#define CHECK_IN_INTERVAL_QS_METADATA "pollControl.checkInIntervalQS"

#define POLL_CONTROL_CHECKIN_COMMAND_ID 0x00

typedef struct
{
    void (*checkin)(uint64_t eui64, uint8_t endpointId, const ComcastBatterySavingData *batterySavingData, const void *ctx);
} PollControlClusterCallbacks;


ZigbeeCluster *pollControlClusterCreate(const PollControlClusterCallbacks *callbacks, const void *callbackContext);

/**
 * Add callback handler (PollControlClusterCallbacks) for passed ZigbeeCluster reference
 *
 * @param baseCluster
 * @param callbacks
 * @param callbackContext
 * @return True on success
 */
bool pollControlClusterAddCallback(ZigbeeCluster *baseCluster, const PollControlClusterCallbacks *callback, const void *callbackContext);

/**
 * Set whether or not to set a binding on this cluster.  By default we bind the cluster.
 * @param deviceConfigurationContext the configuration context
 * @param bind true to bind or false not to
 */
void pollControlClusterSetBindingEnabled(const DeviceConfigurationContext *deviceConfigurationContext, bool bind);

bool pollControlClusterSendCustomCheckInResponse(uint64_t eui64, uint8_t endpointId);

bool pollControlClusterSendCheckInResponse(uint64_t eui64, uint8_t endpointId, bool startFastPoll);

bool pollControlClusterStopFastPoll(uint64_t eui64, uint8_t endpointId);

/**
 * Set the long poll interval (in quarter seconds).  This can be used to speed up data requests on sleepy devices
 * during pairing or reconfiguration when we dont receive a checkin command (where the mechanism to speed up is
 * different).
 *
 * @param eui64
 * @param endpointId
 * @param newIntervalQs the new long poll interval in quarter seconds
 * @return true on success
 */
bool pollControlClusterSetLongPollInterval(uint64_t eui64, uint8_t endpointId, uint32_t newIntervalQs);

#endif // BARTON_CONFIG_ZIGBEE

#endif //ZILKER_POLLCONTROLCLUSTER_H
