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

#ifndef OBSERVABILITY_INIT_H
#define OBSERVABILITY_INIT_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef BARTON_CONFIG_OBSERVABILITY

/**
 * Initialize all OpenTelemetry providers (tracer, meter, logger).
 * Reads configuration from standard OTel environment variables:
 *   - OTEL_EXPORTER_OTLP_ENDPOINT (default: http://localhost:4318)
 *   - OTEL_SERVICE_NAME (default: barton-core)
 *   - OTEL_RESOURCE_ATTRIBUTES
 *
 * @return 0 on success, non-zero on failure
 */
int observabilityInit(void);

/**
 * Flush pending observability data and shut down all providers.
 * Safe to call even if init was not called or failed.
 */
void observabilityShutdown(void);

#else /* !BARTON_CONFIG_OBSERVABILITY */

static inline int observabilityInit(void)
{
    return 0;
}
static inline void observabilityShutdown(void)
{
    (void) 0;
}

#endif /* BARTON_CONFIG_OBSERVABILITY */

#ifdef __cplusplus
}
#endif

#endif /* OBSERVABILITY_INIT_H */
