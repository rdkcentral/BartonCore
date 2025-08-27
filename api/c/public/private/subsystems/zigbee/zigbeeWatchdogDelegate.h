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
    ZIGBEE_CORE_RECOVERY_ENITITY_HEARTBEAT,
    ZIGBEE_CORE_RECOVERY_ENITITY_COMM_FAIL,
    ZIGBEE_CORE_RECOVERY_ENITITY_NETWORK_BUSY
} ZigbeeCoreRecoveryEntity;

static const char *ZigbeeCoreRecoveryEntityLabels[] = {"heartbeat", "communication failure", "network busy"};

static const char *zigbeeCoreRecoveryReasonLabels[] = {"Recovery reason: heartbeat",
                                                       "Recovery reason: communication failure",
                                                       "Recovery reason: network busy"};

typedef enum
{
    ZIGBEE_CORE_RESTART_ACTIVE,     // Delegate is attempting restart (keep trying)
    ZIGBEE_CORE_RESTART_NOT_ACTIVE, // Delegate won't/can't restart (stop trying)
} ZigbeeCoreRestartStatus;

/**
 * @brief Watchdog delegate interface for Zigbee subsystem monitoring.
 *
 * This structure defines the interface that external watchdog implementations
 * must provide to monitor and manage the health of the Zigbee subsystem and
 * ZigbeeCore process. The Zigbee subsystem uses this delegate to report
 * health status, request recovery actions, and coordinate with system-wide
 * watchdog functionality.
 *
 * @note ALL function pointers in this structure are MANDATORY and must be
 *       implemented. The Zigbee subsystem will reject any delegate that has
 *       NULL function pointers during registration.
 *
 * @see zigbeeSubsystemSetWatchdogDelegate()
 */
typedef struct ZigbeeWatchdogDelegate
{
    /**
     * @brief Initialize the software watchdog system.
     *
     * This function is called once during subsystem initialization to set up
     * watchdog monitoring. It should configure any necessary watchdog entities,
     * timers, or monitoring threads.
     *
     */
    void (*initializeWatchdog)(void);

    /**
     * @brief Shutdown the software watchdog system.
     *
     * This function is called during subsystem shutdown to cleanly disable
     * watchdog monitoring and free resources.
     */
    void (*shutdownWatchdog)(void);

    /**
     * @brief Update the status of a specific Zigbee watchdog entity.
     *
     * This function allows the Zigbee subsystem to report the health status of
     * various monitored entities (heartbeat, network busy, communication failure).
     *
     * @param ZigbeeCoreRecoveryEntity The specific Zigbee entity being reported on
     * @param status true if the entity is healthy, false if it's in a failure state
     * @return true if the status update was processed successfully, false on error
     */
    bool (*updateWatchdogStatus)(ZigbeeCoreRecoveryEntity watchdogEntity, bool status);

    /**
     * @brief Pet the watchdog for a specific entity.
     *
     * This function is called to indicate that a specific Zigbee entity is operating
     * normally and should reset its watchdog timer. This prevents the watchdog from
     * triggering recovery actions for healthy entities.
     *
     * @param watchdogEntity The specific entity to pet
     * @return true if the pet was successful, false on error
     */
    bool (*petWatchdog)(ZigbeeCoreRecoveryEntity watchdogEntity);

    /**
     * @brief Restart the ZigbeeCore process.
     *
     * This function is called when the Zigbee subsystem determines that ZigbeeCore
     * needs to be restarted due to failure conditions. The implementation should
     * attempt to restart the ZigbeeCore process and return the appropriate status.
     *
     * @return ZIGBEE_CORE_RESTART_ACTIVE if restart is being attempted,
     *         ZIGBEE_CORE_RESTART_NOT_ACTIVE if restart cannot or will not be performed
     */
    ZigbeeCoreRestartStatus (*restartZigbeeCore)(void);

    /**
     * @brief Notify that device communication has been restored.
     *
     * This function is called when devices that were previously in communication
     * failure have successfully resumed communication. This allows the watchdog
     * system to update its state and potentially cancel pending recovery actions.
     *
     * @return true if the notification was processed successfully, false on error
     */
    bool (*notifyDeviceCommRestored)(void);

    /**
     * @brief Handle ZHAL response events for watchdog monitoring.
     *
     * This function is called for each ZHAL operation response to monitor
     * the health of the Zigbee stack.
     *
     * @param responseType The type of ZHAL operation that completed
     * @param resultCode The ZHAL_STATUS result code from the operation
     */
    void (*zhalResponseHandler)(const char *responseType, ZHAL_STATUS resultCode);

    /**
     * @brief Check if a watchdog recovery action is currently in progress.
     *
     * This function allows subsystems to query whether the watchdog system
     * is currently performing recovery operations (e.g., restarting services).
     *
     * @return true if recovery is in progress, false otherwise
     */
    bool (*getActionInProgress)(void);

} ZigbeeWatchdogDelegate;

/**
 * @brief Create a new ZigbeeWatchdogDelegate instance.
 *
 * Allocates memory for a new watchdog delegate and initializes all function
 * pointers to NULL. The caller is responsible for setting appropriate function
 * pointers after creation.
 *
 * @param name The name of the process being monitored
 * @return Pointer to newly allocated delegate
 * @note Caller must call destroyZigbeeWatchdogDelegate() to free memory
 */
ZigbeeWatchdogDelegate *createZigbeeWatchdogDelegate(void);

/**
 * @brief Destroy a ZigbeeWatchdogDelegate instance.
 *
 * Frees the memory allocated for the watchdog delegate. Does not call any
 * of the function pointers (like shutdownWatchdog) - the caller should
 * perform any necessary cleanup before calling this function.
 *
 * @param delegate Pointer to the delegate to destroy, may be NULL (no-op)
 * @note After calling this function, the delegate pointer is invalid
 */
void destroyZigbeeWatchdogDelegate(ZigbeeWatchdogDelegate *delegate);
