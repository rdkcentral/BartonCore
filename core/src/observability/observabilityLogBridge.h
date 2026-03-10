//------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2024 Comcast Cable Communications Management, LLC
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
//------------------------------ tabstop = 4 ----------------------------------

#ifndef OBSERVABILITY_LOG_BRIDGE_H
#define OBSERVABILITY_LOG_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef BARTON_CONFIG_OBSERVABILITY

/**
 * Initialize the OTel log bridge. Call after observabilityInit().
 * Registers a hook so that icLog messages are also emitted as OTel log records.
 *
 * @return 0 on success, non-zero on failure
 */
int observabilityLogBridgeInit(void);

/**
 * Emit a log record through the OpenTelemetry Logs SDK.
 * Typically called from the icLog hook, not directly.
 *
 * @param category  Log category / scope name (e.g., "deviceService")
 * @param priority  icLog priority (maps to OTel severity)
 * @param file      Source file name
 * @param func      Function name
 * @param line      Source line number
 * @param message   Formatted log message
 */
void observabilityLogBridgeEmit(const char *category,
                        int priority,
                        const char *file,
                        const char *func,
                        int line,
                        const char *message);

/**
 * Shut down the OTel log bridge and deregister the icLog hook.
 */
void observabilityLogBridgeShutdown(void);

#else /* !BARTON_CONFIG_OBSERVABILITY */

static inline int observabilityLogBridgeInit(void)
{
    return 0;
}
static inline void observabilityLogBridgeEmit(const char *category,
                                      int priority,
                                      const char *file,
                                      const char *func,
                                      int line,
                                      const char *message)
{
    (void) category;
    (void) priority;
    (void) file;
    (void) func;
    (void) line;
    (void) message;
}
static inline void observabilityLogBridgeShutdown(void)
{
    (void) 0;
}

#endif /* BARTON_CONFIG_OBSERVABILITY */

#ifdef __cplusplus
}
#endif

#endif /* OBSERVABILITY_LOG_BRIDGE_H */
