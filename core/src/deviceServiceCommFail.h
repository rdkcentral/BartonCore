// Copyright (C) 2024 Comcast Corporation
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
// any other form of intellectual property whatsoever.my
//
// Comcast Corporation retains all ownership rights.

#pragma once
#include "device/icDevice.h"
#include "stdint.h"

void deviceServiceCommFailInit(void);
void deviceServiceCommFailShutdown(void);

/**
 * @brief Set the service wide comm fail timeout in seconds.
 *        Devices that have 'commFailOverrideSeconds' metadata will
 *        override this value automatically.
 *
 * @param commFailTimeoutSecs
 */
void deviceServiceCommFailSetTimeoutSecs(uint32_t commFailTimeoutSecs);

/**
 * @brief Get the service wide comm fail timeout in seconds.
 *
 * @return uint32_t
 */
uint32_t deviceServiceCommFailGetTimeoutSecs(void);

/**
 * @brief Notify a device driver of its default comm-fail timeout, in seconds.
 *        This is included in all '*Set' functions, and is useful for
 *        notifying drivers of intent without actually starting monitoring (e.g., for configuration)
 *
 * @param device
 * @param defaultTimeoutSecs
 */
void deviceServiceCommFailHintDeviceTimeoutSecs(const icDevice *device, uint32_t defaultTimeoutSecs);

/**
 * @brief Set the device comm fail timeout, in seconds. If the device has
 *        'commFailOverrideSeconds,' the value there will be used instead.
 *
 * @param device
 * @param defaultTimeoutSecs
 */
void deviceServiceCommFailSetDeviceTimeoutSecs(const icDevice *device, uint32_t defaultTimeoutSecs);

/**
 * @brief Get the comm fail timeout for a device, in seconds
 *
 * @param deviceUuid
 * @return uint32_t
 */
uint32_t deviceServiceCommFailGetDeviceTimeoutSecs(const icDevice *device);
