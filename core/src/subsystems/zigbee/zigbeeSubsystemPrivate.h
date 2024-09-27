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
/*
 * These are called internally by the event handler.
 *
 * Created by Thomas Lea on 3/29/16.
 */

#ifndef FLEXCORE_ZIGBEESUBSYSTEMPRIVATE_H_H
#define FLEXCORE_ZIGBEESUBSYSTEMPRIVATE_H_H

#include <stdbool.h>
#include <stdint.h>

void zigbeeSubsystemHandleZhalStartupEvent(void);

void zigbeeSubsystemDeviceDiscovered(IcDiscoveredDeviceDetails *details);

void zigbeeSubsystemAttributeReportReceived(ReceivedAttributeReport* report);

void zigbeeSubsystemClusterCommandReceived(ReceivedClusterCommand* command);

void zigbeeSubsystemDeviceLeft(uint64_t eui64);

void zigbeeSubsystemDeviceRejoined(uint64_t eui64, bool isSecure);

void zigbeeSubsystemLinkKeyUpdated(uint64_t eui64, bool isUsingHashBasedKey);

void zigbeeSubsystemApsAckFailure(uint64_t eui64);

void zigbeeSubsystemDeviceBeaconReceived(uint64_t eui64,
                                         uint16_t panId,
                                         bool isOpen,
                                         bool hasEndDeviceCapacity,
                                         bool hasRouterCapability,
                                         uint8_t depth);

void zigbeeSubsystemDeviceOtaUpgradeMessageSent(OtaUpgradeEvent *otaEvent);
void zigbeeSubsystemDeviceOtaUpgradeMessageReceived(OtaUpgradeEvent *otaEvent);

void zigbeeSubsystemRequestUnclaimedDevicesLeave(void);

/**
 * Set network configuration and bump counters. Call this prior to network initialization.
 * @param eui64 Network coordinator address
 * @param networkBlob base64 encoded configuration from/for ZigbeeCore (opaque)
 * @return whether all properties were set.
 */
bool zigbeeSubsystemSetNetworkConfig(uint64_t eui64, const char *networkBlob);

bool zigbeeSubsystemClaimDiscoveredDevice(IcDiscoveredDeviceDetails *details, DeviceMigrator *deviceMigrator);

/**
 * Add a premature cluster command
 * @param command the command to add
 */
void zigbeeSubsystemAddPrematureClusterCommand(const ReceivedClusterCommand *command);

#endif //FLEXCORE_ZIGBEESUBSYSTEMPRIVATE_H_H
