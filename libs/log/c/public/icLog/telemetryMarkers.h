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
// Created by dcalde202 on 6/18/20.
//

#ifndef ZILKER_TELEMETRYMARKERS_H
#define ZILKER_TELEMETRYMARKERS_H

// Marker Prefixes

#define TELEMETRY_MARKER_PREFIX_INFO                     "INFO_"
#define TELEMETRY_MARKER_PREFIX_ERROR                    "ERROR_"

// Full Marker Strings
#define TELEMETRY_MARKER_BL_PARSING_ERROR                "ZBE_ERROR_BLParsing"
#define TELEMETRY_MARKER_BL_DOWNLOAD_ERROR               "ZBE_ERROR_BLDownload"
#define TELEMETRY_MARKER_WL_PARSING_ERROR                "ZBE_ERROR_WLParsing"
#define TELEMETRY_MARKER_WL_DOWNLOAD_ERROR               "ZBE_ERROR_WLDownload"
#define TELEMETRY_MARKER_DEVICE_COMMFAIL                 "ZBE_ERROR_DevCommfail"
#define TELEMETRY_MARKER_DEVICE_COMMRESTORE              "ZBE_INFO_DevCommRes"
#define TELEMETRY_MARKER_DEVICE_FIRMWARE_UPGRADE_START   "ZBE_INFO_devFWUpgStarted"
#define TELEMETRY_MARKER_DEVICE_FIRMWARE_UPGRADE_SUCCESS "ZBE_INFO_devFWUpgSuccess"
#define TELEMETRY_MARKER_DEVICE_FIRMWARE_UPGRADE_FAILED  "ZBE_ERROR_devFWUpgFailed"
#define TELEMETRY_MARKER_XBB_COMM_FAIL                   "XBB_CommunicationFailure"
#define TELEMETRY_MARKER_XBB_LOW_VOLTAGE                 "XBB_ERROR_LowVoltage"
#define TELEMETRY_MARKER_XBB_LID_OPEN                    "XBB_LidOpen_true"
#define TELEMETRY_MARKER_XBB_LID_CLOSED                  "XBB_LidOpen_false"

#define TELEMETRY_MARKER_STARTUP_ERROR                   "ERROR_startup"

// Backup Restore Markers
#define TELEMETRY_MARKER_BACKUPRESTORE_PREFIX            "BR_"
#define TELEMETRY_MARKER_BACKUPRESTORE_BACKUP_FAILED                                                                   \
    TELEMETRY_MARKER_BACKUPRESTORE_PREFIX TELEMETRY_MARKER_PREFIX_ERROR "backupFailed"
#define TELEMETRY_MARKER_BACKUPRESTORE_BACKUP_SUCCESSFUL                                                               \
    TELEMETRY_MARKER_BACKUPRESTORE_PREFIX TELEMETRY_MARKER_PREFIX_INFO "backupSuccessful"

// CSLT template validation markers
#define TELEMETRY_MARKER_AUTOMATION_TEMPLATE_PREFIX "HS_AutomationTemplate_"
#define TELEMETRY_MARKER_TEMPLATE_SCHEMA_INSUFFICIENT_INPUTS                                                           \
    TELEMETRY_MARKER_AUTOMATION_TEMPLATE_PREFIX "InsufficientInputs"
#define TELEMETRY_MARKER_TEMPLATE_SCHEMA_DUPLICATE_INPUTS TELEMETRY_MARKER_AUTOMATION_TEMPLATE_PREFIX "DuplicateInputs"
#define TELEMETRY_MARKER_TEMPLATE_SCHEMA_INCORRECT_DATA_TYPE_INPUT                                                     \
    TELEMETRY_MARKER_AUTOMATION_TEMPLATE_PREFIX "InvalidDataType"
#define TELEMETRY_MARKER_TEMPLATE_SCHEMA_EXTRA_INPUT TELEMETRY_MARKER_AUTOMATION_TEMPLATE_PREFIX "ExtraInput"
#define TELEMETRY_MARKER_TEMPLATE_SCHEMA_NONEXISTENT_DEVICE_ID_INPUT                                                   \
    TELEMETRY_MARKER_AUTOMATION_TEMPLATE_PREFIX "NonexistentDeviceIdInput"

// AS2 markers
#define TELEMETRY_MARKER_AS2_SUCCESS "as2DeliverySuccess"
#define TELEMETRY_MARKER_AS2_FAIL    "as2DeliveryFail"

#endif // ZILKER_TELEMETRYMARKERS_H
