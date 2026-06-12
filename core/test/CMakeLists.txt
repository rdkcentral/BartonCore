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
    LINK_LIBRARIES BartonCoreStatic BartonCommon::xhDeviceHelper
    INCLUDES ${BARTON_PRIVATE_INCLUDES} ${PRIVATE_API_INCLUDES}
)

bcore_add_cmocka_test(
    NAME testDeviceDescriptorsAvailability
    TYPE unit
    TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/deviceDescriptorsAvailabilityTest.c
    WRAPPED_FUNCTIONS b_core_property_provider_has_property
                      b_core_property_provider_get_property_as_string
                      deviceServiceSetSystemProperty
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
        BartonCommon::xhTypes
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

# Observability no-op test: verifies all API calls are safe no-ops when linked
# against the noop backend.
bcore_add_cmocka_test(
    NAME testObservabilityNoop
    TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/observabilityNoopTest.c
    LINK_LIBRARIES BartonCoreStatic
    INCLUDES ${BARTON_PRIVATE_INCLUDES}
)

if (BCORE_OBSERVABILITY_BACKEND STREQUAL "otel")
    include(BCoreAddCppTest)

    bcore_add_cpp_test(
        NAME testObservabilityTracing
        SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/observabilityTracingTest.cpp
        LIBS BartonCoreStatic opentelemetry_exporter_in_memory opentelemetry_trace opentelemetry_resources opentelemetry_common
        INCLUDES ${BARTON_PRIVATE_INCLUDES}
    )
    # Fix static link ordering: gmock_main -> gmock -> gtest
    if(TARGET testObservabilityTracing)
        set_property(TARGET testObservabilityTracing PROPERTY LINK_LIBRARIES gmock_main gmock gtest BartonCoreStatic opentelemetry_exporter_in_memory opentelemetry_trace opentelemetry_resources opentelemetry_common)
    endif()

    bcore_add_cpp_test(
        NAME testObservabilityInit
        SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/observabilityInitTest.cpp
        LIBS BartonCoreStatic opentelemetry_trace opentelemetry_metrics opentelemetry_logs opentelemetry_resources opentelemetry_common opentelemetry_exporter_otlp_http opentelemetry_exporter_otlp_http_metric opentelemetry_exporter_otlp_http_log opentelemetry_otlp_recordable opentelemetry_http_client_curl
        INCLUDES ${BARTON_PRIVATE_INCLUDES}
    )
    if(TARGET testObservabilityInit)
        set_property(TARGET testObservabilityInit PROPERTY LINK_LIBRARIES gmock_main gmock gtest BartonCoreStatic opentelemetry_trace opentelemetry_metrics opentelemetry_logs opentelemetry_resources opentelemetry_common opentelemetry_exporter_otlp_http opentelemetry_exporter_otlp_http_metric opentelemetry_exporter_otlp_http_log opentelemetry_otlp_recordable opentelemetry_http_client_curl)
    endif()

    bcore_add_cpp_test(
        NAME testObservabilityMetrics
        SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/observabilityMetricsTest.cpp
        LIBS BartonCoreStatic opentelemetry_metrics opentelemetry_resources opentelemetry_common
        INCLUDES ${BARTON_PRIVATE_INCLUDES}
    )
    if(TARGET testObservabilityMetrics)
        set_property(TARGET testObservabilityMetrics PROPERTY LINK_LIBRARIES gmock_main gmock gtest BartonCoreStatic opentelemetry_metrics opentelemetry_resources opentelemetry_common)
    endif()

    bcore_add_cpp_test(
        NAME testObservabilityLogBridge
        SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/observabilityLogBridgeTest.cpp
        LIBS BartonCoreStatic opentelemetry_logs opentelemetry_trace opentelemetry_exporter_in_memory opentelemetry_resources opentelemetry_common
        INCLUDES ${BARTON_PRIVATE_INCLUDES}
    )
    if(TARGET testObservabilityLogBridge)
        set_property(TARGET testObservabilityLogBridge PROPERTY LINK_LIBRARIES gmock_main gmock gtest BartonCoreStatic opentelemetry_logs opentelemetry_trace opentelemetry_exporter_in_memory opentelemetry_resources opentelemetry_common)
    endif()

    bcore_add_cpp_test(
        NAME testDeviceTelemetry
        SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/deviceTelemetryTest.cpp
        LIBS BartonCoreStatic opentelemetry_metrics opentelemetry_resources opentelemetry_common
        INCLUDES ${BARTON_PRIVATE_INCLUDES}
    )
    if(TARGET testDeviceTelemetry)
        set_property(TARGET testDeviceTelemetry PROPERTY LINK_LIBRARIES gmock_main gmock gtest BartonCoreStatic opentelemetry_metrics opentelemetry_resources opentelemetry_common)
        target_link_options(testDeviceTelemetry PRIVATE
            -Wl,--wrap=deviceServiceGetDevice
            -Wl,--wrap=deviceServiceGetEndpointById)
    endif()
endif()

bcore_add_cmocka_test(
    NAME testDeviceServiceHash
    TYPE unit
    TEST_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/deviceServiceHashTest.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/deviceServiceHash.c
    WRAPPED_FUNCTIONS fread ferror
    LINK_LIBRARIES BartonCommon::xhLog BartonCommon::xhUtil
)

if (BCORE_MATTER)
    include(BCoreAddCppTest)
    include(BCoreConfigureGLib)

    bcore_add_cmocka_test(
        NAME sbmdParserTest
        INCLUDES ${PRIVATE_API_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR}/..
        TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/sbmdParserTest.cpp
        LINK_LIBRARIES BartonCoreStatic yaml-cpp
    )

    if (TARGET sbmdParserTest)
        target_compile_definitions(sbmdParserTest PRIVATE -DSBMD_SPEC_DIR="${CMAKE_SOURCE_DIR}/core/deviceDrivers/matter/sbmd/specs/")
    endif()

    bcore_add_cpp_test(
        NAME testMatterDeviceEndpointMap
        SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/MatterDeviceEndpointMapTest.cpp
        LIBS BartonCoreStatic ${BCORE_MATTER_LIB} ${OPENSSL_LINK_LIBRARIES} gmock
        INCLUDES ${BARTON_PRIVATE_INCLUDES}
                 ${PROJECT_SOURCE_DIR}/api/c/public
                 ${CMAKE_BINARY_DIR}/matter-install/include/matter
                 ${PROJECT_SOURCE_DIR}/core
    )

    bcore_add_cpp_test(
        NAME testMatterDevice
        SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/MatterDeviceTest.cpp
        LIBS BartonCoreStatic ${BCORE_MATTER_LIB} ${OPENSSL_LINK_LIBRARIES} gmock
        INCLUDES ${BARTON_PRIVATE_INCLUDES}
                 ${PROJECT_SOURCE_DIR}/api/c/public
                 ${CMAKE_BINARY_DIR}/matter-install/include/matter
                 ${PROJECT_SOURCE_DIR}/core
    )

    bcore_add_cpp_test(
        NAME testSbmdPrerequisites
        SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/sbmdPrerequisitesTest.cpp
        LIBS BartonCoreStatic ${BCORE_MATTER_LIB} ${OPENSSL_LINK_LIBRARIES} gmock
        INCLUDES ${BARTON_PRIVATE_INCLUDES}
                 ${PROJECT_SOURCE_DIR}/api/c/public
                 ${CMAKE_BINARY_DIR}/matter-install/include/matter
                 ${PROJECT_SOURCE_DIR}/core
    )

    # Select engine-specific sources and libraries for the SbmdScript test
    if (BCORE_MATTER_SBMD_JS_ENGINE STREQUAL "mquickjs")
        set(SBMD_SCRIPT_TEST_ENGINE_SOURCES
                ${PROJECT_SOURCE_DIR}/core/deviceDrivers/matter/sbmd/mquickjs/SbmdScriptImpl.cpp
                ${PROJECT_SOURCE_DIR}/core/deviceDrivers/matter/sbmd/mquickjs/MQuickJsRuntime.cpp
                ${PROJECT_SOURCE_DIR}/core/deviceDrivers/matter/sbmd/mquickjs/SbmdUtilsLoader.cpp
                ${PROJECT_SOURCE_DIR}/core/deviceDrivers/matter/sbmd/mquickjs/MQuickJsStdlib.c
        )
        set(SBMD_SCRIPT_TEST_ENGINE_LIBS mquickjs)
    elseif (BCORE_MATTER_SBMD_JS_ENGINE STREQUAL "quickjs")
        set(SBMD_SCRIPT_TEST_ENGINE_SOURCES
                ${PROJECT_SOURCE_DIR}/core/deviceDrivers/matter/sbmd/quickjs/SbmdScriptImpl.cpp
                ${PROJECT_SOURCE_DIR}/core/deviceDrivers/matter/sbmd/quickjs/QuickJsRuntime.cpp
                ${PROJECT_SOURCE_DIR}/core/deviceDrivers/matter/sbmd/quickjs/SbmdUtilsLoader.cpp
        )
        set(SBMD_SCRIPT_TEST_ENGINE_LIBS quickjs)
    endif()

    bcore_add_cpp_test(
        NAME testSbmdScript
        SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/SbmdScriptTest.cpp
                ${PROJECT_SOURCE_DIR}/core/deviceDrivers/matter/sbmd/ScriptResult.cpp
                ${SBMD_SCRIPT_TEST_ENGINE_SOURCES}
        LIBS ${BCORE_MATTER_LIB} ${OPENSSL_LINK_LIBRARIES} gmock ${SBMD_SCRIPT_TEST_ENGINE_LIBS} jsoncpp BartonCommon::xhLog
        INCLUDES ${BARTON_PRIVATE_INCLUDES}
                 ${CMAKE_BINARY_DIR}/matter-install/include/matter
                 ${PROJECT_SOURCE_DIR}/core
                 ${PROJECT_SOURCE_DIR}/core/deviceDrivers/matter/sbmd
    )

    if (TARGET testSbmdScript)
        target_link_libraries(testSbmdScript bCoreConfig)
        # Use a short timeout for tests so they don't wait 5 seconds.
        # The -U removes the definition from bCoreConfig before -D redefines it.
        target_compile_options(testSbmdScript PRIVATE
            -UBARTON_CONFIG_SBMD_SCRIPT_TIMEOUT_MS
            -DBARTON_CONFIG_SBMD_SCRIPT_TIMEOUT_MS=100)
    endif()

    bcore_add_cpp_test(
        NAME testScriptResult
        SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/ScriptResultTest.cpp
                ${PROJECT_SOURCE_DIR}/core/deviceDrivers/matter/sbmd/ScriptResult.cpp
        LIBS ${BCORE_MATTER_LIB} ${OPENSSL_LINK_LIBRARIES} gmock jsoncpp BartonCommon::xhLog
        INCLUDES ${BARTON_PRIVATE_INCLUDES}
                 ${CMAKE_BINARY_DIR}/matter-install/include/matter
                 ${PROJECT_SOURCE_DIR}/core
    )

    if (BUILD_TESTING)
        bcore_configure_glib()
    endif()
endif()
