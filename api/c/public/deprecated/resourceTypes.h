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
 * Created by Thomas Lea on 3/24/16.
 */

#ifndef FLEXCORE_RESOUCETYPES_H
#define FLEXCORE_RESOUCETYPES_H

#define RESOURCE_TYPE_INTEGER               "com.icontrol.integer"
#define RESOURCE_TYPE_BOOLEAN               "com.icontrol.boolean"
#define RESOURCE_TYPE_STRING                "com.icontrol.string"
#define RESOURCE_TYPE_PERCENTAGE            "com.icontrol.percentage"
#define RESOURCE_TYPE_SECONDS               "com.icontrol.seconds"
#define RESOURCE_TYPE_MINUTES               "com.icontrol.minutes"
#define RESOURCE_TYPE_URL                   "com.icontrol.url"
#define RESOURCE_TYPE_MILLIWATTS            "com.icontrol.milliWatts"
#define RESOURCE_TYPE_LIGHT_LEVEL           "com.icontrol.lightLevel"
#define RESOURCE_TYPE_CIE_1931_COLOR        "com.icontrol.cie1931Color"
#define RESOURCE_TYPE_WATTS                 "com.icontrol.watts"
#define RESOURCE_TYPE_ENDPOINT_URI          "com.icontrol.endpointUri"
#define RESOURCE_TYPE_DOORLOCK_PIN_CODES    "com.icontrol.doorLock.pinCodes"
#define RESOURCE_TYPE_DOORLOCK_PROGRAMMING_EVENT "com.icontrol.doorLock.programmingEvent"

#define RESOURCE_TYPE_SERIAL_NUMBER              "com.icontrol.serialNumber"
#define RESOURCE_TYPE_MAC_ADDRESS                "com.icontrol.macAddress"
#define RESOURCE_TYPE_IP_ADDRESS                 "com.icontrol.ipAddress"
#define RESOURCE_TYPE_IP_PORT                    "com.icontrol.ipPort"
#define RESOURCE_TYPE_REBOOT_OPERATION           "com.icontrol.rebootOperation"
#define RESOURCE_TYPE_PING_OPERATION             "com.icontrol.pingOperation"
#define RESOURCE_TYPE_LABEL                      "com.icontrol.label"
#define RESOURCE_TYPE_USER_ID                    "com.icontrol.userId"
#define RESOURCE_TYPE_PASSWORD                   "com.icontrol.password"
#define RESOURCE_TYPE_VERSION                    "com.icontrol.version"
#define RESOURCE_TYPE_VIDEO_RESOLUTION           "com.icontrol.videoResolution"
#define RESOURCE_TYPE_VIDEO_ASPECT_RATIO         "com.icontrol.aspectRatio"
#define RESOURCE_TYPE_TIMEZONE                   "com.icontrol.timeZone"
#define RESOURCE_TYPE_WIFI_CREDENTIALS_OPERATION "com.icontrol.setWifiCredentials"
#define RESOURCE_TYPE_AUDIO_ASSETS               "com.icontrol.audioAssets"
#define RESOURCE_TYPE_NETWORK_TYPE               "com.icontrol.networkType"

/**
 * Unix timestamp in milliseconds. Text representations using a 14 char buffer will store up to
 * Sat Nov 20 17:46:39 UTC 2286
 */
#define RESOURCE_TYPE_DATETIME              "com.icontrol.dateTime"
#define RESOURCE_TYPE_TROUBLE               "com.icontrol.trouble"
#define RESOURCE_TYPE_FIRMWARE_VERSION_STATUS "com.icontrol.firmwareVersionStatus"
#define RESOURCE_TYPE_RSSI                  "rssi"
#define RESOURCE_TYPE_LQI                   "lqi"
#define RESOURCE_TYPE_SIGNAL_STRENGTH       "signalStrength"
#define RESOURCE_TYPE_LINK_QUALITY          "com.icontrol.linkQuality"
#define RESOURCE_TYPE_BATTERY_VOLTAGE       "battery.voltage"
#define RESOURCE_TYPE_BATTERY_THRESHOLDS    "com.icontrol.batteryThresholds"
#define RESOURCE_TYPE_RELATIVE_HUMIDITY     "humidity.relative"

#define RESOURCE_TYPE_RESET_TO_FACTORY_OPERATION "com.icontrol.resetToFactoryOperation"

#define RESOURCE_TYPE_CREATE_MEDIA_TUNNEL_OPERATION     "com.icontrol.createMediaTunnelOperation"
#define RESOURCE_TYPE_DESTROY_MEDIA_TUNNEL_OPERATION    "com.icontrol.destroyMediaTunnelOperation"
#define RESOURCE_TYPE_GET_MEDIA_TUNNEL_STATUS_OPERATION "com.icontrol.getMediaTunnelStatusOperation"
#define RESOURCE_TYPE_GET_PICTURE_OPERATION             "com.icontrol.getPictureOperation"
#define RESOURCE_TYPE_UPLOAD_VIDEO_CLIP_OPERATION       "com.icontrol.uploadVideoClipOperation"

#define RESOURCE_TYPE_MOTION_SENSITIVITY    "com.icontrol.motionSensitivity"
#define RESOURCE_TYPE_SENSOR_TYPE           "com.icontrol.sensorType"
#define RESOURCE_TYPE_SENSOR_TROUBLE        "com.icontrol.sensorTrouble"
#define RESOURCE_TYPE_SECURITY_CONTROLLER_TYPE "com.icontrol.securityControllerType"

#define RESOURCE_TYPE_TSTAT_SYSTEM_MODE     "com.icontrol.tstatSystemMode"
#define RESOURCE_TYPE_TSTAT_SYSTEM_STATE    "com.icontrol.tstatSystemState"
#define RESOURCE_TYPE_TSTAT_FAN_MODE        "com.icontrol.tstatFanMode"
#define RESOURCE_TYPE_TEMPERATURE           "com.icontrol.temperature"
#define RESOURCE_TYPE_TSTAT_CTRL_SEQ_OP     "com.icontrol.tstatCtrlSeqOp"

#define RESOURCE_TYPE_BUTTON_PRESSED        "com.icontrol.buttonPressed"

#define RESOURCE_TYPE_MOVE_UP_OPERATION     "com.icontrol.moveUpOperation"
#define RESOURCE_TYPE_MOVE_DOWN_OPERATION   "com.icontrol.moveDownOperation"
#define RESOURCE_TYPE_STOP_OPERATION        "com.icontrol.stopOperation"

#define RESOURCE_TYPE_WARNING_TONE          "com.icontrol.warningTone"
#define RESOURCE_TYPE_SECURITY_STATE        "com.icontrol.securityState"
#define RESOURCE_TYPE_ZONE_CHANGED          "com.icontrol.zoneChanged"

#define RESOURCE_TYPE_BRIDGE_REFRESH        "com.icontrol.bridgeRefresh"
#define RESOURCE_TYPE_BRIDGE_REFRESH_STATE  "com.icontrol.bridgeRefreshState"
#define RESOURCE_TYPE_BRIDGE_CONFIGURATION_MODE  "com.icontrol.bridgeConfigurationMode"
#define RESOURCE_TYPE_BRIDGE_RESET          "com.icontrol.bridgeReset"
#define RESOURCE_TYPE_BRIDGE_RESET_SMOKE_SENSORS "com.icontrol.bridgeResetSmokeSensors"

#define RESOURCE_TYPE_BRAINSUCK_CONTROL     "com.icontrol.brainsuckControl"
#define RESOURCE_TYPE_BRAINSUCK_STATUS      "com.icontrol.brainsuckStatus"
#define RESOURCE_TYPE_PIM_FIRMWARE_UPGRADE  "com.icontrol.upgradePimFirmware"
#define RESOURCE_TYPE_PIM_INSTALLER_CODE_CONTROL "com.icontrol.installerCodeControl"

#define RESOURCE_TYPE_XBB_SIRENSTART        "com.comcast.xbb.sirenStart"
#define RESOURCE_TYPE_XBB_SIRENSTOP         "com.comcast.xbb.sirenStop"
#define RESOURCE_TYPE_XBB_SIRENMUTE         "com.comcast.xbb.sirenMute"
#define RESOURCE_TYPE_XBB_STATUS            "com.comcast.xbb.batteryStatus"
#define RESOURCE_TYPE_XBB_CONFIG            "com.comcast.xbb.batteryConfig"
#define RESOURCE_TYPE_XBB_ALARMS            "com.comcast.xbb.batteryAlarms"
#define RESOURCE_TYPE_XBBL_LID_STATUS        "com.comcast.xbbl.lidStatus"
#define RESOURCE_TYPE_XBBL_AC_POWER_LOST     "com.comcast.xbbl.acPowerLost"
#define RESOURCE_TYPE_XBBL_BANK_STATUS       "com.comcast.xbbl.bankStatus"
#define RESOURCE_TYPE_XBBL_TEMP_STATUS       "com.comcast.xbbl.tempStatus"

#define RESOURCE_TYPE_MODEM_POWER_ON        "com.icontrol.remoteCellModemPowerOn"
#define RESOURCE_TYPE_MODEM_POWER_OFF       "com.icontrol.remoteCellModemPowerOff"
#define RESOURCE_TYPE_MODEM_EMERGENCY_RESET "com.icontrol.remoteCellModemEmergencyReset"
#define RESOURCE_TYPE_MODEM_REBOOT_REASON   "com.icontrol.remoteCellModemRebootReason"
#define RESOURCE_TYPE_MODEM_RESYNC_UART     "com.icontrol.remoteCellModemResyncUart"

#define RESOURCE_TYPE_MAGNETIC_FIELD_STRENGTH     "com.icontrol.magneticFieldStrength"

#endif //FLEXCORE_RESOUCETYPES_H

