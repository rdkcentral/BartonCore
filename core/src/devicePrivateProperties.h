//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2024 Comcast
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
// Comcast retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------

/*
 * Created by Christian Leithner on 9/9/2024.
 */

#pragma once

// FIXME: tech debt, these are old zilker properties

#define DEVICE_DESCRIPTOR_LIST                                "deviceDescriptorList"
#define DEVICE_DESC_ALLOWLIST_URL_OVERRIDE                    "deviceDescriptor.whitelist.url.override"
#define DEVICE_DESC_DENYLIST                                  "deviceDescriptor.blacklist"
#define POSIX_TIME_ZONE_PROP                                  "POSIX_TZ"
#define DEVICE_FIRMWARE_URL_NODE                              "deviceFirmwareBaseUrl"
#define CPE_DENYLISTED_DEVICES_PROPERTY_NAME                  "cpe.denylistedDevices"
#define TOUCHSCREEN_SENSOR_COMMFAIL_TROUBLE_DELAY             "touchscreen.sensor.commFail.troubleDelay"
#define ZIGBEE_PROPS_PREFIX                                   "cpe.zigbee."
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