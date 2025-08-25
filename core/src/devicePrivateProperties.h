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

/*
 * Created by Christian Leithner on 9/9/2024.
 */

#pragma once

// FIXME: tech debt, these are old properties

#define DEVICE_DESCRIPTOR_LIST                                "deviceDescriptorList"
#define DEVICE_DESC_ALLOWLIST_URL_OVERRIDE                    "deviceDescriptor.whitelist.url.override"
#define DEVICE_DESC_DENYLIST                                  "deviceDescriptor.blacklist"
#define POSIX_TIME_ZONE_PROP                                  "POSIX_TZ"
#define DEVICE_FIRMWARE_URL_NODE                              "deviceFirmwareBaseUrl"
#define CPE_DENYLISTED_DEVICES_PROPERTY_NAME                  "cpe.denylistedDevices"
#define TOUCHSCREEN_SENSOR_COMMFAIL_TROUBLE_DELAY             "touchscreen.sensor.commFail.troubleDelay"
#define TELEMETRY_PROPS_PREFIX                                "telemetry."
#define FAST_COMM_FAIL_PROP                                   "zigbee.testing.fastCommFail.flag"
#define CPE_ZIGBEE_REPORT_DEVICE_INFO_ENABLED                 "cpe.zigbee.reportDeviceInfo.enabled"
#define CPE_DIAGNOSTIC_ZIGBEEDATA_ENABLED                     "cpe.diagnostics.zigBeeData.enabled"
#define CPE_DIAGNOSTIC_ZIGBEEDATA_PER_CHANNEL_NUMBER_OF_SCANS "cpe.diagnostics.zigBeeData.numberOfScansPerChannel"
#define CPE_DIAGNOSTIC_ZIGBEEDATA_CHANNEL_SCAN_DURATION_MS    "cpe.diagnostics.zigBeeData.channelScanDurationMs"
#define CPE_DIAGNOSTIC_ZIGBEEDATA_CHANNEL_SCAN_DELAY_MS       "cpe.diagnostics.zigBeeData.perChannelScanDelayMs"
#define CPE_DIAGNOSTIC_ZIGBEEDATA_COLLECTION_DELAY_MIN        "cpe.diagnostics.zigBeeData.preCollectionDelayMinutes"
#define TEST_FASTTIMERS_PROP                                  "cpe.tests.fastTimers"
#define CPE_ZIGBEE_CHANNEL_CHANGE_ENABLED_KEY                 "cpe.zigbee.channelChange.enabled"
#define CPE_ZIGBEE_CHANNEL_CHANGE_MAX_REJOIN_WAITTIME_MINUTES "cpe.zigbee.channelChange.maxWaitTimeMin"
#define CPE_REGION_CODE_PROPERTY_NAME                         "country_code"
#define ZIGBEE_HEALTH_CHECK_INTERVAL_MILLIS                   "cpe.zigbee.healthCheck.intervalMillis"
#define ZIGBEE_HEALTH_CHECK_CCA_THRESHOLD                     "cpe.zigbee.healthCheck.ccaThreshold"
#define ZIGBEE_HEALTH_CHECK_CCA_FAILURE_THRESHOLD             "cpe.zigbee.healthCheck.ccaFailureThreshold"
#define ZIGBEE_HEALTH_CHECK_RESTORE_THRESHOLD                 "cpe.zigbee.healthCheck.restoreThreshold"
#define ZIGBEE_HEALTH_CHECK_DELAY_BETWEEN_THRESHOLD_RETRIES_MILLIS                                                     \
    "cpe.zigbee.healthCheck.delayBetweenThresholdRetriesMillis"
#define ZIGBEE_DEFENDER_PAN_ID_CHANGE_THRESHOLD_OPTION      "cpe.zigbee.defender.panIdChangeThreshold"
#define ZIGBEE_DEFENDER_PAN_ID_CHANGE_WINDOW_MILLIS_OPTION  "cpe.zigbee.defender.panIdChangeWindowMillis"
#define ZIGBEE_DEFENDER_PAN_ID_CHANGE_RESTORE_MILLIS_OPTION "cpe.zigbee.defender.panIdChangeRestoreMillis"
#define ZIGBEE_FW_UPGRADE_NO_DELAY_BOOL_PROPERTY            "zigbee.fw.upgrade.nodelay.flag"
