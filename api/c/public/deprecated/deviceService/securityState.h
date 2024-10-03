//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2019 Comcast Corporation
//
// All rights reserved.
//
// This software is protected by copyright laws of the United States
// and of foreign countries. This material may also be protected by
// patent laws of the United States and of foreign countries.
//
// This software is furnished under a license agreement and/or a
// nondisclosure agreement and may only be used or copied in accordance
// with the terms of those agreements.
//
// The mere transfer of this software does not imply any licenses of trade
// secrets, proprietary technology, copyrights, patents, trademarks, or
// any other form of intellectual property whatsoever.
//
// Comcast Corporation retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------

#ifndef ZILKER_SECURITYSTATE_H
#define ZILKER_SECURITYSTATE_H

#include <icUtil/array.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define SECURITY_STATE_PANEL_STATUS  "panelStatus"
#define SECURITY_STATE_INDICATION    "indication"
#define SECURITY_STATE_TIME_LEFT     "timeLeft"
#define SECURITY_STATE_BYPASS_ACTIVE "bypassActive"

#define ENUM_LABEL(x)                x
#define ENUM_VALUES                                                                                                    \
    ENUM_LABEL(PANEL_STATUS_INVALID), ENUM_LABEL(PANEL_STATUS_DISARMED), ENUM_LABEL(PANEL_STATUS_ARMED_STAY),          \
        ENUM_LABEL(PANEL_STATUS_ARMING_STAY), ENUM_LABEL(PANEL_STATUS_ARMED_AWAY),                                     \
        ENUM_LABEL(PANEL_STATUS_ARMING_AWAY), ENUM_LABEL(PANEL_STATUS_ARMED_NIGHT),                                    \
        ENUM_LABEL(PANEL_STATUS_ARMING_NIGHT), ENUM_LABEL(PANEL_STATUS_UNREADY), ENUM_LABEL(PANEL_STATUS_ALARM_NONE),  \
        ENUM_LABEL(PANEL_STATUS_ALARM_BURG), ENUM_LABEL(PANEL_STATUS_ALARM_AUDIBLE),                                   \
        ENUM_LABEL(PANEL_STATUS_ALARM_FIRE), ENUM_LABEL(PANEL_STATUS_ALARM_MEDICAL),                                   \
        ENUM_LABEL(PANEL_STATUS_ALARM_CO), ENUM_LABEL(PANEL_STATUS_ALARM_POLICE), ENUM_LABEL(PANEL_STATUS_PANIC_FIRE), \
        ENUM_LABEL(PANEL_STATUS_PANIC_MEDICAL), ENUM_LABEL(PANEL_STATUS_PANIC_POLICE),                                 \
        ENUM_LABEL(PANEL_STATUS_EXIT_DELAY), ENUM_LABEL(PANEL_STATUS_ENTRY_DELAY),                                     \
        ENUM_LABEL(PANEL_STATUS_ENTRY_DELAY_ONESHOT)

typedef enum
{
    ENUM_VALUES
} PanelStatus;

#undef ENUM_LABEL
#define ENUM_LABEL(x) #x
static const char *const PanelStatusLabels[] = {ENUM_VALUES};
#undef ENUM_LABEL
#undef ENUM_VALUES

#define ENUM_LABEL(x) x
#define ENUM_VALUES                                                                                                    \
    ENUM_LABEL(SECURITY_INDICATION_INVALID), ENUM_LABEL(SECURITY_INDICATION_NONE),                                     \
        ENUM_LABEL(SECURITY_INDICATION_AUDIBLE), ENUM_LABEL(SECURITY_INDICATION_VISUAL),                               \
        ENUM_LABEL(SECURITY_INDICATION_BOTH)

typedef enum
{
    ENUM_VALUES
} SecurityIndication;

#undef ENUM_LABEL
#define ENUM_LABEL(x) #x

static const char *const SecurityIndicationLabels[] = {ENUM_VALUES};
#undef ENUM_LABEL
#undef ENUM_VALUES

SecurityIndication securityIndicationValueOf(const char *indicationLabel);
PanelStatus panelStatusValueOf(const char *panelStatusLabel);

typedef struct
{
    const PanelStatus panelStatus;
    /**
     * This indicates the time left for panel statuses that care about time remaining. E.g., arming/entry/exit.
     * It SHOULD be set to the default time remaining for quiescent statuses, e.g., disarmed/armed
     */
    const uint8_t timeLeft;
    /**
     * The kind of indication to make
     */
    const SecurityIndication indication;

    /**
     * True when at least one zone is bypassed
     */
    const bool bypassActive;
} SecurityState;

/**
 * Create an immutable security state
 * @return a heap allocated event pointer
 * @see deviceServiceSecurityEventDestroy
 */
SecurityState *
securityStateCreate(PanelStatus panelStatus, uint32_t timeLeft, SecurityIndication indication, bool bypassActive);

/**
 * Clone a security state
 * @param event
 * @return A heap allocated event pointer
 * @see deviceServiceSecurityEventDestroy
 */
SecurityState *securityStateClone(const SecurityState *event);

/**
 * Safely copy one device service security state to another
 * @param dst
 * @param src
 * @return EINVAL when either or both arguments are NULL, otherwise 0
 */
int securityStateCopy(SecurityState *dst, const SecurityState *src);

/**
 * Safely discard a security state. It does NOT free the state pointer itself
 */
void securityStateDestroy(SecurityState *state);

/**
 * For use with auto cleanup. This WILL free the state pointer.
 */
inline void securityStateDestroy__auto(SecurityState **state)
{
    securityStateDestroy(*state);
    free(*state);
}

#define scoped_SecurityState AUTO_CLEAN(securityStateDestroy__auto) SecurityState

/**
 * Serialize a security state to JSON. The result must be freed.
 */
const char *securityStateToJSON(SecurityState *state);

/**
 * Deserialize a JSON security state
 * @note the caller must free the state pointer
 * @see securityStateDestroy
 * @return NULL when parsing fails
 */
SecurityState *securityStateFromJSON(const char *json);

#endif // ZILKER_SECURITYSTATE_H
