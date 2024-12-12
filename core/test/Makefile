# ------------------------------ tabstop = 4 ----------------------------------
#
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2024 Comcast Cable Communications Management, LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0
#
# ------------------------------ tabstop = 4 ----------------------------------

#----------------------------------------------------
#
# makefile for DeviceService test
#
#
#----------------------------------------------------




LOCAL_STATIC_LIBS := \
    ../build/objs/src/deviceService.o \
    ../build/objs/src/deviceDriverManager.o \
    ../build/objs/src/deviceDescriptorHandler.o \
    ../build/objs/src/subsystemManager.o \
    ../build/objs/src//subsystems/zigbee/zhalEventHandler.o \
    ../build/objs/src//subsystems/zigbee/zigbeeSubsystem.o \
    ../build/objs/src//deviceCommunicationWatchdog.o \
    ../build/objs/src/ipc/deviceEventProducer.o \
    ../build/objs/src/ipc/deviceService_ipc_handler.o \
    ../build/objs/src/ipc/deviceServiceIpcHandler.o \
    ../build/objs/src/ipc/deviceServiceIpcCommon.o \
    ../build/objs/src/ipc/deviceServiceEventHandler.o \
    ../build/objs/src/device/icDeviceEndpoint.o \
    ../build/objs/src/device/icDeviceMetadata.o \
    ../build/objs/src/device/icDevice.o \
    ../build/objs/src/device/icDeviceResource.o \
    ../build/objs/src/database/database.o \
    ../build/objs/src/database/databaseInitializer.o \
    ../build/objs/src/deviceModelHelper.o \
    ../build/objs/deviceDrivers/openHomeCamera/openHomeCameraDeviceDriver.o \
    ../build/objs/deviceDrivers/openHomeCamera/cameraDevice.o \
    ../build/objs/deviceDrivers/openHomeCamera/cameraSet.o \
    ../build/objs/deviceDrivers//centraliteGarageDoorController/centraliteGarageDoorControllerDeviceDriver.o \
    ../build/objs/deviceDrivers/test/testDeviceDriver.o \
    ../build/objs/deviceDrivers/philipsHue/philipsHueDeviceDriver.o \
    ../build/objs/deviceDrivers/rtcoaWifi/rtcoaWifiDeviceDriver.o \
    ../build/objs/deviceDrivers/xbb/xbbDeviceDriver.o \
    ../build/objs/deviceDrivers/zigbeeDriverCommon.o \
    ../build/objs/deviceDrivers/zigbeeLight/zigbeeLightDeviceDriver.o \
    ../build/objs/deviceDrivers/zigbeeSensor/zigbeeSensorDeviceDriver.o \
    ../build/objs/deviceDrivers/zigbeeThermostat/zigbeeThermostatDeviceDriver.o \
    ../build/objs/deviceDrivers/zigbeeDoorLock/zigbeeDoorLockDeviceDriver.o \

# define variables that our build-*.mk files need
#
LOCAL_MODULE := testDeviceService
LOCAL_CFLAGS := -I../public -I.. -I../database -I$(CROSS_OUTPUT)/include/libxml2

LOCAL_LDFLAGS := \
                -licLog \
                -licTypes \
                -ldeviceServiceAPI \
                -lwatchdogServiceAPI \
                -lbackupRestoreServiceAPI \
                -lpropsServiceAPI \
                -licSystem \
                -lcjson -licIpc \
                -lopenHomeCamera \
                -lurlHelper \
                -ldeviceDescriptors \
                -lssdp \
                -lphilipsHue \
                -lrtcoaWifi \
                -licUtil \
                -lxmlHelper \
                -lxml2 \
                -lcmocka \
                -lsqlite3 \
                -lcurl \
                -lpthread \
                -lm \
				-lzhal \

LOCAL_SRC_FILES := $(wildcard src/*.c)

# add targets/rules for making the executable
#
include ${ZILKER_TOP}/buildTools/make/build-exe.mk

