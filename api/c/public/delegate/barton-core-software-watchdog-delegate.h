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
// Created by Kevin Funderburg on 8/15/2025.
//

#pragma once

#include <glib-object.h>

/*
 * available event codes/values
 */

#define BARTON_WATCHDOG_INIT_COMPLETE                                                                                  \
    100 // sent after ALL services started or restarted.  see eventValue for details as to why
#define BARTON_WATCHDOG_SERVICE_STATE_CHANGED                                                                          \
    101 // sent if 'a single service' has a state change.  see eventValue for further detail
#define BARTON_WATCHDOG_GROUP_STATE_CHANGED                                                                            \
    102 // sent if 'a group of services' had a state change.  see eventValue for further detail
#define BARTON_WATCHDOG_EVENT_VALUE_ALL_SERVICES_STARTED                                                               \
    1 // WATCHDOG_INIT_COMPLETE eventValue when 'all services' COMPLETED startup
#define BARTON_WATCHDOG_EVENT_VALUE_SOME_SERVICES_STARTED                                                              \
    2 // WATCHDOG_INIT_COMPLETE eventValue when 'one or more services' COMPLETED startup (after crash or bounce)
#define BARTON_WATCHDOG_EVENT_ALL_SERVICES_PHASE2_COMPLETE                                                             \
    3 // WATCHDOG_INIT_COMPLETE eventValue when 'all services' have completed phase2
#define BARTON_WATCHDOG_EVENT_SERVICE_PHASE2_COMPLETE                                                                  \
    4 // WATCHDOG_INIT_COMPLETE eventValue for a specific service completing phase2
#define BARTON_WATCHDOG_EVENT_VALUE_ACTION_START                                                                       \
    10 // WATCHDOG_*_STATE_CHANGED eventValue when one or more services were started
#define BARTON_WATCHDOG_EVENT_VALUE_ACTION_DEATH                                                                       \
    11 // WATCHDOG_*_STATE_CHANGED eventValue when one or more services died
#define BARTON_WATCHDOG_EVENT_VALUE_ACTION_RESTART                                                                     \
    12 // WATCHDOG_*_STATE_CHANGED eventValue when one or more services were restarted

typedef enum
{
    BARTON_SOFTWARE_WATCHDOG_RECOVERY_ACTION_INVALID = -1,       // invalid value
    BARTON_SOFTWARE_WATCHDOG_RECOVERY_ACTION_NONE,               // silently ignore
    BARTON_SOFTWARE_WATCHDOG_RECOVERY_ACTION_LOG,                // just log a message and ignore
    BARTON_SOFTWARE_WATCHDOG_RECOVERY_ACTION_RESTART_PROCESS,    // restart the related process
    BARTON_SOFTWARE_WATCHDOG_RECOVERY_ACTION_RESTART_EVERYTHING, // restart all of our processes
    BARTON_SOFTWARE_WATCHDOG_RECOVERY_ACTION_REBOOT, // reboot the entire device if supported.  if not, restart
                                                     // everything
    BARTON_SOFTWARE_WATCHDOG_RECOVERY_ACTION_SOFTWARE_TROUBLE =
        255 // it is only selectable through softwareWatchdogRecoveryActionStepCreateWithTrouble function
} BartonSoftwareWatchdogRecoveryAction;

typedef struct BartonSoftwareWatchdogContext
{
    gpointer delegateContext; // The opaque context from the delegate
    gchar *entityName;        // For debugging/logging
} BartonSoftwareWatchdogContext;

typedef struct BartonSoftwareWatchdogRecoveryActionStep
{
    gpointer delegateContext; // The opaque context from the delegate
} BartonSoftwareWatchdogRecoveryActionStep;

typedef struct BartonSoftwareWatchdogBaseEvent
{
    guint64 eventId;   // unique event identifier
    gint32 eventCode;  // code used to describe the event so receivers know how to decode (ex: ARM_EVENT_CODE)
    gint32 eventValue; // auxiliary value to augment the eventCode however the event deems necessary (ex: percent
                       // complete of an UPGRADE_DOWNLOAD_EVENT)
    struct timespec eventTime; // when the event occurred
} BartonSoftwareWatchdogBaseEvent;

typedef struct BartonSoftwareWatchdogEvent
{
    BartonSoftwareWatchdogBaseEvent baseEvent;
    gchar *name;
} BartonSoftwareWatchdogEvent;

G_BEGIN_DECLS

#define B_CORE_SOFTWARE_WATCHDOG_DELEGATE_TYPE (b_core_software_watchdog_delegate_get_type())
G_DECLARE_INTERFACE(BCoreSoftwareWatchdogDelegate,
                    b_core_software_watchdog_delegate,
                    B_CORE,
                    SOFTWARE_WATCHDOG_DELEGATE,
                    GObject);

struct _BCoreSoftwareWatchdogDelegateInterface
{
    GTypeInterface parent_iface;

    gboolean (*watchdog_delegate_register)(BCoreSoftwareWatchdogDelegate *self, BartonSoftwareWatchdogContext *context);

    gboolean (*watchdog_delegate_unregister)(BCoreSoftwareWatchdogDelegate *self,
                                             BartonSoftwareWatchdogContext *context);

    gboolean (*watchdog_delegate_update_status)(BCoreSoftwareWatchdogDelegate *self,
                                                BartonSoftwareWatchdogContext *context,
                                                gboolean status);

    gboolean (*watchdog_delegate_pet)(BCoreSoftwareWatchdogDelegate *self, BartonSoftwareWatchdogContext *context);

    // memory management
    BartonSoftwareWatchdogContext *(*watchdog_delegate_context_create)(
        BCoreSoftwareWatchdogDelegate *self,
        const gchar *entityName,
        const gchar *serviceName,
        guint32 petFrequencySeconds,
        BartonSoftwareWatchdogRecoveryActionStep *primaryRecoveryStep);

    BartonSoftwareWatchdogContext *(*watchdog_delegate_context_create_for_failure_count)(
        BCoreSoftwareWatchdogDelegate *self,
        const gchar *entityName,
        const gchar *serviceName,
        BartonSoftwareWatchdogRecoveryActionStep *primaryRecoveryStep);

    BartonSoftwareWatchdogContext *(*watchdog_delegate_context_acquire)(BCoreSoftwareWatchdogDelegate *self,
                                                                        BartonSoftwareWatchdogContext *context);

    void (*watchdog_delegate_context_release)(BCoreSoftwareWatchdogDelegate *self,
                                              BartonSoftwareWatchdogContext *context);

    // recovery action stuff
    BartonSoftwareWatchdogRecoveryActionStep *(*watchdog_delegate_recovery_action_step_create)(
        BCoreSoftwareWatchdogDelegate *self,
        BartonSoftwareWatchdogRecoveryAction action,
        const gchar *message,
        guint32 failuresBeforeTrigger);

    BartonSoftwareWatchdogRecoveryActionStep *(*watchdog_delegate_recovery_action_step_create_with_trouble)(
        BCoreSoftwareWatchdogDelegate *self,
        gint troubleCode,
        const gchar *message,
        gboolean includeDiag,
        guint32 delay_in_seconds);

    void (*watchdog_delegate_recovery_action_step_destroy)(BCoreSoftwareWatchdogDelegate *self,
                                                           BartonSoftwareWatchdogRecoveryActionStep *step);

    gboolean (*watchdog_delegate_recovery_action_step_append)(BCoreSoftwareWatchdogDelegate *self,
                                                              BartonSoftwareWatchdogContext *context,
                                                              BartonSoftwareWatchdogRecoveryActionStep *step);

    gboolean (*watchdog_delegate_reset_failure_counts)(BCoreSoftwareWatchdogDelegate *self,
                                                       BartonSoftwareWatchdogContext *context,
                                                       gboolean allSteps);

    gboolean (*watchdog_delegate_immediate_recovery_service_with_trouble)(BCoreSoftwareWatchdogDelegate *self,
                                                                          const gchar *serviceName,
                                                                          const gchar *entityName,
                                                                          const gchar *troubleString,
                                                                          gint troubleCode);
};


gboolean b_core_software_watchdog_delegate_register(BCoreSoftwareWatchdogDelegate *self,
                                                    BartonSoftwareWatchdogContext *context);

gboolean b_core_software_watchdog_delegate_unregister(BCoreSoftwareWatchdogDelegate *self,
                                                      BartonSoftwareWatchdogContext *context);

gboolean b_core_software_watchdog_delegate_update_status(BCoreSoftwareWatchdogDelegate *self,
                                                         BartonSoftwareWatchdogContext *context,
                                                         gboolean status);

gboolean b_core_software_watchdog_delegate_pet(BCoreSoftwareWatchdogDelegate *self,
                                               BartonSoftwareWatchdogContext *context);

// memory management
BartonSoftwareWatchdogContext *
b_core_software_watchdog_delegate_context_create(BCoreSoftwareWatchdogDelegate *self,
                                                 const gchar *entityName,
                                                 const gchar *serviceName,
                                                 guint32 petFrequencySeconds,
                                                 BartonSoftwareWatchdogRecoveryActionStep *primaryRecoveryStep);

BartonSoftwareWatchdogContext *b_core_software_watchdog_delegate_context_create_for_failure_count(
    BCoreSoftwareWatchdogDelegate *self,
    const gchar *entityName,
    const gchar *serviceName,
    BartonSoftwareWatchdogRecoveryActionStep *primaryRecoveryStep);

BartonSoftwareWatchdogContext *
b_core_software_watchdog_delegate_context_acquire(BCoreSoftwareWatchdogDelegate *self,
                                                  BartonSoftwareWatchdogContext *context);

void b_core_software_watchdog_delegate_context_release(BCoreSoftwareWatchdogDelegate *self,
                                                       BartonSoftwareWatchdogContext *context);


// recovery action step stuff
// TODO - need to figure out if these really belong here. Maybe they need to be their own gobject
BartonSoftwareWatchdogRecoveryActionStep *
b_core_software_watchdog_delegate_recovery_action_step_create(BCoreSoftwareWatchdogDelegate *self,
                                                              BartonSoftwareWatchdogRecoveryAction action,
                                                              const gchar *message,
                                                              guint32 failuresBeforeTrigger);

BartonSoftwareWatchdogRecoveryActionStep *
b_core_software_watchdog_delegate_recovery_action_step_create_with_trouble(BCoreSoftwareWatchdogDelegate *self,
                                                                           gint troubleCode,
                                                                           const gchar *message,
                                                                           gboolean includeDiag,
                                                                           guint32 delay_in_seconds);

void b_core_software_watchdog_delegate_recovery_action_step_destroy(BCoreSoftwareWatchdogDelegate *self,
                                                                    BartonSoftwareWatchdogRecoveryActionStep *step);

gboolean b_core_software_watchdog_delegate_recovery_action_step_append(BCoreSoftwareWatchdogDelegate *self,
                                                                       BartonSoftwareWatchdogContext *context,
                                                                       BartonSoftwareWatchdogRecoveryActionStep *step);

gboolean b_core_software_watchdog_delegate_reset_failure_counts(BCoreSoftwareWatchdogDelegate *self,
                                                                BartonSoftwareWatchdogContext *context,
                                                                gboolean allSteps);

gboolean b_core_software_watchdog_delegate_immediate_recovery_service_with_trouble(BCoreSoftwareWatchdogDelegate *self,
                                                                                   const gchar *serviceName,
                                                                                   const gchar *entityName,
                                                                                   const gchar *troubleString,
                                                                                   gint troubleCode);


// TODO - this should probably live elsewhere
/*
 * function signature of the 'listener' to call when
 * watchdogEvent events are received by watchdogService
 */
typedef void (*handleEvent_BartonSoftwareWatchdogEvent)(BartonSoftwareWatchdogEvent *event);

void b_core_software_watchdog_delegate_register_BartonSoftwareWatchdogEvent_event_listener(
    BCoreSoftwareWatchdogDelegate *self,
    handleEvent_BartonSoftwareWatchdogEvent listener);

// G_DEFINE_AUTOPTR_CLEANUP_FUNC(BartonSoftwareWatchdogContext, b_core_software_watchdog_delegate_context_release)

G_END_DECLS
