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
// Created by Kevin Funderburg on 8/20/2025.
//

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "private/softwareWatchdogDelegate/softwareWatchdogDelegate.h"

typedef enum
{
    ZIGBEE_CORE_RECOVERY_ENITITY_HEARTBEAT,
    ZIGBEE_CORE_RECOVERY_ENITITY_COMM_FAIL,
    ZIGBEE_CORE_RECOVERY_ENITITY_NETWORK_BUSY
} ZigbeeCoreRecoveryEntity;

extern const char *ZigbeeCoreRecoveryEntityLabels[];

extern const char *zigbeeCoreRecoveryReasonLabels[];

// Set a default for watchdog run interval, could make this configurable later
#define ZIGBEE_CORE_HEARTBEAT_PET_FREQUENCY_SECS 60

/**
 * @brief Zigbee-specific software watchdog delegate interface.
 *
 * This structure extends the base BartonSoftwareWatchdogDelegate with Zigbee-specific
 * watchdog functionality. It provides specialized operations for monitoring and
 * recovering ZigbeeCore processes, handling communication failures, and coordinating
 * recovery actions specific to Zigbee network operations.
 */
typedef struct ZigbeeSoftwareWatchdogDelegate
{
    /**
     * @brief Base watchdog delegate providing core functionality.
     *
     * Contains the fundamental watchdog operations (initialize, shutdown, tickle, etc.)
     * that are common across all subsystems. The Zigbee delegate composes this base
     * delegate to inherit standard watchdog behavior.
     */
    BartonSoftwareWatchdogDelegate *baseDelegate;

    /**
     * @brief Update the status of a specific Zigbee watchdog entity.
     *
     * This function allows the Zigbee subsystem to report the health status of
     * various monitored entities (heartbeat, network busy, communication failure).
     * The watchdog system can use this information to determine if recovery
     * actions are needed.
     *
     * @param ZigbeeCoreRecoveryEntity The specific Zigbee entity being reported on
     * @param status true if the entity is healthy, false if it's in a failure state
     * @return true if the status update was processed successfully, false on error
     * @note May be NULL if status reporting is not supported
     */
    bool (*updateWatchdogStatus)(ZigbeeCoreRecoveryEntity watchdogEntity, bool status);

    /**
     * @brief Immediately trigger service recovery with detailed trouble information.
     *
     * This function provides a mechanism to request immediate recovery of a service
     * while also reporting specific trouble codes and descriptions. This is typically
     * used when the Zigbee subsystem detects critical failures that require
     * immediate intervention rather than waiting for normal watchdog timeouts.
     *
     * @param serviceName Name of the service requiring recovery (e.g., "ZigbeeCore")
     * @param entityName Name of the specific entity that failed (e.g., "heartbeat")
     * @param troubleString Human-readable description of the problem
     * @param troubleCode Numeric trouble code for categorization and tracking
     * @return true if the recovery request was accepted, false on error
     * @note May be NULL if immediate recovery is not supported
     */
    bool (*immediateRecoveryServiceWithTrouble)(const char *serviceName,
                                                const char *entityName,
                                                const char *troubleString,
                                                int troubleCode);

    /**
     * @brief Signal a monitoring thread for a specific Zigbee watchdog entity.
     *
     * This function allows the subsystem to wake up or signal monitoring threads
     * that are responsible for specific Zigbee entities. This can be used to
     * trigger immediate checks, wake threads from timed waits, or coordinate
     * monitoring activities across different watchdog entities.
     *
     * @param entity The specific Zigbee watchdog entity whose thread should be signaled
     * @note May be NULL if thread signaling is not supported
     */
    void (*signalThread)(ZigbeeCoreRecoveryEntity entity);

} ZigbeeSoftwareWatchdogDelegate;

/**
 * @brief Create a new ZigbeeSoftwareWatchdogDelegate instance.
 *
 * Allocates memory for a new Zigbee watchdog delegate and initializes it with
 * a base BartonSoftwareWatchdogDelegate. All Zigbee-specific function pointers
 * are initialized to NULL and must be set by the caller after creation.
 *
 * @return Pointer to newly allocated Zigbee delegate, or NULL on allocation failure
 * @note Caller must call destroyZigbeeSoftwareWatchdogDelegate() to free memory
 */
ZigbeeSoftwareWatchdogDelegate *createZigbeeSoftwareWatchdogDelegate(void);

/**
 * @brief Destroy a ZigbeeSoftwareWatchdogDelegate instance.
 *
 * Frees the memory allocated for the Zigbee watchdog delegate and its composed
 * base delegate.
 *
 * @param delegate Pointer to the Zigbee delegate to destroy, may be NULL (no-op)
 * @note Does not call any function pointers - caller should perform necessary
 *       cleanup (like calling shutdownWatchdog) before destruction
 * @note After calling this function, the delegate pointer is invalid
 */
void destroyZigbeeSoftwareWatchdogDelegate(ZigbeeSoftwareWatchdogDelegate *delegate);
