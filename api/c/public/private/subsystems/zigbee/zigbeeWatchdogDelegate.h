// ------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2025 Comcast Cable Communications Management, LLC
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
// ------------------------------ tabstop = 4 ----------------------------------

//
// Created by Kevin Funderburg on 8/26/2025.
//

#pragma once

#include <stdbool.h>
#include <zhal/zhal.h>

typedef enum
{
    ZHAL_RESTART_ACTIVE,     // Delegate is attempting restart (keep trying)
    ZHAL_RESTART_NOT_ACTIVE, // Delegate won't/can't restart (stop trying)
} ZhalRestartStatus;

/**
 * @brief Watchdog delegate interface for Zigbee subsystem monitoring.
 *
 * This structure defines the interface that external watchdog implementations
 * may provide to monitor and manage the health of the Zigbee subsystem.
 * The Zigbee subsystem uses this delegate to report health status, request
 * recovery actions, and coordinate with broader system health monitoring
 * and process management systems.
 *
 * @note Function pointer requirements:
 *       - OPTIONAL: init, shutdown (may be NULL if not needed)
 *       - MANDATORY: All other function pointers must be implemented
 *       The Zigbee subsystem will reject any delegate that has NULL
 *       mandatory function pointers during registration.
 *
 * @see zigbeeSubsystemSetWatchdogDelegate()
 */
typedef struct ZigbeeWatchdogDelegate
{
    /**
     * @brief Initialize the watchdog delegate (OPTIONAL).
     *
     * This function is called during subsystem initialization to set up
     * watchdog monitoring, configure timers, and prepare for health monitoring
     * operations. May be NULL if no initialization is required.
     */
    void (*init)(void);

    /**
     * @brief Shutdown the delegate (OPTIONAL).
     *
     * This function is called during subsystem shutdown to cleanly disable
     * watchdog monitoring and free resources. May be NULL if no cleanup is required.
     */
    void (*shutdown)(void);

    /**
     * @brief Notify the delegate of device communication failure status.
     *
     * This function informs the watchdog delegate when all monitored Zigbee
     * devices are in communication failure.
     *
     * @param allInCommFail true when all monitored devices are in comm fail,
     *                      false when at least one device is communicating
     */
    void (*setAllDevicesInCommFail)(bool allInCommFail);

    /**
     * @brief Pet the zhal watchdog.
     *
     * This function is called to indicate that the zhal process is operating
     * normally and should reset its watchdog timer. Unlike the updateXxxStatus()
     * functions which immediately set a specific health state, this function
     * allows the delegate to determine if the zhal is healthy based on the
     * frequency of petting and its own timing logic.
     *
     * @return true if the pet was successful, false on error
     */
    bool (*petZhal)(void);

    /**
     * @brief Restart the ZHAL implementation.
     *
     * This function is called when the Zigbee subsystem determines that the ZHAL
     * implementation needs to be restarted due to failure conditions. The implementation
     * should attempt to restart the ZHAL process and return the appropriate status.
     *
     * @return ZHAL_RESTART_ACTIVE if restart is being attempted,
     *         ZHAL_RESTART_NOT_ACTIVE if restart cannot or will not be performed
     */
    ZhalRestartStatus (*restartZhal)(void);

    /**
     * @brief Handle ZHAL response events for watchdog monitoring.
     *
     * This function is called for each ZHAL operation response to monitor
     * the health of the Zigbee stack.
     *
     * @param networkBusy true if the network is busy, false if it's idle
     */
    void (*zhalResponseHandler)(bool networkBusy);

    /**
     * @brief Check if a watchdog recovery action is currently in progress.
     *
     * This function allows the subsystem to query whether the watchdog system
     * is currently performing recovery operations (e.g., restarting services).
     *
     * @return true if recovery is in progress, false otherwise
     */
    bool (*getActionInProgress)(void);

} ZigbeeWatchdogDelegate;
