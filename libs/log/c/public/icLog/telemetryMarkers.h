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
// Created by dcalde202 on 6/18/20.
//

#ifndef ZILKER_TELEMETRYMARKERS_H
#define ZILKER_TELEMETRYMARKERS_H

// Marker Prefixes

#define TELEMETRY_MARKER_PREFIX_INFO "INFO_"
#define TELEMETRY_MARKER_PREFIX_ERROR "ERROR_"

// Full Marker Strings
#define TELEMETRY_MARKER_BL_PARSING_ERROR "ZBE_ERROR_BLParsing"
#define TELEMETRY_MARKER_BL_DOWNLOAD_ERROR "ZBE_ERROR_BLDownload"
#define TELEMETRY_MARKER_WL_PARSING_ERROR "ZBE_ERROR_WLParsing"
#define TELEMETRY_MARKER_WL_DOWNLOAD_ERROR "ZBE_ERROR_WLDownload"
#define TELEMETRY_MARKER_DEVICE_COMMFAIL "ZBE_ERROR_DevCommfail"
#define TELEMETRY_MARKER_DEVICE_COMMRESTORE "ZBE_INFO_DevCommRes"
#define TELEMETRY_MARKER_DEVICE_FIRMWARE_UPGRADE_START "ZBE_INFO_devFWUpgStarted"
#define TELEMETRY_MARKER_DEVICE_FIRMWARE_UPGRADE_SUCCESS "ZBE_INFO_devFWUpgSuccess"
#define TELEMETRY_MARKER_DEVICE_FIRMWARE_UPGRADE_FAILED "ZBE_ERROR_devFWUpgFailed"
#define TELEMETRY_MARKER_XBB_COMM_FAIL "XBB_CommunicationFailure"
#define TELEMETRY_MARKER_XBB_LOW_VOLTAGE "XBB_ERROR_LowVoltage"
#define TELEMETRY_MARKER_XBB_LID_OPEN "XBB_LidOpen_true"
#define TELEMETRY_MARKER_XBB_LID_CLOSED "XBB_LidOpen_false"

#define TELEMETRY_MARKER_STARTUP_ERROR "ERROR_startup"

// Backup Restore Markers
#define TELEMETRY_MARKER_BACKUPRESTORE_PREFIX "BR_"
#define TELEMETRY_MARKER_BACKUPRESTORE_BACKUP_FAILED TELEMETRY_MARKER_BACKUPRESTORE_PREFIX TELEMETRY_MARKER_PREFIX_ERROR "backupFailed"
#define TELEMETRY_MARKER_BACKUPRESTORE_BACKUP_SUCCESSFUL TELEMETRY_MARKER_BACKUPRESTORE_PREFIX TELEMETRY_MARKER_PREFIX_INFO "backupSuccessful"

// CSLT template validation markers
#define TELEMETRY_MARKER_AUTOMATION_TEMPLATE_PREFIX "HS_AutomationTemplate_"
#define TELEMETRY_MARKER_TEMPLATE_SCHEMA_INSUFFICIENT_INPUTS TELEMETRY_MARKER_AUTOMATION_TEMPLATE_PREFIX"InsufficientInputs"
#define TELEMETRY_MARKER_TEMPLATE_SCHEMA_DUPLICATE_INPUTS TELEMETRY_MARKER_AUTOMATION_TEMPLATE_PREFIX"DuplicateInputs"
#define TELEMETRY_MARKER_TEMPLATE_SCHEMA_INCORRECT_DATA_TYPE_INPUT TELEMETRY_MARKER_AUTOMATION_TEMPLATE_PREFIX"InvalidDataType"
#define TELEMETRY_MARKER_TEMPLATE_SCHEMA_EXTRA_INPUT TELEMETRY_MARKER_AUTOMATION_TEMPLATE_PREFIX"ExtraInput"
#define TELEMETRY_MARKER_TEMPLATE_SCHEMA_NONEXISTENT_DEVICE_ID_INPUT TELEMETRY_MARKER_AUTOMATION_TEMPLATE_PREFIX"NonexistentDeviceIdInput"

// AS2 markers
#define TELEMETRY_MARKER_AS2_SUCCESS "as2DeliverySuccess"
#define TELEMETRY_MARKER_AS2_FAIL "as2DeliveryFail"

#endif //ZILKER_TELEMETRYMARKERS_H
