# ------------------------------ tabstop = 4 ----------------------------------
#
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2025 Comcast Cable Communications Management, LLC
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

#
# Created by Christian Leithner on 5/23/2025.
#

bcore_add_cmocka_test(
    NAME testSubsystemManager
    TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/subsystems/subsystemManagerTest.c
    WRAPPED_FUNCTIONS deviceServiceGetSystemProperty deviceServiceSetSystemProperty
    LINK_LIBRARIES BartonCoreStatic
    INCLUDES ${BARTON_PRIVATE_INCLUDES} ${PRIVATE_API_INCLUDES}
)

bcore_add_cmocka_test(
    NAME testDeviceService
    TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/deviceServiceTest.c
    WRAPPED_FUNCTIONS
        jsonDatabaseRestore
        subsystemManagerRestoreConfig
        deviceDriverManagerGetDeviceDriversByDeviceClass
        sendDiscoveryStartedEvent
        sendDiscoveryStoppedEvent
        sendRecoveryStoppedEvent
        deviceDescriptorsListIsReady
    LINK_LIBRARIES BartonCoreStatic
    INCLUDES ${PRIVATE_API_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR}/src
)

bcore_add_cmocka_test(
    NAME testJsonDatabase
    TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/database/jsonDatabaseTest.c
    WRAPPED_FUNCTIONS
        storageLoad
        storageLoadJSON
        storageParse
        storageParseBad
        storageSave
        storageGetKeys
        storageDelete
        storageRestoreNamespace
        simpleProtectConfigData
        simpleUnprotectConfigData
    LINK_LIBRARIES BartonCoreStatic xhDeviceHelper
    INCLUDES ${BARTON_PRIVATE_INCLUDES} ${PRIVATE_API_INCLUDES}
)

bcore_add_cmocka_test(
    NAME testDeviceDescriptorsAvailability
    TYPE unit
    TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/deviceDescriptorsAvailabilityTest.c
    WRAPPED_FUNCTIONS b_core_property_provider_has_property
                      b_core_property_provider_get_property_as_string
                      deviceServiceSetSystemProperty digestFile
                      deviceServiceGetSystemProperty urlHelperDownloadFile

    LINK_LIBRARIES BartonCoreStatic xhDeviceDescriptors
    INCLUDES ${PRIVATE_API_INCLUDES}
)

if(TARGET testJsonDatabase)
    set(JSONDB_FIXTURES "${CMAKE_CURRENT_LIST_DIR}/etc")
    target_compile_definitions(testJsonDatabase PRIVATE -DFIXTURES_DIR="${JSONDB_FIXTURES}")
endif()

if(TARGET testDeviceDescriptorsAvailability)
    target_compile_definitions(testDeviceDescriptorsAvailability PRIVATE -DTEST_DIR="${CMAKE_CURRENT_LIST_DIR}/")
endif()

if (BCORE_ZIGBEE)
    bcore_add_cmocka_test(
            NAME testZigbeeSubsystem
            TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/subsystems/zigbee/zigbeeSubsystemTest.c
            WRAPPED_FUNCTIONS deviceServiceGetDevicesBySubsystem deviceServiceGetDeviceDescriptorForDevice
                              deviceServiceConfigurationGetFirmwareFileDir subsystemManagerRegister
            LINK_LIBRARIES BartonCoreStatic
            INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/src ${PRIVATE_API_INCLUDES} ${BARTON_PRIVATE_INCLUDES}
    )

    bcore_add_cmocka_test(
            NAME testZigbeeDriverCommon
            TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/deviceDrivers/zigbee/zigbeeDriverCommonTest.c
            WRAPPED_FUNCTIONS deviceServiceGetDevicesBySubsystem
            LINK_LIBRARIES BartonCoreStatic
            INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/src ${PRIVATE_API_INCLUDES}
    )
endif()

if (BCORE_MATTER)
    add_subdirectory("src/subsystems/matter")
endif()

bcore_add_cmocka_test(
    NAME testCommFail
    TYPE unit
    INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
        ${PROJECT_SOURCE_DIR}/api/c/public
        ${PRIVATE_API_INCLUDES}
        $<TARGET_PROPERTY:xhDeviceDescriptors,INCLUDE_DIRECTORIES>
    TEST_SOURCES
        src/commFailTest.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/deviceServiceCommFail.c
    LINK_LIBRARIES
        xhTypes
        #glib-2 applied by configure_glib
)

bcore_add_cmocka_test(
    NAME deviceServiceConfigurationTest
    TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/deviceServiceConfigurationTest.c
    WRAPPED_FUNCTIONS
    LINK_LIBRARIES BartonCoreStatic
    INCLUDES ${BARTON_PRIVATE_INCLUDES}
)

bcore_add_cmocka_test(
    NAME deviceCommunicationWatchdogTest
    INCLUDES ${PRIVATE_API_INCLUDES}
    TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/deviceCommunicationWatchdogTest.c
    WRAPPED_FUNCTIONS b_core_property_provider_get_property_as_bool
    LINK_LIBRARIES BartonCoreStatic
)
