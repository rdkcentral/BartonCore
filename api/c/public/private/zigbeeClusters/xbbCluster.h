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
 * Created by Thomas Lea on 4/2/21.
 */

#ifndef ZILKER_XBBCLUSTER_H
#define ZILKER_XBBCLUSTER_H

#include "zigbeeCluster.h"

#define XBB_CLUSTER_STATUS_LID_OPEN_BIT               0x00
#define XBB_CLUSTER_STATUS_AC_POWER_LOSS_BIT          0x01
#define XBB_CLUSTER_STATUS_BATTERY_BANK_1_PRESENT_BIT 0x02
#define XBB_CLUSTER_STATUS_BATTERY_BANK_2_PRESENT_BIT 0x03
#define XBB_CLUSTER_STATUS_BATTERY_BANK_3_PRESENT_BIT 0x04
#define XBB_CLUSTER_STATUS_BATTERY_BANK_1_LOW_BIT     0x05
#define XBB_CLUSTER_STATUS_BATTERY_BANK_2_LOW_BIT     0x06
#define XBB_CLUSTER_STATUS_BATTERY_BANK_3_LOW_BIT     0x07
#define XBB_CLUSTER_STATUS_TEMP_LOW_BIT               0x08
#define XBB_CLUSTER_STATUS_TEMP_HIGH_BIT              0x09

typedef enum
{
    xbbBatteryTypeUnknown,
    xbbBatteryTypeXbb1_or_24,
    xbbBatteryTypeXbbl,
} XbbBatteryType;

typedef struct
{
    void (*xbbStatusChanged)(void *ctx, uint64_t eui64, uint8_t endpointId, uint16_t status);
} XbbClusterCallbacks;

/**
 * Create an instance of the XBB cluster
 *
 * @param callbacks
 * @param callbackContext
 * @return
 */
ZigbeeCluster *xbbClusterCreate(const XbbClusterCallbacks *callbacks, void *callbackContext);

/**
 * Set the battery type to be used by this cluster instance
 *
 * @param cluster
 * @param batteryType
 */
void xbbClusterSetBatteryType(const ZigbeeCluster *cluster, XbbBatteryType batteryType);

/**
 * Read the status attribute.
 *
 * @param cluster
 * @param eui64
 * @param endpointId
 * @param status will be populated with the current status
 * @return true on succcess
 */
bool xbbClusterGetStatus(const ZigbeeCluster *cluster, uint64_t eui64, uint8_t endpointId, uint16_t *status);

#endif // ZILKER_XBBCLUSTER_H
