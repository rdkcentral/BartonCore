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
//


#ifdef BARTON_CONFIG_ZIGBEE

#include "zigbeeClusters/iasACECluster.h"
#include "zigbeeClusters/iasZoneCluster.h"
#include <subsystems/zigbee/zigbeeSubsystem.h>
#include <icLog/logging.h>
#include <subsystems/zigbee/zigbeeIO.h>
#include <icUtil/array.h>
#include <icUtil/stringUtils.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>

#define LOG_TAG "IASACECluster"

#define AUDIBLE_NOTIF_MUTE    0x00
#define AUDIBLE_NOTIF_DEFAULT 0x01

/* Private Data */
struct IASACECluster
{
    ZigbeeCluster cluster;
    const IASACEClusterCallbacks *callbacks;
    const void *callbackContext;
};

typedef struct
{
    uint8_t armMode;
    char *accessCode;
    uint8_t zoneId;
} ZCLArmCommandPayload;

enum ZCLArmMode
{
    ZCL_ARM_DISARM  = 0x00,
    ZCL_ARM_STAY    = 0x01,
    ZCL_ARM_NIGHT   = 0x02,
    ZCL_ARM_AWAY    = 0x03
};

enum ZCLArmNotification
{
    ZCL_ARM_NOTIF_DISARMED_ALL        = 0x00,
    ZCL_ARM_NOTIF_ARMED_STAY          = 0x01,
    ZCL_ARM_NOTIF_ARMED_NIGHT         = 0x02,
    ZCL_ARM_NOTIF_ARMED_AWAY          = 0x03,
    ZCL_ARM_NOTIF_ARM_CODE_INVALID    = 0x04,
    ZCL_ARM_NOTIF_ARM_NOT_READY       = 0x05,
    ZCL_ARM_NOTIF_ALREADY_DISARMED    = 0x06
};

enum ZCLAlarmStatus
{
    ZCL_ALARM_NONE          = 0x00,
    ZCL_ALARM_BURGLAR       = 0x01,
    ZCL_ALARM_FIRE          = 0x02,
    ZCL_ALARM_EMERG         = 0x03,
    ZCL_ALARM_POLICE_PANIC  = 0x04,
    ZCL_ALARM_FIRE_PANIC    = 0x05,
    ZCL_ALARM_EMERG_PANIC   = 0x06
};

enum ZCLPanelStatus
{
    ZCL_PANEL_STATUS_DISARMED       = 0x00,
    ZCL_PANEL_STATUS_ARMED_STAY     = 0x01,
    ZCL_PANEL_STATUS_ARMED_NIGHT    = 0x02,
    ZCL_PANEL_STATUS_ARMED_AWAY     = 0x03,
    ZCL_PANEL_STATUS_EXIT_DELAY     = 0x04,
    ZCL_PANEL_STATUS_ENTRY_DELAY    = 0x05,
    ZCL_PANEL_STATUS_NOT_READY      = 0x06,
    ZCL_PANEL_STATUS_IN_ALARM       = 0x07,
    ZCL_PANEL_STATUS_ARMING_STAY    = 0x08,
    ZCL_PANEL_STATUS_ARMING_NIGHT   = 0x09,
    ZCL_PANEL_STATUS_ARMING_AWAY    = 0x0a
};

static pthread_mutex_t refCounterMutex = PTHREAD_MUTEX_INITIALIZER;
static uint8_t refCounter;
static icHashMap *zclArmModeToPanelStatus;
static icHashMap *alarmStatusToZCL;
static icHashMap *panelStatusToZCL;
static icHashMap *armNotificationToZCL;

/* Callback implementations */
static bool handleClusterCommand(ZigbeeCluster *cluster, ReceivedClusterCommand *command);
static void destroyCluster(const ZigbeeCluster *ctx);

/* Private Functions */
static int readArmCommandPayload(ReceivedClusterCommand *command, ZCLArmCommandPayload *payload);
static void requestArmDisarm(IASACECluster *cluster, ReceivedClusterCommand *command);
static void requestPanic(IASACECluster *cluster, uint64_t eui64, uint8_t endpointId, PanelStatus panicStatus);

static void addDictionaryEntry(icHashMap *map, int from, int to);

/**
 * Atomically get and increment ref counter
 */
static uint8_t getAndIncrementRefCounter(void);

/**
 * Atomically get and decrement ref counter
 */
//TODO: If flex can get gcc 4.9+ (C11 standard support), we can use standard atomic types without locks.
// Punt to pthread_mutex until then
static uint8_t getAndDecrementRefCounter(void);


ZigbeeCluster *iasACEClusterCreate(const IASACEClusterCallbacks *callbacks, const void *callbackContext)
{
    IASACECluster *_this = calloc(1, sizeof(IASACECluster));

    _this->cluster.clusterId = IAS_ACE_CLUSTER_ID;
    _this->callbackContext = callbackContext;
    _this->cluster.handleClusterCommand = handleClusterCommand;
    _this->cluster.destroy = destroyCluster;
    _this->callbacks = callbacks;

    uint8_t counter = getAndIncrementRefCounter();

    if (counter == 0)
    {
        alarmStatusToZCL = hashMapCreate();
        panelStatusToZCL = hashMapCreate();
        armNotificationToZCL = hashMapCreate();
        zclArmModeToPanelStatus = hashMapCreate();

        addDictionaryEntry(alarmStatusToZCL, PANEL_STATUS_ALARM_BURG, ZCL_ALARM_BURGLAR);
        addDictionaryEntry(alarmStatusToZCL, PANEL_STATUS_ALARM_MEDICAL, ZCL_ALARM_EMERG);
        addDictionaryEntry(alarmStatusToZCL, PANEL_STATUS_ALARM_FIRE, ZCL_ALARM_FIRE);
        addDictionaryEntry(alarmStatusToZCL, PANEL_STATUS_ALARM_NONE, ZCL_ALARM_NONE);
        addDictionaryEntry(alarmStatusToZCL, PANEL_STATUS_PANIC_MEDICAL, ZCL_ALARM_EMERG_PANIC);
        addDictionaryEntry(alarmStatusToZCL, PANEL_STATUS_PANIC_FIRE, ZCL_ALARM_FIRE_PANIC);
        addDictionaryEntry(alarmStatusToZCL, PANEL_STATUS_PANIC_POLICE, ZCL_ALARM_POLICE_PANIC);
        addDictionaryEntry(alarmStatusToZCL, PANEL_STATUS_ALARM_AUDIBLE, ZCL_ALARM_EMERG);
        addDictionaryEntry(alarmStatusToZCL, PANEL_STATUS_ALARM_CO, ZCL_ALARM_EMERG);

        addDictionaryEntry(panelStatusToZCL, PANEL_STATUS_ALARM_BURG, ZCL_PANEL_STATUS_IN_ALARM);
        addDictionaryEntry(panelStatusToZCL, PANEL_STATUS_ALARM_MEDICAL, ZCL_PANEL_STATUS_IN_ALARM);
        addDictionaryEntry(panelStatusToZCL, PANEL_STATUS_ALARM_FIRE, ZCL_PANEL_STATUS_IN_ALARM);
        addDictionaryEntry(panelStatusToZCL, PANEL_STATUS_PANIC_MEDICAL, ZCL_PANEL_STATUS_IN_ALARM);
        addDictionaryEntry(panelStatusToZCL, PANEL_STATUS_PANIC_FIRE, ZCL_PANEL_STATUS_IN_ALARM);
        addDictionaryEntry(panelStatusToZCL, PANEL_STATUS_PANIC_POLICE, ZCL_PANEL_STATUS_IN_ALARM);
        addDictionaryEntry(panelStatusToZCL, PANEL_STATUS_ALARM_CO, ZCL_PANEL_STATUS_IN_ALARM);
        addDictionaryEntry(panelStatusToZCL, PANEL_STATUS_ALARM_AUDIBLE, ZCL_PANEL_STATUS_IN_ALARM);
        addDictionaryEntry(panelStatusToZCL, PANEL_STATUS_ARMED_NIGHT, ZCL_PANEL_STATUS_ARMED_NIGHT);
        addDictionaryEntry(panelStatusToZCL, PANEL_STATUS_ARMED_AWAY, ZCL_PANEL_STATUS_ARMED_AWAY);
        addDictionaryEntry(panelStatusToZCL, PANEL_STATUS_ARMED_STAY, ZCL_PANEL_STATUS_ARMED_STAY);
        addDictionaryEntry(panelStatusToZCL, PANEL_STATUS_DISARMED, ZCL_PANEL_STATUS_DISARMED);
        addDictionaryEntry(panelStatusToZCL, PANEL_STATUS_EXIT_DELAY, ZCL_PANEL_STATUS_EXIT_DELAY);
        addDictionaryEntry(panelStatusToZCL, PANEL_STATUS_ENTRY_DELAY, ZCL_PANEL_STATUS_ENTRY_DELAY);
        addDictionaryEntry(panelStatusToZCL, PANEL_STATUS_ENTRY_DELAY_ONESHOT, ZCL_PANEL_STATUS_ENTRY_DELAY);
        addDictionaryEntry(panelStatusToZCL, PANEL_STATUS_ARMING_NIGHT, ZCL_PANEL_STATUS_ARMING_NIGHT);
        addDictionaryEntry(panelStatusToZCL, PANEL_STATUS_ARMING_AWAY, ZCL_PANEL_STATUS_ARMING_AWAY);
        addDictionaryEntry(panelStatusToZCL, PANEL_STATUS_ARMING_STAY, ZCL_PANEL_STATUS_ARMING_STAY);
        addDictionaryEntry(panelStatusToZCL, PANEL_STATUS_DISARMED, ZCL_PANEL_STATUS_DISARMED);
        addDictionaryEntry(panelStatusToZCL, PANEL_STATUS_UNREADY, ZCL_PANEL_STATUS_NOT_READY);

        addDictionaryEntry(zclArmModeToPanelStatus, ZCL_ARM_DISARM, PANEL_STATUS_DISARMED);
        addDictionaryEntry(zclArmModeToPanelStatus, ZCL_ARM_STAY, PANEL_STATUS_ARMED_STAY);
        addDictionaryEntry(zclArmModeToPanelStatus, ZCL_ARM_AWAY, PANEL_STATUS_ARMED_AWAY);
        addDictionaryEntry(zclArmModeToPanelStatus, ZCL_ARM_NIGHT, PANEL_STATUS_ARMED_NIGHT);

        addDictionaryEntry(armNotificationToZCL, ARM_NOTIF_BAD_ACCESS_CODE, ZCL_ARM_NOTIF_ARM_CODE_INVALID);
        addDictionaryEntry(armNotificationToZCL, ARM_NOTIF_ARMED_NIGHT, ZCL_ARM_NOTIF_ARMED_NIGHT);
        addDictionaryEntry(armNotificationToZCL, ARM_NOTIF_ARMED_ALL, ZCL_ARM_NOTIF_ARMED_AWAY);
        addDictionaryEntry(armNotificationToZCL, ARM_NOTIF_ARMED_HOME, ZCL_ARM_NOTIF_ARMED_STAY);
        addDictionaryEntry(armNotificationToZCL, ARM_NOTIF_ALREADY_DISARMED, ZCL_ARM_NOTIF_ALREADY_DISARMED);
        addDictionaryEntry(armNotificationToZCL, ARM_NOTIF_NOT_READY, ZCL_ARM_NOTIF_ARM_NOT_READY);
        addDictionaryEntry(armNotificationToZCL, ARM_NOTIF_TROUBLE, ZCL_ARM_NOTIF_ARM_NOT_READY);
        addDictionaryEntry(armNotificationToZCL, ARM_NOTIF_DISARMED, ZCL_ARM_NOTIF_DISARMED_ALL);
        addDictionaryEntry(armNotificationToZCL, ARM_NOTIF_ALREADY_ARMED, ZCL_ARM_NOTIF_ARM_NOT_READY);
    }

    return (ZigbeeCluster *) _this;
}

static void destroyCluster(const ZigbeeCluster *ctx)
{
    uint8_t counter = getAndDecrementRefCounter();
    if (counter == 1)
    {
        hashMapDestroy(alarmStatusToZCL, NULL);
        hashMapDestroy(panelStatusToZCL, NULL);
        hashMapDestroy(armNotificationToZCL, NULL);
        hashMapDestroy(zclArmModeToPanelStatus, NULL);
        /* Common driver will free our data object */
    }
}

static bool handleClusterCommand(ZigbeeCluster *cluster, ReceivedClusterCommand *command)
{
    bool handled = false;
    IASACECluster *_this = (IASACECluster *) cluster;

    if (command->clusterId == IAS_ACE_CLUSTER_ID && !command->fromServer)
    {
        switch (command->commandId)
        {
            case IAS_ACE_PANIC_COMMAND_ID:
                requestPanic(_this, command->eui64, command->sourceEndpoint, PANEL_STATUS_PANIC_POLICE);
                break;
            case IAS_ACE_EMERGENCY_COMMAND_ID:
                requestPanic(_this, command->eui64, command->sourceEndpoint, PANEL_STATUS_PANIC_MEDICAL);
                break;
            case IAS_ACE_FIRE_COMMAND_ID:
                requestPanic(_this, command->eui64, command->sourceEndpoint, PANEL_STATUS_PANIC_FIRE);
                break;
            case IAS_ACE_ARM_COMMAND_ID:
                if (_this->callbacks->onArmRequestReceived)
                {
                    requestArmDisarm(_this, command);
                }
                break;
            case IAS_ACE_GET_PANEL_STATUS_COMMAND_ID:
                if (_this->callbacks->onGetPanelStatusReceived)
                {
                    _this->callbacks->onGetPanelStatusReceived(command->eui64, command->sourceEndpoint, _this->callbackContext);
                    handled = true;
                }
                break;
            /* TODO: support these commands as required */
            case IAS_ACE_GET_BYPASSED_ZONE_LIST_COMMAND_ID:
            case IAS_ACE_GET_ZONE_STATUS_COMMAND_ID:
            case IAS_ACE_BYPASS_COMMAND_ID:
            case IAS_ACE_GET_ZONE_ID_MAP_COMMAND_ID:
            case IAS_ACE_GET_ZONE_INFO_COMMAND_ID:
            default:
                icLogError(LOG_TAG, "Unsupported ACE cluster command %" PRIx8, command->commandId);
                break;
        }
    }

    return handled;
}

static void requestPanic(IASACECluster *cluster, const uint64_t eui64, const uint8_t endpointId, const PanelStatus panicStatus)
{
    if (cluster->callbacks->onPanicRequestReceived)
    {
        cluster->callbacks->onPanicRequestReceived(eui64,
                                                   endpointId,
                                                   panicStatus,
                                                   cluster->callbackContext);
    }
}

static void requestArmDisarm(IASACECluster *cluster, ReceivedClusterCommand *command)
{
    ZCLArmCommandPayload payload;
    enum ArmNotification *zclResult = NULL;
    int rc  = readArmCommandPayload(command, &payload);

    if (rc != 0)
    {
        AUTO_CLEAN(free_generic__auto) const char *errStr = strerrorSafe(rc);
        icLogError(LOG_TAG, "Could not read arm command payload: %s", errStr);
        return;
    }

    PanelStatus *requestedStatus = hashMapGet(zclArmModeToPanelStatus, &payload.armMode, sizeof(int));
    if (requestedStatus)
    {
        const IASACEArmRequest request = {
                .requestedStatus = *requestedStatus,
                .accessCode = payload.accessCode
        };
        ArmDisarmNotification result = cluster->callbacks->onArmRequestReceived(command->eui64,
                                                                                command->sourceEndpoint,
                                                                                &request,
                                                                                cluster->callbackContext);
        zclResult = hashMapGet(armNotificationToZCL, &result, sizeof(int));
        if (zclResult)
        {
            zigbeeSubsystemSendCommand(command->eui64,
                                       command->sourceEndpoint,
                                       IAS_ACE_CLUSTER_ID,
                                       false,
                                       IAS_ACE_ARM_RESPONSE_COMMAND_ID,
                                       (uint8_t *) zclResult,
                                       1);
        }
        else
        {
            icLogWarn(LOG_TAG, "Arm/Disarm request result [0x%02" PRIx8 "] did not map to a ZCL arm notification", (uint8_t) result);
        }
    }
    else
    {
        icLogError(LOG_TAG, "Unable to convert ZCL arm mode 0x%02" PRIx8 " to a deviceService panel status", payload.armMode);
    }

    free(payload.accessCode);
}

static void addDictionaryEntry(icHashMap *map, int from, int to)
{
    hashMapPutCopy(map, &from, sizeof(int), &to, sizeof(int));
}

static int readArmCommandPayload(ReceivedClusterCommand *command, ZCLArmCommandPayload *payload)
{
    memset(payload, 0, sizeof(*payload));
    errno = 0;

    icLogDebug(LOG_TAG, "Arm command len %d", command->commandDataLen);

    sbZigbeeIOContext *ctx = zigbeeIOInit(command->commandData, command->commandDataLen, ZIO_READ);
    payload->armMode = zigbeeIOGetUint8(ctx);
    payload->accessCode = zigbeeIOGetString(ctx);
    payload->zoneId = zigbeeIOGetUint8(ctx);

    if (!errno)
    {
        icLogDebug(LOG_TAG, "Arm command: Mode: 0x%02" PRIx8 ", Zone: 0x%02" PRIx8 "", payload->armMode, payload->zoneId);
    }
    else
    {
        icLogError(LOG_TAG, "Unable to read arm command: %s", strerrorSafe(errno));
        free(payload->accessCode);
    }

    return errno;
}

static uint8_t getAndDecrementRefCounter(void)
{
    uint8_t val = 0;
    pthread_mutex_lock(&refCounterMutex);
    {
        val = refCounter;
        if (refCounter > 0)
        {
            refCounter--;
        }
    }
    pthread_mutex_unlock(&refCounterMutex);

    return val;
}

static uint8_t getAndIncrementRefCounter(void)
{
    uint8_t val = 0;
    pthread_mutex_lock(&refCounterMutex);
    {
        val = refCounter++;
    }
    pthread_mutex_unlock(&refCounterMutex);

    return val;
}

void iasACEClusterSendPanelStatus(uint64_t eui64, uint8_t destEndpoint, const SecurityState *state, bool isResponse)
{
    enum ZCLAlarmStatus *zclAlarmStatus = hashMapGet(alarmStatusToZCL, (int *) &state->panelStatus, sizeof(int));
    enum ZCLPanelStatus *zclPanelStatus = hashMapGet(panelStatusToZCL, (int *) &state->panelStatus, sizeof(int));

    if (zclPanelStatus && *zclPanelStatus == ZCL_PANEL_STATUS_IN_ALARM && !zclAlarmStatus)
    {
        icLogWarn(LOG_TAG, "Ignoring unknown alarm status %s", PanelStatusLabels[state->panelStatus]);
    }
    else if (zclPanelStatus)
    {
        uint8_t audibleNotif = 0x00;
        uint8_t commandId = IAS_ACE_PANEL_STATUS_CHANGED_COMMAND_ID;
        uint8_t zclAlarm = ZCL_ALARM_NONE;

        if (state->indication == SECURITY_INDICATION_BOTH || state->indication == SECURITY_INDICATION_AUDIBLE)
        {
            audibleNotif = AUDIBLE_NOTIF_DEFAULT;
        }

        if (zclAlarmStatus)
        {
            zclAlarm = *zclAlarmStatus;
        }

        uint8_t payload[4];
        sbZigbeeIOContext *zio = zigbeeIOInit(payload, 4, ZIO_WRITE);
        zigbeeIOPutUint8(zio, *zclPanelStatus);
        zigbeeIOPutUint8(zio, state->timeLeft);
        zigbeeIOPutUint8(zio, audibleNotif);
        zigbeeIOPutUint8(zio, zclAlarm);

        if (isResponse)
        {
            commandId = IAS_ACE_GET_PANEL_STATUS_RESPONSE_COMMAND_ID;
        }

        icLogDebug(LOG_TAG,
                   "Sending panel status [0x%02" PRIx8 "] to %" PRIx64 ".%" PRIu8" audible: %d, seconds left: %d",
                   *zclPanelStatus,
                   eui64,
                   destEndpoint,
                   audibleNotif,
                   state->timeLeft);

        zigbeeSubsystemSendCommand(eui64,
                                   destEndpoint,
                                   IAS_ACE_CLUSTER_ID,
                                   false,
                                   commandId,
                                   payload,
                                   ARRAY_LENGTH(payload));
    }
    else
    {
        icLogWarn(LOG_TAG, "Ignoring unknown panel status %s", PanelStatusLabels[state->panelStatus]);
    }
}

void iasACEClusterSendZoneStatus(uint64_t eui64, uint8_t destEndpoint, const ZoneChanged *zoneChanged)
{
    const char *label = stringCoalesceAlt(zoneChanged->label, "");

    /* Payload is uint8 enum16 enum8 zstring */
    uint8_t payload[5 + strlen(label)];
    sbZigbeeIOContext *zio = zigbeeIOInit(payload, sizeof(payload), ZIO_WRITE);

    /*
     * The zone table is not programmed in the ACE device.
     * All notifications shall use the default zone ID (0xff)
     * Ref: ZCLv7 8.3.2.4.4.2
     */
    zigbeeIOPutUint8(zio, 0xffU);

    uint16_t zclZoneStatus = 0;
    if (zoneChanged->faulted == true)
    {
        zclZoneStatus |= IAS_ZONE_STATUS_ALARM1;
    }

    zigbeeIOPutUint16(zio, zclZoneStatus);

    uint8_t audibleNotif = AUDIBLE_NOTIF_MUTE;
    switch(zoneChanged->indication)
    {
        case SECURITY_INDICATION_AUDIBLE:
        case SECURITY_INDICATION_BOTH:
            audibleNotif = AUDIBLE_NOTIF_DEFAULT;
            break;

        case SECURITY_INDICATION_NONE:
        case SECURITY_INDICATION_VISUAL:
            audibleNotif = AUDIBLE_NOTIF_MUTE;
            break;

        case SECURITY_INDICATION_INVALID:
        default:
            icLogWarn(LOG_TAG, "SecurityIndication [%d] not recognized! Audible notification disabled.", zoneChanged->indication);
            break;
    }

    zigbeeIOPutUint8(zio, audibleNotif);
    zigbeeIOPutString(zio, (char *) label);

    zigbeeSubsystemSendCommand(eui64,
                               destEndpoint,
                               IAS_ACE_CLUSTER_ID,
                               false,
                               IAS_ACE_ZONE_STATUS_CHANGED_COMMAND_ID,
                               payload,
                               ARRAY_LENGTH(payload));
}

#endif // BARTON_CONFIG_ZIGBEE
