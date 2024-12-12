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
 * Created by Thomas Lea on 3/29/16.
 */

#ifndef FLEXCORE_ZIGBEECOMMONIDS_H
#define FLEXCORE_ZIGBEECOMMONIDS_H

#define HA_PROFILE_ID                                                                        0x0104

// Manufacturer IDs
#define COMCAST_MFG_ID                                                                       0x111d
// Some devices in the field are using this incorrect Comcast Mfg Id, so we must support and track it
#define COMCAST_MFG_ID_INCORRECT                                                             0x109d
#define ICONTROL_MFG_ID                                                                      0x1022

// Basic Cluster
#define BASIC_CLUSTER_ID                                                                     0x0000
#define BASIC_APPLICATION_VERSION_ATTRIBUTE_ID                                               0x0001
#define BASIC_HARDWARE_VERSION_ATTRIBUTE_ID                                                  0x0003
#define BASIC_MANUFACTURER_NAME_ATTRIBUTE_ID                                                 0x0004
#define BASIC_MODEL_IDENTIFIER_ATTRIBUTE_ID                                                  0x0005
#define DATE_CODE_ATTRIBUTE_ID                                                               0x0006
#define BASIC_RESET_TO_FACTORY_COMMAND_ID                                                    0x00
#define BASIC_REBOOT_DEVICE_MFG_COMMAND_ID                                                   0x01
#define COMCAST_BASIC_CLUSTER_MFG_SPECIFIC_MODEM_REBOOT_REASON_ATTRIBUTE_ID                  0x0000

// OnOff Cluster
#define ON_OFF_CLUSTER_ID                                                                    0x0006
#define ON_OFF_ATTRIBUTE_ID                                                                  0x0000
#define ON_OFF_TURN_OFF_COMMAND_ID                                                           0x00
#define ON_OFF_TURN_ON_COMMAND_ID                                                            0x01

// Level Control Cluster
#define LEVEL_CONTROL_CLUSTER_ID                                                             0x0008
#define LEVEL_CONTROL_CURRENT_LEVEL_ATTRIBUTE_ID                                             0x0000
#define LEVEL_CONTROL_MOVE_TO_LEVEL_WITH_ON_OFF_COMMAND_ID                                   0x0004
#define LEVEL_CONTROL_CLUSTER_ON_OFF_TRANSITION_TIME_ATTRIBUTE_ID                            0x0010
#define LEVEL_CONTROL_ON_LEVEL_ATTRIBUTE_ID                                                  0x0011

// Color Control Cluster
#define COLOR_CONTROL_CLUSTER_ID                                                             0x0300
#define COLOR_CONTROL_CURRENTX_ATTRIBUTE_ID                                                  0x0003
#define COLOR_CONTROL_CURRENTY_ATTRIBUTE_ID                                                  0x0004
#define COLOR_CONTROL_MOVE_TO_COLOR_COMMAND_ID                                               0x07

// Thermostat Cluster
#define THERMOSTAT_CLUSTER_ID                                                                0x0201
#define THERMOSTAT_LOCAL_TEMPERATURE_ATTRIBUTE_ID                                            0x0000
#define THERMOSTAT_OCCUPIED_COOLING_SETPOINT_ATTRIBUTE_ID                                    0x0011
#define THERMOSTAT_OCCUPIED_HEATING_SETPOINT_ATTRIBUTE_ID                                    0x0012
#define THERMOSTAT_CTRL_SEQ_OP_ATTRIBUTE_ID                                                  0x001b
#define THERMOSTAT_SYSTEM_MODE_ATTRIBUTE_ID                                                  0x001c
#define THERMOSTAT_LOCAL_TEMPERATURE_CALIBRATION_ATTRIBUTE_ID                                0x0010
#define THERMOSTAT_SETPOINT_HOLD_ATTRIBUTE_ID                                                0x0023
#define THERMOSTAT_RUNNING_STATE_ATTRIBUTE_ID                                                0x0029
#define THERMOSTAT_ABS_MIN_HEAT_SETPOINT_ATTRIBUTE_ID                                        0x0003
#define THERMOSTAT_ABS_MAX_HEAT_SETPOINT_ATTRIBUTE_ID                                        0x0004
#define THERMOSTAT_ABS_MIN_COOL_SETPOINT_ATTRIBUTE_ID                                        0x0005
#define THERMOSTAT_ABS_MAX_COOL_SETPOINT_ATTRIBUTE_ID                                        0x0006

// Fan Control Cluster
#define FAN_CONTROL_CLUSTER_ID                                                               0x0202
#define FAN_CONTROL_FAN_MODE_ATTRIBUTE_ID                                                    0x0000

// OTA Cluster
#define OTA_UPGRADE_CLUSTER_ID                                                               0x0019
#define OTA_CURRENT_FILE_VERSION_ATTRIBUTE_ID                                                0x0002
#define OTA_IMAGE_NOTIFY_COMMAND_ID                                                          0x00

// IAS Zone Cluster
#define IAS_ZONE_CLUSTER_ID                                                                  0x0500

// IAS Ancillary Control Equipment Cluster
#define IAS_ACE_CLUSTER_ID                                                                   0x0501

// Poll Control Cluster
#define POLL_CONTROL_CLUSTER_ID                                                              0x0020

// Door Lock Cluster
#define DOORLOCK_CLUSTER_ID                                                                  0x0101
#define DOORLOCK_LOCK_STATE_ATTRIBUTE_ID                                                     0x0000
#define DOORLOCK_NUM_PIN_USERS_SUPPORTED_ATTRIBUTE_ID                                        0x0012
#define DOORLOCK_MAX_PIN_CODE_LENGTH_ATTRIBUTE_ID                                            0x0017
#define DOORLOCK_MIN_PIN_CODE_LENGTH_ATTRIBUTE_ID                                            0x0018
#define DOORLOCK_AUTO_RELOCK_TIME_ATTRIBUTE_ID                                               0x0023
#define DOORLOCK_ENABLE_LOCAL_PROGRAMMING_ATTRIBUTE_ID                                       0x0028
#define DOORLOCK_USER_CODE_TEMPORARY_DISABLE_TIME                                            0x0031
#define DOORLOCK_SEND_PIN_OVER_THE_AIR_ATTRIBUTE_ID                                          0x0032
#define DOORLOCK_ALARM_MASK_ATTRIBUTE_ID                                                     0x0040
#define DOORLOCK_KEYPAD_OPERATION_EVENT_MASK_ATTRIBUTE_ID                                    0x0041
#define DOORLOCK_RF_OPERATION_EVENT_MASK_ATTRIBUTE_ID                                        0x0042
#define DOORLOCK_MANUAL_OPERATION_EVENT_MASK_ATTRIBUTE_ID                                    0x0043
#define DOORLOCK_KEYPAD_PROGRAMMING_EVENT_MASK_ATTRIBUTE_ID                                  0x0045
#define DOORLOCK_RF_PROGRAMMING_EVENT_MASK_ATTRIBUTE_ID                                      0x0046
#define DOORLOCK_LOCK_DOOR_COMMAND_ID                                                        0x00
#define DOORLOCK_UNLOCK_DOOR_COMMAND_ID                                                      0x01
#define DOORLOCK_SET_PIN_CODE_COMMAND_ID                                                     0x05
#define DOORLOCK_GET_PIN_CODE_COMMAND_ID                                                     0x06
#define DOORLOCK_CLEAR_PIN_CODE_COMMAND_ID                                                   0x07
#define DOORLOCK_CLEAR_ALL_PIN_CODES_COMMAND_ID                                              0x08
#define DOORLOCK_SET_PIN_CODE_RESPONSE_COMMAND_ID                                            0x05
#define DOORLOCK_GET_PIN_CODE_RESPONSE_COMMAND_ID                                            0x06
#define DOORLOCK_CLEAR_PIN_CODE_RESPONSE_COMMAND_ID                                          0x07
#define DOORLOCK_CLEAR_ALL_PIN_CODES_RESPONSE_COMMAND_ID                                     0x08
#define DOORLOCK_OPERATION_EVENT_NOTIFICATION_COMMAND_ID                                     0x20
#define DOORLOCK_PROGRAMMING_EVENT_NOTIFICATION_COMMAND_ID                                   0x21

// Occupancy Sensing
#define OCCUPANCY_SENSING_CLUSTER_ID                                                         0x0406
#define OCCUPANCY_SENSING_OCCUPANCY_ATTRIBUTE_ID                                             0x0000

// Alarms
#define ALARMS_CLUSTER_ID                                                                    0x0009
#define ALARMS_ALARM_COUNT_ATTRIBUTE_ID                                                      0x0000
#define ALARMS_GET_ALARM_COMMAND_ID                                                          0x02
#define ALARMS_ALARM_COMMAND_ID                                                              0x00
#define ALARMS_GET_ALARM_RESPONSE_COMMAND_ID                                                 0x01
#define ALARMS_CLEAR_ALARM_COMMAND_ID                                                        0x02

// Power Configurationg
#define POWER_CONFIGURATION_CLUSTER_ID                                                       0x0001
#define MAINS_ALARM_MASK_ATTRIBUTE_ID                                                        0x0010
#define MAINS_VOLTAGE_ATTRIBUTE_ID                                                           0x0000
#define BATTERY_VOLTAGE_ATTRIBUTE_ID                                                         0x0020
#define BATTERY_PERCENTAGE_REMAINING_ATTRIBUTE_ID                                            0x0021
#define BATTERY_ALARM_MASK_ATTRIBUTE_ID                                                      0x0035
#define BATTERY_VOLTAGE_MIN_THRESHOLD_ATTRIBUTE_ID                                           0x0036
#define BATTERY_VOLTAGE_THRESHOLD1_ATTRIBUTE_ID                                              0x0037
#define BATTERY_VOLTAGE_THRESHOLD2_ATTRIBUTE_ID                                              0x0038
#define BATTERY_VOLTAGE_THRESHOLD3_ATTRIBUTE_ID                                              0x0039
#define BATTERY_PERCENTAGE_MIN_THRESHOLD_ATTRIBUTE_ID                                        0x003a
#define BATTERY_PERCENTAGE_THRESHOLD1_ATTRIBUTE_ID                                           0x003b
#define BATTERY_PERCENTAGE_THRESHOLD2_ATTRIBUTE_ID                                           0x003c
#define BATTERY_PERCENTAGE_THRESHOLD3_ATTRIBUTE_ID                                           0x003d
#define BATTERY_ALARM_STATE_ATTRIBUTE_ID                                                     0x003e
#define COMCAST_POWER_CONFIGURATION_CLUSTER_MFG_SPECIFIC_BATTERY_RECHARGE_CYCLE_ATTRIBUTE_ID 0x000d

// Relative Humidity
#define RELATIVE_HUMIDITY_MEASUREMENT_CLUSTER_ID                                             0x0405
#define RELATIVE_HUMIDITY_MEASURED_VALUE_ATTRIBUTE_ID                                        0x0000

// Diagnostics
#define DIAGNOSTICS_CLUSTER_ID                                                               0x0b05
#define DIAGNOSTICS_LAST_MESSAGE_LQI_ATTRIBUTE_ID                                            0x011c
#define DIAGNOSTICS_LAST_MESSAGE_RSSI_ATTRIBUTE_ID                                           0x011d

// Device Temperature Configuration
#define DEVICE_TEMPERATURE_CONFIGURATION_CLUSTER_ID                                          0x0002
#define CURRENT_TEMPERATURE_ATTRIBUTE_ID                                                     0x0000
#define MIN_TEMPERATURE_EXPERIENCED_ATTRIBUTE_ID                                             0x0001
#define MAX_TEMPERATURE_EXPERIENCED_ATTRIBUTE_ID                                             0x0002
#define DEVICE_TEMPERATURE_ALARM_MASK_ATTRIBUTE_ID                                           0x0010
#define LOW_TEMPERATURE_THRESHOLD_ATTRIBUTE_ID                                               0x0011
#define HIGH_TEMPERATURE_THRESHOLD_ATTRIBUTE_ID                                              0x0012
#define LOW_TEMPERATURE_DWELL_TRIP_POINT_ATTRIBUTE_ID                                        0x0013
#define HIGH_TEMPERATURE_DWELL_TRIP_POINT_ATTRIBUTE_ID                                       0x0014

// Temperature Measurement
#define TEMPERATURE_MEASUREMENT_CLUSTER_ID                                                   0x0402
#define TEMP_MEASURED_VALUE_ATTRIBUTE_ID                                                     0x0000

// Electrical Measurement
#define ELECTRICAL_MEASUREMENT_CLUSTER_ID                                                    0x0b04
#define ELECTRICAL_MEASUREMENT_ACTIVE_POWER_ATTRIBUTE_ID                                     0x050b
#define ELECTRICAL_MEASUREMENT_AC_MULTIPLIER_ATTRIBUTE_ID                                    0x0604
#define ELECTRICAL_MEASUREMENT_AC_DIVISOR_ATTRIBUTE_ID                                       0x0605

// Metering
#define METERING_CLUSTER_ID                                                                  0x0702
#define METERING_INSTANTANEOUS_DEMAND_ATTRIBUTE_ID                                           0x0400
#define METERING_MULTIPLIER_ATTRIBUTE_ID                                                     0x0301
#define METERING_DIVISOR_ATTRIBUTE_ID                                                        0x0302

// Window Covering
#define WINDOW_COVERING_CLUSTER_ID                                                           0x0102
#define WINDOW_COVERING_CONTROLLER_DEVICE_ID                                                 0x0202
#define WINDOW_COVERING_UP_COMMAND_ID                                                        0x00
#define WINDOW_COVERING_DOWN_COMMAND_ID                                                      0x01
#define WINDOW_COVERING_STOP_COMMAND_ID                                                      0x02

// Bridge
#define BRIDGE_CLUSTER_ID                                                                    0xFD00

// M1LTE-specific clusters
#define REMOTE_CELL_MODEM_CLUSTER_ID                                                         0xfd03
#define REMOTE_CELL_MODEM_POWER_STATUS_ATTRIBUTE_ID                                          0x0000
#define REMOTE_CELL_MODEM_POWER_ON_COMMAND_ID                                                0x00
#define REMOTE_CELL_MODEM_OFF_COMMAND_ID                                                     0x01
#define REMOTE_CELL_MODEM_RESET_COMMAND_ID                                                   0x02

// Zigbee UART cluster (internal to ZigbeeCore: not for direct consumption)
// Use zhal UART API.
#define ZIGBEE_UART_CLUSTER                                                                  0xfd04

// XBB
#define XBB_CLUSTER_ID                                                                       0xfd02
#define XBB_CLUSTER_STATUS_ATTRIBUTE_ID                                                      0x00
#define XBB_CLUSTER_STATUS_COMMAND_ID                                                        0x00

// Remote CLI
#define REMOTE_CLI_CLUSTER_ID                                                                0xFD50
#define REMOTE_CLI_CLI_COMMAND_COMMAND_ID                                                    0x00
#define REMOTE_CLI_ENABLE_REMOTE_CLI_COMMAND_ID                                              0x01
#define REMOTE_CLI_CLI_COMMAND_RESPONSE_COMMAND_ID                                           0x00

// Device ids
#define ON_OFF_LIGHT_DEVICE_ID                                                               0x0100
#define ON_OFF_PLUGIN_UNIT_DEVICE_ID                                                         0x010a
#define DIMMABLE_LIGHT_DEVICE_ID                                                             0x0101
#define DIMMABLE_PLUGIN_UNIT_DEVICE_ID                                                       0x010b
#define COLOR_DIMMABLE_LIGHT_DEVICE_ID                                                       0x0102
#define COLOR_DIMMABLE2_LIGHT_DEVICE_ID                                                      0x0200
#define EXTENDED_COLOR_LIGHT_DEVICE_ID                                                       0x010d
#define EXTENDED_COLOR2_LIGHT_DEVICE_ID                                                      0x0210
#define COLOR_TEMPERATURE_LIGHT_DEVICE_ID                                                    0x010c
#define COLOR_TEMPERATURE2_LIGHT_DEVICE_ID                                                   0x0220
#define ON_OFF_LIGHT_SWITCH_DEVICE_ID                                                        0x0103
#define DIMMABLE_LIGHT_SWITCH_DEVICE_ID                                                      0x0104
#define COLOR_DIMMABLE_LIGHT_SWITCH_DEVICE_ID                                                0x0105
#define IAS_ACE_DEVICE_ID                                                                    0x0401
#define SENSOR_DEVICE_ID                                                                     0x0402
#define THERMOSTAT_DEVICE_ID                                                                 0x0301
#define DOORLOCK_DEVICE_ID                                                                   0x000a
#define WINDOW_COVERING_DEVICE_ID                                                            0x0202
#define LIGHT_CONTROLLER_DEVICE_ID                                                           0x0820
#define SMART_PLUG_DEVICE_ID                                                                 0x0051
#define LEGACY_ICONTROL_SENSOR_DEVICE_ID                                                     0xffff
#define MATTER_SPEAKER_DEVICE_ID                                                             0x0022

#endif // FLEXCORE_ZIGBEECOMMONIDS_H
