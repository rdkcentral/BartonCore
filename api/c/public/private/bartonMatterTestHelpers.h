//------------------------------ tabstop = 4 ----------------------------------
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
//------------------------------ tabstop = 4 ----------------------------------

#ifndef BARTON_MATTER_TEST_HELPERS_H
#define BARTON_MATTER_TEST_HELPERS_H

/**
 * @file bartonMatterTestHelpers.h
 *
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * WARNING: THIS HEADER IS FOR INTEGRATION TEST USE ONLY.
 *
 * Every symbol declared here exists solely to enable fast, deterministic
 * integration tests.  NONE of these functions may be called from production
 * code, from the public Barton C API surface, or from any Barton client
 * application.
 *
 * The functions declared here manipulate internal Matter SDK state in ways
 * that are unsafe at arbitrary times and have no defined semantics outside
 * of controlled test scenarios.
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * [FOR INTEGRATION TEST USE ONLY]
 *
 * Trigger immediate auto-resubscription for a commissioned Matter device.
 *
 * Overrides the ReadClient's liveness timeout to 1 ms so that the liveness
 * timer fires immediately on the next Matter event-loop tick.  This causes
 * DefaultResubscribePolicy to schedule a new CASE session and deliver a
 * fresh priming report, which drives communicationRestored → synchronizeDevice
 * — the same path exercised after a real comm-fail recovery.  The
 * ClusterStateCache is preserved across the reconnect so cached attribute
 * values remain accessible until the new priming report arrives.
 *
 * DeviceDataCache::OnSubscriptionEstablished resets the override to
 * Clock::kZero so subsequent subscriptions use the naturally negotiated
 * liveness window and do not loop.
 *
 * Intended solely for integration tests that need to exercise the
 * communicationRestored → synchronizeDevice path without waiting for the
 * full negotiated liveness window (which can be tens of seconds).
 *
 * MUST NOT be called from production code or any Barton client application.
 *
 * @param uuid The device UUID as registered in the device service.
 */
void bartonMatterDeviceForceResubscription(const char *uuid);

#ifdef __cplusplus
}
#endif

#endif /* BARTON_MATTER_TEST_HELPERS_H */
