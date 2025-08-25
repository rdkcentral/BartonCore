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
// Created by Kevin Funderburg on 8/22/2025.
//

#pragma once

#include <stdbool.h>

/**
 * @brief Base software watchdog delegate interface for Barton Core subsystems.
 *
 * This struct provides a standardized interface for subsystems to interact with
 * external software watchdog systems.
 *
 * The delegate pattern allows subsystems to:
 * - Initialize and configure their watchdog monitoring
 * - Periodically "tickle" the watchdog to indicate healthy operation
 * - Check if recovery operations are in progress
 * - Cleanly shutdown watchdog monitoring
 *
 * @note All function pointers may be NULL if the functionality is not needed
 *       or not available. Always check for NULL before calling.
 */
typedef struct BartonSoftwareWatchdogDelegate
{
    /**
     * @brief Initialize the software watchdog system.
     *
     * This function is called once during subsystem initialization to set up
     * watchdog monitoring. It should configure any necessary watchdog entities,
     * timers, or monitoring threads.
     *
     * @note May be NULL if no initialization is required
     */
    void (*initializeWatchdog)(void);

    /**
     * @brief Shutdown the software watchdog system.
     *
     * This function is called during subsystem shutdown to cleanly disable
     * watchdog monitoring, stop any background threads, and free resources.
     * Should ensure no false watchdog triggers occur during shutdown.
     *
     * @note May be NULL if no shutdown cleanup is required
     */
    void (*shutdownWatchdog)(void);

    /**
     * @brief Tickle/pet the watchdog to indicate healthy operation.
     *
     * @note May be NULL if not needed
     */
    void (*tickleWatchdog)(void);

    /**
     * @brief Check if a watchdog recovery action is currently in progress.
     *
     * This function allows subsystems to query whether the watchdog system
     * is currently performing recovery operations (e.g., restarting services).
     *
     * @return true if recovery is in progress, false otherwise
     * @note May be NULL if recovery status checking is not supported
     */
    bool (*getActionInProgress)(void);

    /**
     * @brief Name of the process being monitored by this watchdog.
     *
     * This string identifies the specific process or subsystem that this
     * watchdog delegate is responsible for monitoring. Used for logging,
     * debugging, and recovery coordination.
     *
     * @note REQUIRED - Must not be NULL
     */
    const char *processName;

} BartonSoftwareWatchdogDelegate;

/**
 * @brief Create a new BartonSoftwareWatchdogDelegate instance.
 *
 * Allocates memory for a new watchdog delegate and initializes all function
 * pointers to NULL. The caller is responsible for setting appropriate function
 * pointers after creation.
 *
 * @param name The name of the process being monitored
 * @return Pointer to newly allocated delegate, or NULL on error
 * @note Caller must call destroyBartonSoftwareWatchdogDelegate() to free memory
 */
BartonSoftwareWatchdogDelegate *createBartonSoftwareWatchdogDelegate(const char *name);

/**
 * @brief Destroy a BartonSoftwareWatchdogDelegate instance.
 *
 * Frees the memory allocated for the watchdog delegate. Does not call any
 * of the function pointers (like shutdownWatchdog) - the caller should
 * perform any necessary cleanup before calling this function.
 *
 * @param delegate Pointer to the delegate to destroy, may be NULL (no-op)
 * @note After calling this function, the delegate pointer is invalid
 */
void destroyBartonSoftwareWatchdogDelegate(BartonSoftwareWatchdogDelegate *delegate);
