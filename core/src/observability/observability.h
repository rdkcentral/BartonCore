// ------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2026 Comcast Cable Communications Management, LLC
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

#ifndef OBSERVABILITY_INIT_H
#define OBSERVABILITY_INIT_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the observability subsystem.
 * Behavior depends on the compiled backend (in-memory or noop).
 *
 * @return 0 on success, non-zero on failure
 */
int observabilityInit(void);

/**
 * Shut down the observability subsystem and release all instruments.
 * Safe to call even if init was not called or failed.
 */
void observabilityShutdown(void);

/**
 * Dump all registered metrics as a JSON string.
 * Caller must free the returned string with free().
 *
 * @return JSON string, or NULL on failure
 */
char *observabilityDumpJson(void);

#ifdef __cplusplus
}
#endif

#endif /* OBSERVABILITY_INIT_H */
