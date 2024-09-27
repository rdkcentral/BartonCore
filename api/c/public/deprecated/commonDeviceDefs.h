//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2015 iControl Networks, Inc.
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
// iControl Networks retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------
/*
 * This contains definitions of elements on every type of device as well as some
 * of our common 'first class citizen' elements.
 *
 * Created by Thomas Lea on 9/30/15.
 */

#ifndef FLEXCORE_COMMONDEVICEDEFS_H
#define FLEXCORE_COMMONDEVICEDEFS_H

//All devices have these (some may be optional)
#define COMMON_DEVICE_RESOURCE_MANUFACTURER                 "manufacturer"
#define COMMON_DEVICE_RESOURCE_MODEL                        "model"
#define COMMON_DEVICE_RESOURCE_HARDWARE_VERSION             "hardwareVersion"
#define COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION             "firmwareVersion"
#define COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION_STRING      "firmwareVersionString"
#define COMMON_DEVICE_RESOURCE_DATE_ADDED                   "dateAdded"
#define COMMON_DEVICE_RESOURCE_DATE_LAST_CONTACTED          "dateLastContacted"
#define COMMON_DEVICE_RESOURCE_COMM_FAIL                    "communicationFailure"
#define COMMON_DEVICE_RESOURCE_SERIAL_NUMBER                "serialNumber"
#define COMMON_DEVICE_RESOURCE_MAC_ADDRESS                  "macAddress"
#define COMMON_DEVICE_RESOURCE_IP_ADDRESS                   "ipAddress"
#define COMMON_DEVICE_FUNCTION_RESET_TO_FACTORY             "resetToFactoryDefaults"
#define COMMON_DEVICE_METADATA_INTEGRATED_PERIPHERAL        "integratedPeripheral" // exists & true if device is on the CPE
#define COMMON_DEVICE_RESOURCE_TIMEZONE                     "timezone" //POSIX 1003.1 section 8.3 compliant value (ex: CST6CDT,M3.2.0,M11.1.0)
#define COMMON_DEVICE_RESOURCE_NERSSI                       "neRssi"
#define COMMON_DEVICE_RESOURCE_NELQI                        "neLqi"
#define COMMON_DEVICE_RESOURCE_FERSSI                       "feRssi"
#define COMMON_DEVICE_RESOURCE_FELQI                        "feLqi"
#define COMMON_DEVICE_RESOURCE_LINK_QUALITY                 "linkQuality"
#define COMMON_DEVICE_RESOURCE_LINK_SCORE                   "linkScore"
#define COMMON_DEVICE_RESOURCE_TEMPERATURE                  "temp"
#define COMMON_DEVICE_RESOURCE_HIGH_TEMPERATURE             "highTemperature"
#define COMMON_DEVICE_RESOURCE_BATTERY_VOLTAGE              "battVolts"
#define COMMON_DEVICE_METADATA_BATTERY_USED_MAH             "battUsedMilliAmpHr"
#define COMMON_DEVICE_METADATA_RETRIES                      "retries"
#define COMMON_DEVICE_METADATA_REJOINS                      "rejoins"
#define COMMON_DEVICE_METADATA_DISPLAY_INDEX                "displayIndex"
#define COMMON_DEVICE_RESOURCE_RELATIVE_HUMIDITY            "relHumid"
#define COMMON_DEVICE_RESOURCE_BATTERY_LOW                  "lowBatt"
#define COMMON_DEVICE_RESOURCE_LAST_USER_INTERACTION_DATE   "lastUserInteractionDate"
#define COMMON_DEVICE_RESOURCE_AC_MAINS_DISCONNECTED        "acMainsDisconnected"
#define COMMON_DEVICE_RESOURCE_BATTERY_BAD                  "badBatt"
#define COMMON_DEVICE_RESOURCE_BATTERY_MISSING              "missingBatt"
#define COMMON_DEVICE_RESOURCE_BATTERY_PERCENTAGE_REMAINING "battPercentRemaining"
#define COMMON_DEVICE_RESOURCE_BATTERY_HIGH_TEMPERATURE     "battHighTemperature"
#define COMMON_DEVICE_RESOURCE_BATTERY_THRESHOLDS           "battThresholds"
#define COMMON_DEVICE_RESOURCE_FIRMWARE_UPDATE_STATUS       "firmwareUpdateStatus"
#define COMMON_DEVICE_RESOURCE_REBOOT                       "reboot"
#define FIRMWARE_UPDATE_STATUS_UP_TO_DATE                   "upToDate"
#define FIRMWARE_UPDATE_STATUS_PENDING                      "pending"
#define FIRMWARE_UPDATE_STATUS_STARTED                      "started"
#define FIRMWARE_UPDATE_STATUS_COMPLETED                    "completed"
#define FIRMWARE_UPDATE_STATUS_FAILED                       "failed"
#define LINK_QUALITY_UNKNOWN                                "unknown"
#define LINK_QUALITY_POOR                                   "poor"
#define LINK_QUALITY_FAIR                                   "fair"
#define LINK_QUALITY_GOOD                                   "good"
#define LINK_QUALITY_EXCELLENT                              "excellent"
#define COMMON_DEVICE_METADATA_RECONFIGURATION_REQUIRED     "reconfRequired"
#define COMMON_DEVICE_METADATA_COMMFAIL_OVERRIDE_SECS       "commFailOverrideSeconds"
#define COMMON_DEVICE_RESOURCE_NETWORK_TYPE                 "networkType"

#define COMMON_ENDPOINT_RESOURCE_LABEL                      "label"
#define COMMON_ENDPOINT_RESOURCE_TAMPERED                   "tampered"
#define COMMON_ENDPOINT_RESOURCE_TROUBLE                    "trouble"

//Camera Stuff
#define CAMERA_DC                                           "camera"
#define DOORBELL_CAMERA_DC                                  "doorbellCamera"
#define CAMERA_DC_CAMERA_PROFILE_ENDPOINT_ID                "camera"
#define CAMERA_DC_SENSOR_PROFILE_ENDPOINT_ID                "sensor"
#define CAMERA_DC_BUTTON_PROFILE_ENDPOINT_ID                "button"
#define CAMERA_DC_SPEAKER_PROFILE_ENDPOINT_ID               "speaker"
#define CAMERA_PROFILE                                      "camera"
#define CAMERA_PROFILE_RESOURCE_PORT_NUMBER                 "portNumber"
#define CAMERA_PROFILE_RESOURCE_SIGNAL_STRENGTH             "signalStrength"
#define CAMERA_PROFILE_RESOURCE_ADMIN_USER_ID               "adminUserId"       // endpoint
#define CAMERA_PROFILE_RESOURCE_ADMIN_PASSWORD              "adminPassword"     // endpoint
#define CAMERA_PROFILE_RESOURCE_USER_USER_ID                "userUserId"        // endpoint
#define CAMERA_PROFILE_RESOURCE_USER_PASSWORD               "userPassword"      // endpoint
#define CAMERA_PROFILE_RESOURCE_PIC_URL                     "pictureURL"        // endpoint
#define CAMERA_PROFILE_RESOURCE_VIDEO_INFORMATION           "videoInfo"         // endpoint
#define CAMERA_PROFILE_RESOURCE_RECORDABLE                  "recordable"        // endpoint
#define CAMERA_PROFILE_RESOURCE_MOTION_CAPABLE              "motionCapable"     // endpoint
#define CAMERA_PROFILE_RESOURCE_API_VERSION                 "apiVersion"        // endpoint
#define CAMERA_PROFILE_RESOURCE_RESOLUTION                  "resolution"        // endpoint
#define CAMERA_PROFILE_RESOURCE_ASPECT_RATIO                "aspectRatio"       // endpoint
#define CAMERA_PROFILE_FUNCTION_REBOOT                      "reboot"
#define CAMERA_PROFILE_FUNCTION_PING                        "ping"
#define CAMERA_PROFILE_FUNCTION_WIFI_CREDENTIALS            "setWifiCredentials"
#define CAMERA_PROFILE_FUNCTION_CREATE_MEDIA_TUNNEL         "createMediaTunnel"
#define CAMERA_PROFILE_FUNCTION_DESTROY_MEDIA_TUNNEL        "destroyMediaTunnel"
#define CAMERA_PROFILE_FUNCTION_GET_MEDIA_TUNNEL_STATUS     "getMediaTunnelStatus"
#define CAMERA_PROFILE_FUNCTION_GET_PICTURE                 "getPicture"
#define CAMERA_PROFILE_FUNCTION_UPLOAD_VIDEO_CLIP           "uploadVideoClip"

//PIM Stuff
#define PIM_DC                                              "pim"
#define PIM_PROFILE                                         "pim"
#define PIM_PROFILE_RESOURCE_SECURITY_STATE                 "securityState"
#define PIM_PROFILE_RESOURCE_ZONE_CHANGED                   "zoneChanged"
#define PIM_PROFILE_RESOURCE_INITIATE_BRAINSUCK             "initiateBrainsuck"
#define PIM_PROFILE_RESOURCE_INITIATE_UPGRADE               "initiateUpgrade"
#define PIM_PROFILE_RESOURCE_BRAINSUCK_STATUS               "brainsuckStatus"
#define PIM_PROFILE_RESOURCE_FIND_INSTALLER_CODE            "findInstallerCode"
#define PIM_PROFILE_BRAINSUCK_STATUS_NOT_STARTED            "notStarted"
#define PIM_PROFILE_BRAINSUCK_STATUS_STARTED                "started"
#define PIM_PROFILE_BRAINSUCK_STATUS_COMPLETED              "completed"
#define PIM_PROFILE_BRAINSUCK_STATUS_FAILED                 "failed"
#define PIM_PROFILE_ZONE_TYPE_METADATA                      "pimZoneType"
#define PIM_PROFILE_ZONE_TYPE_METADATA_DOOR                 "door"
#define PIM_PROFILE_ZONE_TYPE_METADATA_WINDOW               "window"
#define PIM_PROFILE_ZONE_TYPE_METADATA_MOTION               "motion"
#define PIM_PROFILE_ZONE_TYPE_METADATA_PANIC                "panic"
#define PIM_PROFILE_ZONE_TYPE_METADATA_WATER                "water"
#define PIM_PROFILE_ZONE_TYPE_METADATA_ENVIRONMENTAL        "environmental"
#define PIM_PROFILE_ZONE_TYPE_METADATA_SMOKE                "smoke"
#define PIM_PROFILE_ZONE_TYPE_METADATA_CO                   "co"
#define PIM_PROFILE_ZONE_FUNC_METADATA                      "pimZoneFunc"
#define PIM_PROFILE_ZONE_FUNC_METADATA_ENTRY_EXIT           "entryExit"
#define PIM_PROFILE_ZONE_FUNC_METADATA_PERIMETER            "perimeter"
#define PIM_PROFILE_ZONE_FUNC_METADATA_TRBL_DAY_ALRM_NGHT   "troubleDayAlarmNight"
#define PIM_PROFILE_ZONE_FUNC_METADATA_SILENT_24HR          "silent24Hour"
#define PIM_PROFILE_ZONE_FUNC_METADATA_AUDIBLE_24HR         "audible24Hour"
#define PIM_PROFILE_ZONE_FUNC_METADATA_FIRE_24HR            "fire24Hour"
#define PIM_PROFILE_ZONE_FUNC_METADATA_MONITOR_24HR         "monitor24Hour"
#define PIM_PROFILE_ZONE_FUNC_METADATA_NO_ALARM_RESPONSE    "noAlarmResponse"
#define PIM_PROFILE_ZONE_FUNC_METADATA_SILENT_BURG          "silentBurglary"
#define PIM_PROFILE_ZONE_FUNC_METADATA_INT_FOLLOWER         "interiorFollower"
#define PIM_PROFILE_ZONE_FUNC_METADATA_INT_WITH_DELAY       "interiorWithDelay"
#define PIM_PROFILE_ZONE_FUNC_METADATA_INT_ARM_NIGHT        "interiorArmNight"
#define PIM_PROFILE_ZONE_FUNC_METADATA_INT_ARM_NIGHT_DELAY  "interiorArmNightDelay"
#define PIM_PROFILE_ZONE_FUNC_METADATA_ARM_STAY             "armStay"
#define PIM_PROFILE_ZONE_FUNC_METADATA_ARM_AWAY             "armAway"
#define PIM_PROFILE_ZONE_FUNC_METADATA_DISARM               "disarm"
#define PIM_CHIMES_ENABLED_METADATA                         "pimChimesEnabled"
#define PIM_PROFILE_RESOURCE_GENERAL_TROUBLE                "generalTrouble"

//PRM Stuff
#define PRM_DC                                              "prm"
#define BRIDGE_PROFILE                                      "bridge"
#define BRIDGE_PROFILE_RESOURCE_REFRESH                     "refresh"
#define BRIDGE_PROFILE_RESOURCE_REFRESH_STATUS              "refreshStatus"
#define BRIDGE_RESOURCE_REFRESH_STATUS_REQUESTED            "Requested"
#define BRIDGE_RESOURCE_REFRESH_STATUS_COMPLETED            "Completed"
#define BRIDGE_RESOURCE_REFRESH_STATUS_FAILED               "Failed"
#define BRIDGE_PROFILE_RESOURCE_TAMPERED                    COMMON_ENDPOINT_RESOURCE_TAMPERED
#define BRIDGE_PROFILE_RESOURCE_CONFIGURATION_MODE          "configurationMode"
#define BRIDGE_PROFILE_CONFIGURATION_MODE_START             "start"
#define BRIDGE_PROFILE_CONFIGURATION_MODE_STOP              "stop"
#define BRIDGE_PROFILE_RESOURCE_RESET                       "reset"
#define BRIDGE_PROFILE_RESOURCE_RESET_SMOKE_SENSORS         "resetSmokeSensors"

//Sensor Stuff
#define SENSOR_DC                                           "sensor"
#define SENSOR_PROFILE                                      "sensor"
#define SENSOR_PROFILE_RESOURCE_FAULTED                     "faulted"
#define SENSOR_PROFILE_RESOURCE_TAMPERED                    COMMON_ENDPOINT_RESOURCE_TAMPERED
#define SENSOR_PROFILE_RESOURCE_BYPASSED                    "bypassed"
#define SENSOR_PROFILE_RESOURCE_MOTION_SENSITIVITY          "motionSensitivity"
#define SENSOR_PROFILE_RESOURCE_TYPE                        "type"
#define SENSOR_PROFILE_RESOURCE_DIRTY                       "dirty"
#define SENSOR_PROFILE_RESOURCE_END_OF_LIFE                 "endOfLife"
#define SENSOR_PROFILE_RESOURCE_END_OF_LINE_FAULT           "endOfLineFault"
#define SENSOR_PROFILE_MOTION_TYPE                          "motion"
#define SENSOR_PROFILE_RESOURCE_QUALIFIED                   "qualified"
#define SENSOR_PROFILE_CONTACT_SWITCH_TYPE                  "contactSwitch"
#define SENSOR_PROFILE_SMOKE                                "smoke"
#define SENSOR_PROFILE_CO                                   "co"
#define SENSOR_PROFILE_MOTION                               "motion"
#define SENSOR_PROFILE_GLASS_BREAK                          "glassBreak"
#define SENSOR_PROFILE_WATER                                "water"
#define SENSOR_PROFILE_VIBRATION                            "vibration"
#define SENSOR_PROFILE_SIREN                                "siren"
#define SENSOR_PROFILE_PERSONAL_EMERGENCY                   "personalEmergency"
#define SENSOR_PROFILE_REMOTE_CONTROL                       "remoteControl"
#define SENSOR_PROFILE_UNKNOWN_TYPE                         "unknown"
#define SENSOR_PROFILE_ENDPOINT_ID_LIST                     "endpointIdList"
#define SENSOR_PROFILE_RESOURCE_MAGNETIC_FIELD_STRENGTH     "magneticFieldStrength"
#define SENSOR_PROFILE_RESOURCE_MAGNETIC_FIELD_HEALTH       "magneticFieldHealth"
// This metadata is useful to send test bit status with resource update to distinguish test fault.
#define SENSOR_PROFILE_METADATA_TEST                        "test"

//Light stuff
#define LIGHT_DC                                            "light"
#define LIGHT_PROFILE                                       "light"
#define LIGHT_SWITCH_PROFILE                                "lightSwitch"
#define LIGHT_PROFILE_RESOURCE_IS_ON                        "isOn"
#define LIGHT_PROFILE_RESOURCE_CURRENT_LEVEL                "currentLevel"
#define LIGHT_PROFILE_RESOURCE_COLOR                        "colorXY"
#define LIGHT_PROFILE_RESOURCE_CURRENT_POWER                "currentPower"
#define LIGHT_PROFILE_RESOURCE_IS_DIMMABLE_MODE             "isDimmableMode"

//Thermostat stuff
#define THERMOSTAT_DC                                       "thermostat"
#define THERMOSTAT_PROFILE                                  "thermostat"
#define THERMOSTAT_PROFILE_RESOURCE_SYSTEM_STATE            "systemState"
#define THERMOSTAT_PROFILE_RESOURCE_SYSTEM_STATE_OFF        "off"
#define THERMOSTAT_PROFILE_RESOURCE_SYSTEM_STATE_HEATING    "heating"
#define THERMOSTAT_PROFILE_RESOURCE_SYSTEM_STATE_COOLING    "cooling"
#define THERMOSTAT_PROFILE_RESOURCE_SYSTEM_MODE             "systemMode"
#define THERMOSTAT_PROFILE_RESOURCE_SYSTEM_MODE_OFF         "off"
#define THERMOSTAT_PROFILE_RESOURCE_SYSTEM_MODE_HEAT        "heat"
#define THERMOSTAT_PROFILE_RESOURCE_SYSTEM_MODE_COOL        "cool"
#define THERMOSTAT_PROFILE_RESOURCE_SYSTEM_MODE_EMERG_HEAT  "emergencyHeat"
#define THERMOSTAT_PROFILE_RESOURCE_SYSTEM_MODE_PRECOOLING  "precooling"
#define THERMOSTAT_PROFILE_RESOURCE_SYSTEM_MODE_AUTO        "auto"
#define THERMOSTAT_PROFILE_RESOURCE_SYSTEM_MODE_FAN_ONLY    "fanOnly"
#define THERMOSTAT_PROFILE_RESOURCE_HOLD_ON                 "holdOn"
#define THERMOSTAT_PROFILE_RESOURCE_FAN_MODE                "fanMode"
#define THERMOSTAT_PROFILE_RESOURCE_FAN_MODE_OFF            "off"
#define THERMOSTAT_PROFILE_RESOURCE_FAN_MODE_ON             "on"
#define THERMOSTAT_PROFILE_RESOURCE_FAN_MODE_AUTO           "auto"
#define THERMOSTAT_PROFILE_RESOURCE_FAN_MODE_UNKNOWN        "unknown"
#define THERMOSTAT_PROFILE_RESOURCE_FAN_ON                  "fanOn"
#define THERMOSTAT_PROFILE_RESOURCE_LOCAL_TEMP              "localTemperature"
#define THERMOSTAT_PROFILE_RESOURCE_ABS_MIN_HEAT            "absoluteMinHeatLimit"
#define THERMOSTAT_PROFILE_RESOURCE_ABS_MAX_HEAT            "absoluteMaxHeatLimit"
#define THERMOSTAT_PROFILE_RESOURCE_ABS_MIN_COOL            "absoluteMinCoolLimit"
#define THERMOSTAT_PROFILE_RESOURCE_ABS_MAX_COOL            "absoluteMaxCoolLimit"
#define THERMOSTAT_PROFILE_RESOURCE_LOCAL_TEMP_CALIBRATION  "localTemperatureCalibration"
#define THERMOSTAT_PROFILE_RESOURCE_HEAT_SETPOINT           "heatSetpoint"
#define THERMOSTAT_PROFILE_RESOURCE_COOL_SETPOINT           "coolSetpoint"
#define THERMOSTAT_PROFILE_RESOURCE_CONTROL_SEQ             "controlSequenceOfOperation"

//Doorlock stuff
#define DOORLOCK_DC                                         "doorLock"
#define DOORLOCK_PROFILE                                    "doorLock"
#define DOORLOCK_PROFILE_RESOURCE_LOCKED                    "locked"
#define DOORLOCK_PROFILE_RESOURCE_AUTOLOCK_SECS             "autoLockSecs"
#define DOORLOCK_PROFILE_RESOURCE_PIN_CODES                 "pinCodes"
#define DOORLOCK_PROFILE_RESOURCE_LAST_PROGRAMMING_EVENT    "lastProgrammingEvent"
#define DOORLOCK_PROFILE_RESOURCE_MAX_PIN_CODES             "maxPinCodes"
#define DOORLOCK_PROFILE_RESOURCE_MAX_PIN_CODE_LENGTH       "maxPinCodeLength"
#define DOORLOCK_PROFILE_RESOURCE_MIN_PIN_CODE_LENGTH       "minPinCodeLength"
#define DOORLOCK_PROFILE_RESOURCE_JAMMED                    "jammed"
#define DOORLOCK_PROFILE_RESOURCE_TAMPERED                  COMMON_ENDPOINT_RESOURCE_TAMPERED
#define DOORLOCK_PROFILE_RESOURCE_INVALID_CODE_ENTRY_LIMIT  "invalidCodeEntryLimit"
#define DOORLOCK_PROFILE_LOCKED_SOURCE                      "source"
#define DOORLOCK_PROFILE_LOCKED_USERID                      "userId"
#define DOORLOCK_PROFILE_LOCKED_SOURCE_KEYPAD               "keypad"
#define DOORLOCK_PROFILE_LOCKED_SOURCE_RF                   "rf"
#define DOORLOCK_PROFILE_LOCKED_SOURCE_MANUAL               "manual"
#define DOORLOCK_PROFILE_LOCKED_SOURCE_RFID                 "rfid"
#define DOORLOCK_PROFILE_LOCKED_SOURCE_AUTO                 "auto"
#define DOORLOCK_PROFILE_LOCKED_SOURCE_SCHEDULE             "schedule"
#define DOORLOCK_PROFILE_LOCKED_SOURCE_UNKNOWN              "unknown"
#define DOORLOCK_PROFILE_USER_PIN                           "pin"
#define DOORLOCK_PROFILE_USER_ID                            "id"

//M1LTE module stuff
#define M1LTE_DC                                                        "m1lte"
#define REMOTE_CELL_MODEM_PROFILE                                       "remoteCellModem"
#define REMOTE_CELL_MODEM_PROFILE_RESOURCE_IS_ON                        "isOn"
#define REMOTE_CELL_MODEM_PROFILE_RESOURCE_EMERGENCY_RESET              "emergencyReset"
#define REMOTE_CELL_MODEM_PROFILE_RESOURCE_RESYNC_ZIGBEE_UART           "resyncUart"
#define REMOTE_CELL_MODEM_PROFILE_RESOURCE_ZIGBEE_CONNECTION_HEALTHY    "zigbeeConnectionHealthy"
#define REMOTE_CELL_MODEM_PROFILE_RESOURCE_LAST_DIAG_CHECK              "lastDiagCheck"
#define REMOTE_CELL_MODEM_PROFILE_RESOURCE_REBOOT_REASON                "rebootReason"
#define REMOTE_CELL_MODEM_PROFILE_RESOURCE_REBOOT_TIME                  "rebootTime"
#define REMOTE_CELL_MODEM_PROFILE_RESOURCE_RECHARGE_CYCLES              "batteryRechargeCycles"
#define REMOTE_CELL_MODEM_ENDPOINT_NAME                                 "modem"

//Garage Door Controller stuff
#define GARAGE_DOOR_CONTROLLER_DC                               "garageDoorController"
#define GARAGE_DOOR_CONTROLLER_DC_DOOR_PROFILE_ENDPOINT_ID      "doorController"
#define GARAGE_DOOR_CONTROLLER_DC_LIGHT_PROFILE_ENDPOINT_ID     "light"
#define GARAGE_DOOR_CONTROLLER_DC_BEAM_PROFILE_ENDPOINT_ID      "beam"
#define GARAGE_DOOR_CONTROLLER_PROFILE                          "garageDoorController"
#define GARAGE_DOOR_CONTROLLER_PROFILE_RESOURCE_IS_OCCUPIED     "isOccupied"
#define GARAGE_DOOR_CONTROLLER_PROFILE_RESOURCE_IS_OPEN         "isOpen"
#define GARAGE_DOOR_CONTROLLER_PROFILE_RESOURCE_OPEN_PERCENTAGE "openPercentage"
#define GARAGE_DOOR_CONTROLLER_PROFILE_RESOURCE_IS_BEAM_BROKEN  "isSafetyBeamBroken"

//Button stuff
#define BUTTON_PROFILE                                      "button"
#define BUTTON_PROFILE_RESOURCE_PRESSED                     "pressed"
#define BUTTON_PROFILE_METADATA_EVENT_TIME                  "eventTime"

//Speaker stuff
#define SPEAKER_PROFILE                                     "speaker"
#define SPEAKER_PROFILE_FUNCTION_CREATE_MEDIA_TUNNEL        "createMediaTunnel"
#define SPEAKER_PROFILE_FUNCTION_DESTROY_MEDIA_TUNNEL       "destroyMediaTunnel"

//Window Covering stuff
#define WINDOW_COVERING_DC                                  "windowCovering"
#define WINDOW_COVERING_PROFILE                             "windowCovering"
#define WINDOW_COVERING_PROFILE_RESOURCE_LIFT_PERCENTAGE    "liftPercentage"
#define WINDOW_COVERING_FUNCTION_UP                         "moveUp"
#define WINDOW_COVERING_FUNCTION_DOWN                       "moveDown"
#define WINDOW_COVERING_FUNCTION_STOP                       "stop"

//Keypad/Keyfob
#define KEYPAD_DC                                           "keypad"
#define SECURITY_CONTROLLER_PROFILE                         "securityController"
#define SECURITY_CONTROLLER_PROFILE_VERSION                 2
#define SECURITY_CONTROLLER_PROFILE_KEYPAD_TYPE             "keypad"
#define SECURITY_CONTROLLER_PROFILE_KEYFOB_TYPE             "keyfob"
#define SECURITY_CONTROLLER_PROFILE_RESOURCE_TAMPERED       COMMON_ENDPOINT_RESOURCE_TAMPERED
#define SECURITY_CONTROLLER_PROFILE_RESOURCE_TYPE           "type"
#define SECURITY_CONTROLLER_PROFILE_RESOURCE_SECURITY_STATE "securityState"
#define SECURITY_CONTROLLER_PROFILE_RESOURCE_ZONE_CHANGED   "zoneChanged"

// TODO: remove (used by legacy)
#define SENSOR_PROFILE_KEYFOB                               "keyfob"
#define SENSOR_PROFILE_KEYPAD                               "keypad"

//Keyfob is an instance of keypad
#define KEYFOB_DC                                           "keyfob"

//Warning device stuff
#define WARNING_DEVICE_DC                                   "warningDevice"
#define WARNING_DEVICE_PROFILE                              "warningDevice"
#define WARNING_DEVICE_PROFILE_VERSION                      1
#define WARNING_DEVICE_RESOURCE_TONE                        "tone"
#define WARNING_DEVICE_RESOURCE_SECURITY_STATE              "securityState"
#define WARNING_DEVICE_RESOURCE_SILENCED                    "silenced"
#define WARNING_DEVICE_TONE_NONE                            "none"
#define WARNING_DEVICE_TONE_WARBLE                          "warble"
#define WARNING_DEVICE_TONE_FIRE                            "fire"
#define WARNING_DEVICE_TONE_CO                              "co"
#define WARNING_DEVICE_TONE_HIGH                            "high"
#define WARNING_DEVICE_TONE_LOW                             "low"

//Light controller stuff
#define LIGHTCONTROLLER_DC                                  "lightController"
#define LIGHTCONTROLLER_PROFILE                             "lightController"
#define LIGHTCONTROLLER_PROFILE_RESOURCE_BOUND_ENDPOINT_URI "boundEndpointUri"

// Integrated piezo has its own device class
#define INTEGRATED_PIEZO_DC                                 "integratedPiezo"

// Presence device has its own device class
#define PRESENCE_DC                                         "presence"
#define PRESENCE_PROFILE                                    PRESENCE_DC

//Low Power Mode Policy stuff
#define LPM_POLICY_METADATA                                 "lpmPolicy"

//Chime stuff
#define CHIME_DC                                            "chime"
#define CHIME_PROFILE                                       "chime"
#define CHIME_PROFILE_RESOURCE_PLAY_URL                     "playUrl"
#define CHIME_PROFILE_RESOURCE_SNOOZE_UNTIL                 "snoozeUntil"
#define CHIME_PROFILE_RESOURCE_VOLUME                       "volume"
#define CHIME_PROFILE_RESOURCE_AUDIO_ASSET_LIST             "audioAssetList"

//Doorbell Button stuff
#define DOORBELL_BUTTON_DC                                  "doorbellButton"
#define DOORBELL_BUTTON_RESOURCE_PRESSED                    "pressed"

// Wifi Motion stuff
#define WIFI_MOTION_DEVICE_METADATA_IS_WIFI_MOTION      "isWifiMotion"
#define WIFI_MOTION_EVENT_METADATA_MOTION_STARTED_MAC   "motionStartedMacAddr"
#define WIFI_MOTION_EVENT_METADATA_MOTION_STARTED_LABEL "motionStartedLabel"
#define WIFI_MOTION_EVENT_METADATA_MOTION_STARTED_TIME  "motionStartedTime"
#define WIFI_MOTION_EVENT_METADATA_MOTION_STOPPED_TIME  "motionStoppedTime"

// Cloud device stuff
#define CLOUD_MANAGED_OBJ_ID_METADATA                   "cloudObjectId"
#define CLOUD_MANAGED_ALERT_PREFS_RESOURCE              "alertPreferences"

typedef enum
{
    LPM_POLICY_NONE = 0,
    LPM_POLICY_ARMED_NIGHT,
    LPM_POLICY_ARMED_AWAY,
    LPM_POLICY_ALWAYS
} lpmPolicyPriority;

static const char *lpmPolicyPriorityLabels[] = {
        "none",
        "armedNight",
        "armedAway",
        "always"
};

//XBB stuff
#define XBB_DC "xbb"
#define XBB_RESOURCE_STATUS                                 "status"
#define XBB_RESOURCE_CONFIG                                 "config"
#define XBB_RESOURCE_ALARMS                                 "alarms"
#define XBB_RESOURCE_SIREN_MAX_DURATION                     "sirenMaxDuration"
#define XBB_RESOURCE_SIREN_START                            "sirenStart"
#define XBB_RESOURCE_SIREN_STOP                             "sirenStop"
#define XBB_RESOURCE_SIREN_MUTE                             "sirenMute"
#define XBB_RESOURCE_LID_OPEN                               "lidOpen"
#define XBB_RESOURCE_AC_POWER_LOST                          "acPowerLost"
#define XBB_RESOURCE_LOW_VOLTAGE                            "lowVoltage"
#define XBB_RESOURCE_TEMPERATURE_STATUS                     "tempStatus"
#define XBB_RESOURCE_BANK_STATUS                            "bankStatus"
#define XBB_RESOURCE_BANK_STATUS_1                          "bank1"
#define XBB_RESOURCE_BANK_STATUS_2                          "bank2"
#define XBB_RESOURCE_BANK_STATUS_3                          "bank3"
#define XBB_BATTERY_BANK_STATUS_LOW                         "lowVoltage"
#define XBB_BATTERY_BANK_STATUS_PRESENT                     "present"
#define XBB_TEMPERATURE_STATUS_LOW                          "low"
#define XBB_TEMPERATURE_STATUS_HIGH                         "high"
#define XBB_TEMPERATURE_STATUS_NORMAL                       "normal"

// Metadata for migrated devices
#define MIGRATED_DEVICE_METADATA                            "migratedDevice"
#define MIGRATED_USER_PROPERTIES_METADATA                   "migratedUserProperties"
#define MIGRATION_ALIAS_METADATA                            "migrationAlias"
#define CAMERA_MIGRATED_ID_METADATA                         "cameraMigratedId"

//NetworkType values
#define NETWORK_TYPE_ZIGBEE                                 "zigbee"
#define NETWORK_TYPE_THREAD                                 "thread"
#define NETWORK_TYPE_WIFI                                   "wifi"
#define NETWORK_TYPE_ETHERNET                               "ethernet"
#define NETWORK_TYPE_CELLULAR                               "cellular"

#endif //FLEXCORE_COMMONDEVICEDEFS_H
