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
// Created by Kevin Funderburg on 8/19/2025.
//

#include "delegate/barton-core-software-watchdog-delegate.h"

G_DEFINE_INTERFACE(BCoreSoftwareWatchdogDelegate, b_core_software_watchdog_delegate, G_TYPE_OBJECT)

static void b_core_software_watchdog_delegate_default_init(BCoreSoftwareWatchdogDelegateInterface *iface)
{
    // Default interface implementation - all function pointers will be NULL initially
    // Concrete implementations will override these
}

//-----------------------------------------------------------------------------
// Public interface function implementations
//-----------------------------------------------------------------------------

gboolean b_core_software_watchdog_delegate_register(BCoreSoftwareWatchdogDelegate *self,
                                                    BartonSoftwareWatchdogContext *context)
{
    g_return_val_if_fail(B_CORE_IS_SOFTWARE_WATCHDOG_DELEGATE(self), FALSE);
    g_return_val_if_fail(context != NULL, FALSE);

    BCoreSoftwareWatchdogDelegateInterface *iface = B_CORE_SOFTWARE_WATCHDOG_DELEGATE_GET_IFACE(self);
    g_return_val_if_fail(iface->watchdog_delegate_register != NULL, FALSE);

    return iface->watchdog_delegate_register(self, context);
}

gboolean b_core_software_watchdog_delegate_unregister(BCoreSoftwareWatchdogDelegate *self,
                                                      BartonSoftwareWatchdogContext *context)
{
    g_return_val_if_fail(B_CORE_IS_SOFTWARE_WATCHDOG_DELEGATE(self), FALSE);
    g_return_val_if_fail(context != NULL, FALSE);

    BCoreSoftwareWatchdogDelegateInterface *iface = B_CORE_SOFTWARE_WATCHDOG_DELEGATE_GET_IFACE(self);
    g_return_val_if_fail(iface->watchdog_delegate_unregister != NULL, FALSE);

    return iface->watchdog_delegate_unregister(self, context);
}

gboolean b_core_software_watchdog_delegate_update_status(BCoreSoftwareWatchdogDelegate *self,
                                                         BartonSoftwareWatchdogContext *context,
                                                         gboolean status)
{
    g_return_val_if_fail(B_CORE_IS_SOFTWARE_WATCHDOG_DELEGATE(self), FALSE);
    g_return_val_if_fail(context != NULL, FALSE);

    BCoreSoftwareWatchdogDelegateInterface *iface = B_CORE_SOFTWARE_WATCHDOG_DELEGATE_GET_IFACE(self);
    g_return_val_if_fail(iface->watchdog_delegate_update_status != NULL, FALSE);

    return iface->watchdog_delegate_update_status(self, context, status);
}

gboolean b_core_software_watchdog_delegate_pet(BCoreSoftwareWatchdogDelegate *self,
                                               BartonSoftwareWatchdogContext *context)
{
    g_return_val_if_fail(B_CORE_IS_SOFTWARE_WATCHDOG_DELEGATE(self), FALSE);
    g_return_val_if_fail(context != NULL, FALSE);

    BCoreSoftwareWatchdogDelegateInterface *iface = B_CORE_SOFTWARE_WATCHDOG_DELEGATE_GET_IFACE(self);
    g_return_val_if_fail(iface->watchdog_delegate_pet != NULL, FALSE);

    return iface->watchdog_delegate_pet(self, context);
}

//-----------------------------------------------------------------------------
// Memory management functions
//-----------------------------------------------------------------------------

BartonSoftwareWatchdogContext *
b_core_software_watchdog_delegate_context_create(BCoreSoftwareWatchdogDelegate *self,
                                                 const gchar *entityName,
                                                 const gchar *serviceName,
                                                 guint32 petFrequencySeconds,
                                                 BartonSoftwareWatchdogRecoveryActionStep *primaryRecoveryStep)
{
    g_return_val_if_fail(B_CORE_IS_SOFTWARE_WATCHDOG_DELEGATE(self), NULL);
    g_return_val_if_fail(entityName != NULL, NULL);
    g_return_val_if_fail(serviceName != NULL, NULL);

    BCoreSoftwareWatchdogDelegateInterface *iface = B_CORE_SOFTWARE_WATCHDOG_DELEGATE_GET_IFACE(self);
    g_return_val_if_fail(iface->watchdog_delegate_context_create != NULL, NULL);

    return iface->watchdog_delegate_context_create(
        self, entityName, serviceName, petFrequencySeconds, primaryRecoveryStep);
}

BartonSoftwareWatchdogContext *b_core_software_watchdog_delegate_context_create_for_failure_count(
    BCoreSoftwareWatchdogDelegate *self,
    const gchar *entityName,
    const gchar *serviceName,
    BartonSoftwareWatchdogRecoveryActionStep *primaryRecoveryStep)
{
    g_return_val_if_fail(B_CORE_IS_SOFTWARE_WATCHDOG_DELEGATE(self), NULL);
    g_return_val_if_fail(entityName != NULL, NULL);
    g_return_val_if_fail(serviceName != NULL, NULL);

    BCoreSoftwareWatchdogDelegateInterface *iface = B_CORE_SOFTWARE_WATCHDOG_DELEGATE_GET_IFACE(self);
    g_return_val_if_fail(iface->watchdog_delegate_context_create_for_failure_count != NULL, NULL);

    return iface->watchdog_delegate_context_create_for_failure_count(
        self, entityName, serviceName, primaryRecoveryStep);
}

BartonSoftwareWatchdogContext *b_core_software_watchdog_delegate_context_acquire(BCoreSoftwareWatchdogDelegate *self,
                                                                                 BartonSoftwareWatchdogContext *context)
{
    g_return_val_if_fail(B_CORE_IS_SOFTWARE_WATCHDOG_DELEGATE(self), NULL);
    g_return_val_if_fail(context != NULL, NULL);

    BCoreSoftwareWatchdogDelegateInterface *iface = B_CORE_SOFTWARE_WATCHDOG_DELEGATE_GET_IFACE(self);
    g_return_val_if_fail(iface->watchdog_delegate_context_acquire != NULL, NULL);

    return iface->watchdog_delegate_context_acquire(self, context);
}

void b_core_software_watchdog_delegate_context_release(BCoreSoftwareWatchdogDelegate *self,
                                                       BartonSoftwareWatchdogContext *context)
{
    g_return_if_fail(B_CORE_IS_SOFTWARE_WATCHDOG_DELEGATE(self));
    g_return_if_fail(context != NULL);

    BCoreSoftwareWatchdogDelegateInterface *iface = B_CORE_SOFTWARE_WATCHDOG_DELEGATE_GET_IFACE(self);
    g_return_if_fail(iface->watchdog_delegate_context_release != NULL);

    iface->watchdog_delegate_context_release(self, context);
}

//-----------------------------------------------------------------------------
// Recovery action step functions
//-----------------------------------------------------------------------------

BartonSoftwareWatchdogRecoveryActionStep *
b_core_software_watchdog_delegate_recovery_action_step_create(BCoreSoftwareWatchdogDelegate *self,
                                                              BartonSoftwareWatchdogRecoveryAction action,
                                                              const gchar *message,
                                                              guint32 failuresBeforeTrigger)
{
    g_return_val_if_fail(B_CORE_IS_SOFTWARE_WATCHDOG_DELEGATE(self), NULL);

    BCoreSoftwareWatchdogDelegateInterface *iface = B_CORE_SOFTWARE_WATCHDOG_DELEGATE_GET_IFACE(self);
    g_return_val_if_fail(iface->watchdog_delegate_recovery_action_step_create != NULL, NULL);

    return iface->watchdog_delegate_recovery_action_step_create(self, action, message, failuresBeforeTrigger);
}

BartonSoftwareWatchdogRecoveryActionStep *
b_core_software_watchdog_delegate_recovery_action_step_create_with_trouble(BCoreSoftwareWatchdogDelegate *self,
                                                                           gint troubleCode,
                                                                           const gchar *message,
                                                                           gboolean includeDiag,
                                                                           guint32 delay_in_seconds)
{
    g_return_val_if_fail(B_CORE_IS_SOFTWARE_WATCHDOG_DELEGATE(self), NULL);

    BCoreSoftwareWatchdogDelegateInterface *iface = B_CORE_SOFTWARE_WATCHDOG_DELEGATE_GET_IFACE(self);
    g_return_val_if_fail(iface->watchdog_delegate_recovery_action_step_create_with_trouble != NULL, NULL);

    return iface->watchdog_delegate_recovery_action_step_create_with_trouble(
        self, troubleCode, message, includeDiag, delay_in_seconds);
}

void b_core_software_watchdog_delegate_recovery_action_step_destroy(BCoreSoftwareWatchdogDelegate *self,
                                                                    BartonSoftwareWatchdogRecoveryActionStep *step)
{
    g_return_if_fail(B_CORE_IS_SOFTWARE_WATCHDOG_DELEGATE(self));
    g_return_if_fail(step != NULL);

    BCoreSoftwareWatchdogDelegateInterface *iface = B_CORE_SOFTWARE_WATCHDOG_DELEGATE_GET_IFACE(self);
    g_return_if_fail(iface->watchdog_delegate_recovery_action_step_destroy != NULL);

    iface->watchdog_delegate_recovery_action_step_destroy(self, step);
}

gboolean b_core_software_watchdog_delegate_recovery_action_step_append(BCoreSoftwareWatchdogDelegate *self,
                                                                       BartonSoftwareWatchdogContext *context,
                                                                       BartonSoftwareWatchdogRecoveryActionStep *step)
{
    g_return_val_if_fail(B_CORE_IS_SOFTWARE_WATCHDOG_DELEGATE(self), FALSE);
    g_return_val_if_fail(context != NULL, FALSE);
    g_return_val_if_fail(step != NULL, FALSE);

    BCoreSoftwareWatchdogDelegateInterface *iface = B_CORE_SOFTWARE_WATCHDOG_DELEGATE_GET_IFACE(self);
    g_return_val_if_fail(iface->watchdog_delegate_recovery_action_step_append != NULL, FALSE);

    return iface->watchdog_delegate_recovery_action_step_append(self, context, step);
}

gboolean b_core_software_watchdog_delegate_reset_failure_counts(BCoreSoftwareWatchdogDelegate *self,
                                                                BartonSoftwareWatchdogContext *context,
                                                                gboolean allSteps)
{
    g_return_val_if_fail(B_CORE_IS_SOFTWARE_WATCHDOG_DELEGATE(self), FALSE);
    g_return_val_if_fail(context != NULL, FALSE);

    BCoreSoftwareWatchdogDelegateInterface *iface = B_CORE_SOFTWARE_WATCHDOG_DELEGATE_GET_IFACE(self);
    g_return_val_if_fail(iface->watchdog_delegate_reset_failure_counts != NULL, FALSE);

    return iface->watchdog_delegate_reset_failure_counts(self, context, allSteps);
}

gboolean b_core_software_watchdog_delegate_immediate_recovery_service_with_trouble(BCoreSoftwareWatchdogDelegate *self,
                                                                                   const gchar *serviceName,
                                                                                   const gchar *entityName,
                                                                                   const gchar *troubleString,
                                                                                   gint troubleCode)
{
    g_return_val_if_fail(B_CORE_IS_SOFTWARE_WATCHDOG_DELEGATE(self), FALSE);
    g_return_val_if_fail(serviceName != NULL, FALSE);
    g_return_val_if_fail(entityName != NULL, FALSE);

    BCoreSoftwareWatchdogDelegateInterface *iface = B_CORE_SOFTWARE_WATCHDOG_DELEGATE_GET_IFACE(self);
    g_return_val_if_fail(iface->watchdog_delegate_immediate_recovery_service_with_trouble != NULL, FALSE);

    return iface->watchdog_delegate_immediate_recovery_service_with_trouble(
        self, serviceName, entityName, troubleString, troubleCode);
}

//-----------------------------------------------------------------------------
// Event listener function
//-----------------------------------------------------------------------------

void b_core_software_watchdog_delegate_register_BartonSoftwareWatchdogEvent_event_listener(
    BCoreSoftwareWatchdogDelegate *self,
    handleEvent_BartonSoftwareWatchdogEvent listener)
{
    g_return_if_fail(B_CORE_IS_SOFTWARE_WATCHDOG_DELEGATE(self));
    g_return_if_fail(listener != NULL);

    // TODO: Implement event listener registration
    // This might need to be stored in a private structure or handled differently
    // depending on how events are managed in the system
}
