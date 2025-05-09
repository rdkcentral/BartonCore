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

#--------------------------------------------------------------------------------------
#
# CMake definition for the fcore 'device service'
# Should not be utilized directly, but imported from our parent
# file via the 'add_subdirectory()' routine.
#
#--------------------------------------------------------------------------------------

include(FindPkgConfig)
include(BDSConfigureGLib)
include(BDSAddGLibTest)
include(BDSAddCMockaTest)
include(BDSAddCppTest)

file(GLOB_RECURSE BRTN_DS_API_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/src/*.c)
set(PRIVATE_API_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/public/private)

# define source files
# first the main source files (regardless of build options)
file(GLOB mainSrc "src/*.c")
file(GLOB eventSrc "src/event/*.c")
file(GLOB dbaseSrc "src/database/*.c")
file(GLOB deviceSrc "src/device/*.c")
file(GLOB serialSrc "src/serial/*.c")
set(SOURCES ${mainSrc} ${eventSrc} ${dbaseSrc} ${deviceSrc} ${serialSrc})

# philips lights (if option set)
if (BDS_PHILIPS_HUE)
    # add source files
    file(GLOB philipSrc "deviceDrivers/philipsHue*")
    set(SOURCES ${SOURCES} ${philipSrc})

    # include philips device library
    set(XTRA_LIBS ${XTRA_LIBS} xhPhilipsHue)
endif()

# zigbee (if option set)
if (BDS_ZIGBEE)

    # add source files
    file(GLOB zigSubSrc "src/subsystems/zigbee/*.c")
    file(GLOB_RECURSE zigbeeDrivers "deviceDrivers/zigbee/*.c")

    # include zhal library
    set(XTRA_LIBS ${XTRA_LIBS} zhal)
    list(APPEND XTRA_INCLUDES ${PROJECT_SOURCE_DIR}/libs/zhal/c/public)

endif() #BARTON_CONFIG_ZIGBEE

if (BDS_MATTER)
    file(GLOB matterSubSrc "src/subsystems/matter/*.cpp")
    file(GLOB matterSubProviderCommonSrc "src/subsystems/matter/delegates/*.cpp" "src/subsystems/matter/providers/*.cpp")

    file(GLOB matterDriversSrc "deviceDrivers/matter/*.cpp" "deviceDrivers/matter/clusters/*.cpp" "deviceDrivers/matter/subscriptions/*.cpp")

    add_compile_definitions(CHIP_HAVE_CONFIG_H=1)

    list(APPEND SOURCES
        ${matterSubSrc}
        ${matterSubProviderCommonSrc}
        ${BDS_MATTER_PROVIDER_IMPLEMENTATIONS}
        ${BDS_MATTER_DELEGATE_IMPLEMENTATIONS}
        ${matterDriversSrc}
        ${matterSrc}
    )

    # BartonMatter depends on openSSL and glib.
    # configure_glib will take care of setting up glib-2.0,
    # but be sure to explicitly set up openSSL linkage here
    # to avoid relying on magic dependencies to take care of this.

    pkg_check_modules(OPENSSL REQUIRED openssl)

    list(APPEND CMAKE_BUILD_RPATH "/usr/local/openssl/lib")
    list(APPEND CMAKE_INSTALL_RPATH "/usr/local/openssl/lib")
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath,/usr/local/openssl/lib")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-rpath,/usr/local/openssl/lib")

    list(APPEND XTRA_INCLUDES
        ${CMAKE_BINARY_DIR}/matter-install/include/matter
        ${OPENSSL_INCLUDE_DIRS}
        ${BDS_MATTER_PROVIDER_HEADER_PATHS}
        ${BDS_MATTER_DELEGATE_HEADER_PATHS})

    list(APPEND XTRA_LIBS ${BDS_MATTER_LIB} ${OPENSSL_LINK_LIBRARIES} certifier)
    link_directories(${CMAKE_BINARY_DIR}/matter-install/lib)
endif()

if (BDS_THREAD)
    pkg_check_modules(DBUS dbus-1 REQUIRED)

    file(GLOB_RECURSE thrSrc "src/subsystems/thread/*.cpp" "src/subsystems/thread/*.c")
    list(APPEND SOURCES ${thrSrc})

    list(APPEND XTRA_INCLUDES
        ${CMAKE_PREFIX_PATH}/include/otbr
        ${DBUS_INCLUDE_DIRS})

    list(APPEND XTRA_LIBS
        otbr-dbus-client
        otbr-dbus-common
        ${DBUS_LIBRARIES})
endif()

list(APPEND SOURCES
        ${SOURCES}
        ${zigSubSrc}
        ${zigbeeDrivers})

pkg_check_modules(CURL REQUIRED libcurl)
link_directories(${CURL_LIBRARY_DIRS})
list(APPEND XTRA_INCLUDES ${CURL_INCLUDE_DIRS})

# add library dependencies for this binary
set(btnDSLinkLibraries brtnDeviceServiceConfig xhLog xhTypes xhTime xhUtil
        xhConcurrent xhConfig xhUrlHelper xhXmlHelper xhJsonHelper
        xhDeviceHelper xhDeviceDescriptors xhXmlHelper
        xhCrypto ${XTRA_LIBS} cjson uuid curl xml2 z m)

if (BDS_SUPPORT_SOFTWARE_WATCHDOG)
    set(btnDSLinkLibraries ${btnDSLinkLibraries} xhSoftwareWatchdog)
endif()

#FIXME: Reduce copy-paste.
# Object library type compiles once, then can re-use those object files for
# statice/shared libraries.
add_library(brtnDeviceServiceObject OBJECT ${SOURCES} ${BRTN_DS_API_SOURCES})
target_link_libraries(brtnDeviceServiceObject ${btnDSLinkLibraries})
target_include_directories(brtnDeviceServiceObject PRIVATE
                            ${CMAKE_CURRENT_SOURCE_DIR}/src
                            ${CMAKE_CURRENT_SOURCE_DIR}/deviceDrivers
                            ${XTRA_INCLUDES}
                            ${CURRENT_SOURCE_GENERATED_DIR}/src
                            ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/src
                            ${PRIVATE_API_INCLUDES}
                            PUBLIC
                            ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/public
                            )

add_library(brtnDeviceServiceStatic STATIC $<TARGET_OBJECTS:brtnDeviceServiceObject>)
target_link_libraries(brtnDeviceServiceStatic ${btnDSLinkLibraries})
target_include_directories(brtnDeviceServiceStatic PRIVATE
                            ${CMAKE_CURRENT_SOURCE_DIR}/src
                            ${CMAKE_CURRENT_SOURCE_DIR}/deviceDrivers
                            ${XTRA_INCLUDES}
                            ${CURRENT_SOURCE_GENERATED_DIR}/src
                            ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/src
                            PUBLIC
                            ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/public
                            )
install(TARGETS brtnDeviceServiceStatic DESTINATION lib)

add_library(brtnDeviceServiceShared SHARED $<TARGET_OBJECTS:brtnDeviceServiceObject>)
target_link_libraries(brtnDeviceServiceShared ${btnDSLinkLibraries})
target_include_directories(brtnDeviceServiceShared PRIVATE
                            ${CMAKE_CURRENT_SOURCE_DIR}/src
                            ${CMAKE_CURRENT_SOURCE_DIR}/deviceDrivers
                            ${XTRA_INCLUDES}
                            ${CURRENT_SOURCE_GENERATED_DIR}/src
                            ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/src
                            PUBLIC
                            ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/public
                            )
install(TARGETS brtnDeviceServiceShared DESTINATION lib)

# If specified, generate GIR and typelib. Clients need to ensure that the GIR and/or typelib
# file is installed, as well is libbrtnDeviceServiceShared.so. Additionally,
# if libbrtnDeviceServiceShared.so is compiled with sanitizer, clients need to set
# LD_PRELOAD ex:
# LD_PRELOAD=$(gcc -print-file-name=libasan.so) python3 my_app.py
# LD_PRELOAD=$(clang -print-file-name=libclang_rt.asan-x86_64.so) python3 my_app.py
if (BDS_GEN_GIR)
    find_program(G_IR_SCANNER g-ir-scanner)
    if (NOT G_IR_SCANNER)
        message(FATAL_ERROR "g-ir-scanner not found")
    endif()

    find_program(G_IR_COMPILER g-ir-compiler)
    if (NOT G_IR_COMPILER)
        message(FATAL_ERROR "g-ir-compiler not found")
    endif()

    set(NAMESPACE BDeviceService)
    set(NAMESPACE_VERSION 1.0)
    set(GIR_OUTPUT_NAME ${NAMESPACE}-${NAMESPACE_VERSION}.gir)
    set(GIR_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${GIR_OUTPUT_NAME})
    set(TYPELIB_OUTPUT_NAME ${NAMESPACE}-${NAMESPACE_VERSION}.typelib)
    set(TYPELIB_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${TYPELIB_OUTPUT_NAME})

    # The list of --libraries should be only the minimal amount necessary
    # to resolve at run time (and the loader should be able to find everything
    # else through RUNPATH in a client program). However, the scanner needs
    # paths to dependent libraries even if RUNPATH is set in libbrtnDeviceServiceShared.so.
    # I don't know why.

    set(SCANNER_ARGS
    --namespace=${NAMESPACE}
    --symbol-prefix=b_device_service
    --nsversion=${NAMESPACE_VERSION}
    --warn-all
    --no-libtool
    --output=${GIR_OUTPUT}
    --include=GObject-2.0
    --include=Gio-2.0
    --library=brtnDeviceServiceShared
    --library=stdc++
    --library-path=${CMAKE_PREFIX_PATH}/lib
    -I$<JOIN:$<TARGET_PROPERTY:brtnDeviceServiceShared,INCLUDE_DIRECTORIES>, -I>)

    file(GLOB API_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/public/*.h
                           ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/public/events/*.h
                           ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/public/provider/*.h)
    file(GLOB API_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/src/*.c
                          ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/src/events/*.c
                          ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/src/provider/*.c)
    string(REPLACE ";" " " SCANNER_ARGS "${SCANNER_ARGS}")
    string(REPLACE ";" " " API_INCLUDES "${API_INCLUDES}")
    string(REPLACE ";" " " API_SOURCES "${API_SOURCES}")

    # We're going to dynamically create the command to execute in a script.
    # The reason being is trying to create the command in CMake and pass it
    # directly to add_custom_command() may result in a file name too long error.
    # Probably I just needed to add VERBATIM to the add_custom_command call, but
    # I realized that after the fact.
    # This command has to go through two stages (in order). First we use configure_file
    # to inject CMake variables into the script. Then we use file(GENERAGE ...) because
    # the injected variables contain generator expressions.
    configure_file(run_g-ir.in run_g-ir.in @ONLY)
    file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/run_g-ir INPUT ${CMAKE_CURRENT_BINARY_DIR}/run_g-ir.in)

    add_custom_command(
        TARGET brtnDeviceServiceShared POST_BUILD
        COMMAND env LD_LIBRARY_PATH=/usr/local/openssl/lib /bin/sh ${CMAKE_CURRENT_BINARY_DIR}/run_g-ir
        COMMENT "Invoking gobject introspection"
    )

    # gir/typelib files fall under subdirs in paths defined by
    # XDG_DATA_DIRS. We know we wanna be in /usr so just use that prefix
    set(EXPECTED_XDG_DATA_DIR "/usr")

    install(FILES ${GIR_OUTPUT} DESTINATION ${EXPECTED_XDG_DATA_DIR}/share/gir-1.0)
    install(FILES ${TYPELIB_OUTPUT} DESTINATION ${EXPECTED_XDG_DATA_DIR}/lib/girepository-1.0)

    # Next up is python stubs, which are good for development.
    set(BDS_LIBRARY_LOCATION ${CMAKE_INSTALL_PREFIX}/lib)
    set(GIR_NAME ${NAMESPACE})
    set(GIR_VERSION ${NAMESPACE_VERSION})
    # We install this in our Dockerfile
    set(GENERATE_DIR /usr/bin/pygobject-stubs)
    set(OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/pygobject-stubs-generate-output.txt)

    if (BDS_BUILD_WITH_ASAN)
        if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            set(ASAN_LD_PRELOAD "LD_PRELOAD=$(clang -print-file-name=libclang_rt.asan-x86_64.so)")
        elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            set(ASAN_LD_PRELOAD "LD_PRELOAD=$(gcc -print-file-name=libasan.so)")
        endif()
    endif()

    configure_file(gen_pygobject_stubs.in gen_pygobject_stubs @ONLY)

    # We need to run gen_pygobject_stubs after install for it to work.
    install(CODE "execute_process(COMMAND /bin/sh ${CMAKE_CURRENT_BINARY_DIR}/gen_pygobject_stubs)")
endif()


bds_add_cmocka_test(
    NAME testSubsystemManager
    TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/src/subsystems/subsystemManagerTest.c
    WRAPPED_FUNCTIONS deviceServiceGetSystemProperty deviceServiceSetSystemProperty
    LINK_LIBRARIES brtnDeviceServiceStatic
    INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/src ${PRIVATE_API_INCLUDES}
)

# add our 'unit test' to be part of the "make unitTest"
# TODO: The generic test components should not be gated by zigbee!
if (BDS_ZIGBEE)
    bds_add_cmocka_test(
            NAME testZigbeeSubsystem
            TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/src/subsystems/zigbee/zigbeeSubsystemTest.c
            WRAPPED_FUNCTIONS deviceServiceGetDevicesBySubsystem deviceServiceGetDeviceDescriptorForDevice deviceServiceConfigurationGetFirmwareFileDir
            LINK_LIBRARIES brtnDeviceServiceStatic
            INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/src ${PRIVATE_API_INCLUDES}
    )

    bds_add_cmocka_test(
            NAME testDeviceService
            TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/src/deviceServiceTest.c
            WRAPPED_FUNCTIONS
                jsonDatabaseRestore
                subsystemManagerRestoreConfig
            LINK_LIBRARIES brtnDeviceServiceStatic
            INCLUDES ${PRIVATE_API_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    bds_add_cmocka_test(
            NAME testZigbeeDriverCommon
            TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/deviceDrivers/zigbeeDriverCommonTest.c
            WRAPPED_FUNCTIONS deviceServiceGetDevicesBySubsystem
            LINK_LIBRARIES brtnDeviceServiceStatic
            INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/src ${PRIVATE_API_INCLUDES}
    )

    bds_add_cmocka_test(
            NAME testJsonDatabase
            TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/src/database/jsonDatabaseTest.c
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
            LINK_LIBRARIES brtnDeviceServiceStatic xhDeviceHelper
            INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/src ${PRIVATE_API_INCLUDES}
    )

    bds_add_cmocka_test(
            NAME testDeviceDescriptorsAvailability
            TYPE unit
            TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/src/deviceDescriptorsAvailabilityTest.c
            WRAPPED_FUNCTIONS b_device_service_property_provider_has_property
                              b_device_service_property_provider_get_property_as_string
                              deviceServiceSetSystemProperty digestFile
                              deviceServiceGetSystemProperty urlHelperDownloadFile

            LINK_LIBRARIES brtnDeviceServiceStatic xhDeviceDescriptors
            INCLUDES ${PRIVATE_API_INCLUDES}
    )

    if(TARGET testJsonDatabase)
        set(JSONDB_FIXTURES "${CMAKE_CURRENT_LIST_DIR}/test/etc")
        target_compile_definitions(testJsonDatabase PRIVATE -DFIXTURES_DIR="${JSONDB_FIXTURES}")
    endif()

    if(TARGET testDeviceDescriptorsAvailability)
        target_compile_definitions(testDeviceDescriptorsAvailability PRIVATE -DTEST_DIR="${CMAKE_CURRENT_LIST_DIR}/test/")
    endif()
endif()

bds_add_cmocka_test(
    NAME testCommFail
    TYPE unit
    INCLUDES
        src
        public
        ../api/c/public
        ${PRIVATE_API_INCLUDES}
        $<TARGET_PROPERTY:xhDeviceDescriptors,INCLUDE_DIRECTORIES>
    TEST_SOURCES
        test/src/commFailTest.c
        src/deviceServiceCommFail.c
    LINK_LIBRARIES
        xhTypes
        #glib-2 applied by configure_glib
)

if (BDS_MATTER)
    bds_add_cpp_test(NAME testMatterConfigureSubscriptionSpecs
        # TODO: We shouldn't have to explicitly specify the OpenSSL libraries here. Figure this out later.
        LIBS ${OPENSSL_LINK_LIBRARIES} brtnDeviceServiceStatic
        INCLUDES ${OPENSSL_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/deviceDrivers ${CMAKE_BINARY_DIR}/matter-install/include/matter ${PRIVATE_API_INCLUDES} ${XTRA_INCLUDES}
        SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/deviceDrivers/matter/test/MatterConfigureSubscriptionSpecsTest.cpp
    )

    bds_add_cpp_test(NAME testMatterSubscribeInteraction
        # TODO: For some reason, this target requires these libraries to be linked in a different order from the others.
        # Changing the link order within the bds_add_cpp_test function itself just caused the other targets to fail
        # to link. I don't know why, but for now, we're just explicitly specifying the link order here for this one.
        LIBS brtnDeviceServiceStatic gtest gmock gmock_main
        INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/deviceDrivers ${CMAKE_BINARY_DIR}/matter-install/include/matter ${PRIVATE_API_INCLUDES} ${XTRA_INCLUDES}
        SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/deviceDrivers/matter/test/MatterSubscribeInteractionTest.cpp
    )

    add_subdirectory("src/subsystems/matter/test")
endif()

# API tests
bds_add_glib_test(
    NAME device-service-resource-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/device-service-resource-test.c
    GLOG_DOMAIN BDeviceServiceResourceTest
)

bds_add_glib_test(
    NAME device-service-metadata-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/device-service-metadata-test.c
    GLOG_DOMAIN BDeviceServiceMetadataTest
)

bds_add_glib_test(
    NAME device-service-endpoint-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/device-service-endpoint-test.c
    GLOG_DOMAIN BDeviceServiceEndpointTest
)

bds_add_glib_test(
    NAME device-service-device-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/device-service-device-test.c
    GLOG_DOMAIN BDeviceServiceDeviceTest
)

bds_add_glib_test(
    NAME device-service-discovery-filter-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/device-service-discovery-filter-test.c
    GLOG_DOMAIN BDeviceServiceDiscoveryFilterTest
)

bds_add_glib_test(
    NAME device-service-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/device-service-event-test.c
    GLOG_DOMAIN BDeviceServiceEventTest
)

bds_add_glib_test(
    NAME device-service-status-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/device-service-status-test.c
    GLOG_DOMAIN BDeviceServiceStatusTest
)

bds_add_glib_test(
    NAME device-service-device-found-details-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/device-service-device-found-details-test.c
    GLOG_DOMAIN BDeviceServiceDeviceFoundDetailsTest
)

bds_add_glib_test(
    NAME device-service-discovery-started-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/device-service-discovery-started-event-test.c
    GLOG_DOMAIN BDeviceServiceDiscoveryStartedEventTest
)

bds_add_glib_test(
    NAME device-service-recovery-started-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/device-service-recovery-started-event-test.c
    GLOG_DOMAIN BDeviceServiceRecoveryStartedEventTest
)

bds_add_glib_test(
    NAME device-service-discovery-session-info-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/device-service-discovery-session-info-event-test.c
    GLOG_DOMAIN BDeviceServiceDiscoverySessionInfoEventTest
)

bds_add_glib_test(
    NAME device-service-device-discovery-failed-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/device-service-device-discovery-failed-event-test.c
    GLOG_DOMAIN BDeviceServiceDeviceDiscoveryFailedEventTest
)

bds_add_glib_test(
    NAME device-service-device-discovered-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/device-service-device-discovered-event-test.c
    GLOG_DOMAIN BDeviceServiceDeviceDiscoverEventTest
)

bds_add_glib_test(
    NAME device-service-device-rejected-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/device-service-device-rejected-event-test.c
    GLOG_DOMAIN BDeviceServiceDeviceRejectedEventTest
)

bds_add_glib_test(
    NAME device-service-device-configuration-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/device-service-device-configuration-event-test.c
    GLOG_DOMAIN BDeviceServiceDeviceConfigurationEventTest
)

bds_add_glib_test(
    NAME device-service-device-added-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/device-service-device-added-event-test.c
    GLOG_DOMAIN BDeviceServiceDeviceAddedEventTest
)

bds_add_glib_test(
    NAME device-service-device-recovered-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/device-service-device-recovered-event-test.c
    GLOG_DOMAIN BDeviceServiceDeviceRecoveredEventTest
)

bds_add_glib_test(
    NAME device-service-device-discovery-completed-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/device-service-device-discovery-completed-event-test.c
    GLOG_DOMAIN BDeviceServiceDeviceDiscoveryCompletedEventTest
)

bds_add_glib_test(
    NAME device-service-discovery-stopped-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/device-service-discovery-stopped-event-test.c
    GLOG_DOMAIN BDeviceServiceDiscoveryStoppedEventTest
)

bds_add_glib_test(
    NAME device-service-metadata-updated-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/device-service-metadata-updated-event-test.c
    GLOG_DOMAIN BDeviceServiceMetadataUpdatedEventTest
)

bds_add_glib_test(
    NAME device-service-recovery-stopped-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/device-service-recovery-stopped-event-test.c
    GLOG_DOMAIN BDeviceServiceRecoveryStoppedEventTest
)

bds_add_glib_test(
    NAME device-service-resource-updated-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/device-service-resource-updated-event-test.c
    GLOG_DOMAIN BDeviceServiceResourceUpdatedEventTest
)

bds_add_glib_test(
    NAME device-service-endpoint-removed-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/device-service-endpoint-removed-event-test.c
    GLOG_DOMAIN BDeviceServiceEndpointRemovedEventTest
)

bds_add_glib_test(
    NAME device-service-endpoint-added-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/device-service-endpoint-added-event-test.c
    GLOG_DOMAIN BDeviceServiceEndpointAddedEventTest
)

bds_add_glib_test(
    NAME device-service-device-removed-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/device-service-device-removed-event-test.c
    GLOG_DOMAIN BDeviceServiceDeviceRemovedEventTest
)

bds_add_glib_test(
    NAME device-service-zigbee-channel-changed-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/device-service-zigbee-channel-changed-event-test.c
    GLOG_DOMAIN BDeviceServiceZigbeeChannelChangedEventTest
)

bds_add_glib_test(
    NAME device-service-storage-changed-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/device-service-storage-changed-event-test.c
    GLOG_DOMAIN BDeviceServiceStorageChangedEventTest
)

bds_add_glib_test(
    NAME device-service-zigbee-interference-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/device-service-zigbee-interference-event-test.c
    GLOG_DOMAIN BDeviceServiceZigbeeInterferenceEventTest
)

bds_add_glib_test(
    NAME device-service-zigbee-pan-id-attack-changed-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/device-service-zigbee-pan-id-attack-changed-event-test.c
    GLOG_DOMAIN BDeviceServiceZigbeePanIdAttackChangedEventTest
)

bds_add_glib_test(
    NAME device-service-device-database-failure-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/device-service-device-database-failure-event-test.c
    GLOG_DOMAIN BDeviceServiceDeviceDatabaseFailureEventTest
)

bds_add_glib_test(
    NAME device-service-utils-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/device-service-utils-test.c
    INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/src ${PRIVATE_API_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR}/src
    GLOG_DOMAIN BDeviceServiceUtilsTest
)

bds_add_cmocka_test(
    NAME device-service-client-test
    TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/device-service-client-test.c
    WRAPPED_FUNCTIONS
        deviceServiceInitialize
        deviceServiceStart
        allServicesAvailableNotify
        deviceServiceShutdown
        deviceServiceGetEndpointsByProfile
        deviceServiceDiscoverStart
        deviceServiceDiscoverStop
        deviceServiceCommissionDevice
        deviceServiceAddMatterDevice
        deviceServiceGetStatus
        deviceServiceGetDevice
        deviceServiceGetEndpointById
        deviceServiceGetEndpointByUri
        deviceServiceGetAllDevices
        deviceServiceGetDevicesByDeviceClass
        deviceServiceGetResourceByUri
        deviceServiceGetDevicesBySubsystem
        deviceServiceGetDeviceByUri
        deviceServiceGetResourceByUri
        deviceServiceWriteResource
        deviceServiceExecuteResource
        deviceServiceRemoveDevice
        deviceServiceGetSystemProperty
        deviceServiceSetSystemProperty
        deviceServiceRemoveEndpointById
        deviceServiceSetMetadata
        deviceServiceGetMetadata
        deviceServiceGetResourcesByUriPattern
        deviceServiceChangeResourceMode
        zigbeeSubsystemChangeChannel
        deviceServiceGetMetadataByUriPattern
        deviceServiceProcessDeviceDescriptors
        zhalTest
        zigbeeSubsystemPerformEnergyScan
        deviceServiceRestoreConfig
    LINK_LIBRARIES brtnDeviceServiceStatic
    INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/src ${PRIVATE_API_INCLUDES}
)
if(TARGET device-service-client-test)
    target_compile_definitions(device-service-client-test PRIVATE G_LOG_DOMAIN="BDeviceServiceClientTest")
endif()

bds_add_cmocka_test(
    NAME device-service-property-provider-test
    TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/provider/device-service-property-provider-test.c
    WRAPPED_FUNCTIONS
        deviceServiceGetSystemProperty
        deviceServiceSetSystemProperty
    LINK_LIBRARIES brtnDeviceServiceStatic
    INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/src
)
if(TARGET device-service-property-provider-test)
    target_compile_definitions(device-service-property-provider-test PRIVATE G_LOG_DOMAIN="BDeviceServicePropertyProviderTest")
endif()

bds_add_cmocka_test(
    NAME deviceServiceConfigurationTest
    TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/src/deviceServiceConfigurationTest.c
    WRAPPED_FUNCTIONS
    LINK_LIBRARIES brtnDeviceServiceStatic
    INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/src
)

bds_configure_glib()
