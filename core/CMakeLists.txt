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
include(BCoreConfigureGLib)
include(BCoreAddGLibTest)
include(BCoreAddCMockaTest)
include(BCoreAddCppTest)
include(BCoreFindIncludeDir)

file(GLOB_RECURSE B_CORE_API_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/src/*.c)
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
if (BCORE_PHILIPS_HUE)
    # add source files
    file(GLOB philipSrc "deviceDrivers/philipsHue*")
    set(SOURCES ${SOURCES} ${philipSrc})

    # include philips device library
    set(XTRA_LIBS ${XTRA_LIBS} xhPhilipsHue)
endif()

# zigbee (if option set)
if (BCORE_ZIGBEE)

    # add source files
    file(GLOB zigSubSrc "src/subsystems/zigbee/*.c")
    file(GLOB_RECURSE zigbeeDrivers "deviceDrivers/zigbee/*.c")

    # include zhal library
    set(XTRA_LIBS ${XTRA_LIBS} zhal)
    list(APPEND XTRA_INCLUDES ${PROJECT_SOURCE_DIR}/libs/zhal/c/public)

endif() #BARTON_CONFIG_ZIGBEE

if (BCORE_MATTER)
    file(GLOB matterSubSrc "src/subsystems/matter/*.cpp")
    file(GLOB matterSubProviderCommonSrc "src/subsystems/matter/delegates/*.cpp" "src/subsystems/matter/providers/*.cpp")

    file(GLOB matterDriversSrc "deviceDrivers/matter/*.cpp" "deviceDrivers/matter/clusters/*.cpp" "deviceDrivers/matter/subscriptions/*.cpp")

    add_compile_definitions(CHIP_HAVE_CONFIG_H=1)

    list(APPEND SOURCES
        ${matterSubSrc}
        ${matterSubProviderCommonSrc}
        ${BCORE_MATTER_PROVIDER_IMPLEMENTATIONS}
        ${BCORE_MATTER_DELEGATE_IMPLEMENTATIONS}
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

    bds_find_include(
        MATTER_INCLUDE_DIR
        access/AccessRestrictionProvider.h
        matter
        "Matter include directory"
        XTRA_INCLUDES
    )

    list(APPEND XTRA_INCLUDES
        ${OPENSSL_INCLUDE_DIRS}
        ${BCORE_MATTER_PROVIDER_HEADER_PATHS}
        ${BCORE_MATTER_DELEGATE_HEADER_PATHS})

    list(APPEND XTRA_LIBS ${BCORE_MATTER_LIB} ${OPENSSL_LINK_LIBRARIES} certifier)
    link_directories(${CMAKE_BINARY_DIR}/matter-install/lib)
endif()

if (BCORE_THREAD)
    pkg_check_modules(DBUS dbus-1 REQUIRED)

    file(GLOB_RECURSE thrSrc "src/subsystems/thread/*.cpp" "src/subsystems/thread/*.c")
    list(APPEND SOURCES ${thrSrc})

    bds_find_include(
        OTBR_INCLUDE_DIR
        openthread-br/config.h
        otbr
        "OTBR include directory"
        XTRA_INCLUDES
    )

    list(APPEND XTRA_INCLUDES ${DBUS_INCLUDE_DIRS})

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
set(bCoreLinkLibraries bCoreConfig xhLog xhTypes xhTime xhUtil
        xhConcurrent xhConfig xhUrlHelper xhXmlHelper xhJsonHelper
        xhDeviceHelper xhDeviceDescriptors xhXmlHelper
        xhCrypto ${XTRA_LIBS} cjson uuid curl xml2 z m)

if (BCORE_SUPPORT_SOFTWARE_WATCHDOG)
    set(bCoreLinkLibraries ${bCoreLinkLibraries} xhSoftwareWatchdog)
endif()

#FIXME: Reduce copy-paste.
# Object library type compiles once, then can re-use those object files for
# statice/shared libraries.
add_library(bCoreObject OBJECT ${SOURCES} ${B_CORE_API_SOURCES})
target_link_libraries(bCoreObject ${bCoreLinkLibraries})
target_include_directories(bCoreObject PRIVATE
                            ${CMAKE_CURRENT_SOURCE_DIR}/src
                            ${CMAKE_CURRENT_SOURCE_DIR}/deviceDrivers
                            ${XTRA_INCLUDES}
                            ${CURRENT_SOURCE_GENERATED_DIR}/src
                            ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/src
                            ${PRIVATE_API_INCLUDES}
                            PUBLIC
                            ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/public
                            )

add_library(BartonCoreStatic STATIC $<TARGET_OBJECTS:bCoreObject>)
target_link_libraries(BartonCoreStatic ${bCoreLinkLibraries})
target_include_directories(BartonCoreStatic PRIVATE
                            ${CMAKE_CURRENT_SOURCE_DIR}/src
                            ${CMAKE_CURRENT_SOURCE_DIR}/deviceDrivers
                            ${XTRA_INCLUDES}
                            ${CURRENT_SOURCE_GENERATED_DIR}/src
                            ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/src
                            PUBLIC
                            ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/public
                            )

add_library(BartonCore SHARED $<TARGET_OBJECTS:bCoreObject>)
target_link_libraries(BartonCore ${bCoreLinkLibraries})
target_include_directories(BartonCore PRIVATE
                            ${CMAKE_CURRENT_SOURCE_DIR}/src
                            ${CMAKE_CURRENT_SOURCE_DIR}/deviceDrivers
                            ${XTRA_INCLUDES}
                            ${CURRENT_SOURCE_GENERATED_DIR}/src
                            ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/src
                            PUBLIC
                            ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/public
                            )
install(TARGETS BartonCore DESTINATION lib)

# If specified, generate GIR and typelib. Clients need to ensure that the GIR and/or typelib
# file is installed, as well is libBartonCore.so. Additionally,
# if libBartonCore.so is compiled with sanitizer, clients need to set
# LD_PRELOAD ex:
# LD_PRELOAD=$(gcc -print-file-name=libasan.so) python3 my_app.py
# LD_PRELOAD=$(clang -print-file-name=libclang_rt.asan-x86_64.so) python3 my_app.py
if (BCORE_GEN_GIR)
    find_program(G_IR_SCANNER g-ir-scanner)
    if (NOT G_IR_SCANNER)
        message(FATAL_ERROR "g-ir-scanner not found")
    endif()

    find_program(G_IR_COMPILER g-ir-compiler)
    if (NOT G_IR_COMPILER)
        message(FATAL_ERROR "g-ir-compiler not found")
    endif()

    set(NAMESPACE BCore)
    set(NAMESPACE_VERSION 1.0)
    set(GIR_OUTPUT_NAME ${NAMESPACE}-${NAMESPACE_VERSION}.gir)
    set(GIR_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${GIR_OUTPUT_NAME})
    set(TYPELIB_OUTPUT_NAME ${NAMESPACE}-${NAMESPACE_VERSION}.typelib)
    set(TYPELIB_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${TYPELIB_OUTPUT_NAME})

    # The list of --libraries should be only the minimal amount necessary
    # to resolve at run time (and the loader should be able to find everything
    # else through RUNPATH in a client program). However, the scanner needs
    # paths to dependent libraries even if RUNPATH is set in libbCore.so.
    # I don't know why.

    set(SCANNER_ARGS
    --namespace=${NAMESPACE}
    --symbol-prefix=b_core
    --nsversion=${NAMESPACE_VERSION}
    --warn-all
    --no-libtool
    --output=${GIR_OUTPUT}
    --include=GObject-2.0
    --include=Gio-2.0
    --library=BartonCore
    --library=stdc++
    --library-path=${CMAKE_PREFIX_PATH}/lib
    -I$<JOIN:$<TARGET_PROPERTY:BartonCore,INCLUDE_DIRECTORIES>, -I>)

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
        TARGET BartonCore POST_BUILD
        COMMAND env LD_LIBRARY_PATH=/usr/local/openssl/lib /bin/sh ${CMAKE_CURRENT_BINARY_DIR}/run_g-ir
        COMMENT "Invoking gobject introspection"
    )

    # gir/typelib files fall under subdirs in paths defined by
    # XDG_DATA_DIRS. We know we wanna be in /usr so just use that prefix
    set(EXPECTED_XDG_DATA_DIR "/usr")

    install(FILES ${GIR_OUTPUT} DESTINATION ${EXPECTED_XDG_DATA_DIR}/share/gir-1.0)
    install(FILES ${TYPELIB_OUTPUT} DESTINATION ${EXPECTED_XDG_DATA_DIR}/lib/girepository-1.0)

    # Next up is python stubs, which are good for development.
    set(BCORE_LIBRARY_LOCATION ${CMAKE_INSTALL_PREFIX}/lib)
    set(GIR_NAME ${NAMESPACE})
    set(GIR_VERSION ${NAMESPACE_VERSION})
    # We install this in our Dockerfile
    set(GENERATE_DIR /usr/bin/pygobject-stubs)
    set(OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/pygobject-stubs-generate-output.txt)

    if (BCORE_BUILD_WITH_ASAN)
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
    LINK_LIBRARIES BartonCoreStatic
    INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/src ${PRIVATE_API_INCLUDES}
)

# add our 'unit test' to be part of the "make unitTest"
# TODO: The generic test components should not be gated by zigbee!
if (BCORE_ZIGBEE)
    bds_add_cmocka_test(
            NAME testZigbeeSubsystem
            TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/src/subsystems/zigbee/zigbeeSubsystemTest.c
            WRAPPED_FUNCTIONS deviceServiceGetDevicesBySubsystem deviceServiceGetDeviceDescriptorForDevice deviceServiceConfigurationGetFirmwareFileDir
            LINK_LIBRARIES BartonCoreStatic
            INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/src ${PRIVATE_API_INCLUDES}
    )

    bds_add_cmocka_test(
            NAME testDeviceService
            TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/src/deviceServiceTest.c
            WRAPPED_FUNCTIONS
                jsonDatabaseRestore
                subsystemManagerRestoreConfig
            LINK_LIBRARIES BartonCoreStatic
            INCLUDES ${PRIVATE_API_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    bds_add_cmocka_test(
            NAME testZigbeeDriverCommon
            TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/deviceDrivers/zigbeeDriverCommonTest.c
            WRAPPED_FUNCTIONS deviceServiceGetDevicesBySubsystem
            LINK_LIBRARIES BartonCoreStatic
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
            LINK_LIBRARIES BartonCoreStatic xhDeviceHelper
            INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/src ${PRIVATE_API_INCLUDES}
    )

    bds_add_cmocka_test(
            NAME testDeviceDescriptorsAvailability
            TYPE unit
            TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/src/deviceDescriptorsAvailabilityTest.c
            WRAPPED_FUNCTIONS b_core_property_provider_has_property
                              b_core_property_provider_get_property_as_string
                              deviceServiceSetSystemProperty digestFile
                              deviceServiceGetSystemProperty urlHelperDownloadFile

            LINK_LIBRARIES BartonCoreStatic xhDeviceDescriptors
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

if (BCORE_MATTER)
    bds_add_cpp_test(NAME testMatterConfigureSubscriptionSpecs
        # TODO: We shouldn't have to explicitly specify the OpenSSL libraries here. Figure this out later.
        LIBS ${OPENSSL_LINK_LIBRARIES} BartonCoreStatic
        INCLUDES ${OPENSSL_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/deviceDrivers ${CMAKE_BINARY_DIR}/matter-install/include/matter ${PRIVATE_API_INCLUDES} ${XTRA_INCLUDES}
        SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/deviceDrivers/matter/test/MatterConfigureSubscriptionSpecsTest.cpp
    )

    bds_add_cpp_test(NAME testMatterSubscribeInteraction
        # TODO: For some reason, this target requires these libraries to be linked in a different order from the others.
        # Changing the link order within the bds_add_cpp_test function itself just caused the other targets to fail
        # to link. I don't know why, but for now, we're just explicitly specifying the link order here for this one.
        LIBS BartonCoreStatic gtest gmock gmock_main
        INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/deviceDrivers ${CMAKE_BINARY_DIR}/matter-install/include/matter ${PRIVATE_API_INCLUDES} ${XTRA_INCLUDES}
        SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/deviceDrivers/matter/test/MatterSubscribeInteractionTest.cpp
    )

    add_subdirectory("src/subsystems/matter/test")
endif()

# API tests
bds_add_glib_test(
    NAME barton-core-resource-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/barton-core-resource-test.c
    GLOG_DOMAIN BCoreResourceTest
)

bds_add_glib_test(
    NAME barton-core-metadata-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/barton-core-metadata-test.c
    GLOG_DOMAIN BCoreMetadataTest
)

bds_add_glib_test(
    NAME barton-core-endpoint-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/barton-core-endpoint-test.c
    GLOG_DOMAIN BCoreEndpointTest
)

bds_add_glib_test(
    NAME barton-core-device-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/barton-core-device-test.c
    GLOG_DOMAIN BCoreDeviceTest
)

bds_add_glib_test(
    NAME barton-core-discovery-filter-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/barton-core-discovery-filter-test.c
    GLOG_DOMAIN BCoreDiscoveryFilterTest
)

bds_add_glib_test(
    NAME barton-core-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/barton-core-event-test.c
    GLOG_DOMAIN BCoreEventTest
)

bds_add_glib_test(
    NAME barton-core-status-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/barton-core-status-test.c
    GLOG_DOMAIN BCoreStatusTest
)

bds_add_glib_test(
    NAME barton-core-device-found-details-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/barton-core-device-found-details-test.c
    GLOG_DOMAIN BCoreDeviceFoundDetailsTest
)

bds_add_glib_test(
    NAME barton-core-discovery-started-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/barton-core-discovery-started-event-test.c
    GLOG_DOMAIN BCoreDiscoveryStartedEventTest
)

bds_add_glib_test(
    NAME barton-core-recovery-started-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/barton-core-recovery-started-event-test.c
    GLOG_DOMAIN BCoreRecoveryStartedEventTest
)

bds_add_glib_test(
    NAME barton-core-discovery-session-info-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/barton-core-discovery-session-info-event-test.c
    GLOG_DOMAIN BCoreDiscoverySessionInfoEventTest
)

bds_add_glib_test(
    NAME barton-core-device-discovery-failed-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/barton-core-device-discovery-failed-event-test.c
    GLOG_DOMAIN BCoreDeviceDiscoveryFailedEventTest
)

bds_add_glib_test(
    NAME barton-core-device-discovered-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/barton-core-device-discovered-event-test.c
    GLOG_DOMAIN BCoreDeviceDiscoverEventTest
)

bds_add_glib_test(
    NAME barton-core-device-rejected-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/barton-core-device-rejected-event-test.c
    GLOG_DOMAIN BCoreDeviceRejectedEventTest
)

bds_add_glib_test(
    NAME barton-core-device-configuration-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/barton-core-device-configuration-event-test.c
    GLOG_DOMAIN BCoreDeviceConfigurationEventTest
)

bds_add_glib_test(
    NAME barton-core-device-added-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/barton-core-device-added-event-test.c
    GLOG_DOMAIN BCoreDeviceAddedEventTest
)

bds_add_glib_test(
    NAME barton-core-device-recovered-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/barton-core-device-recovered-event-test.c
    GLOG_DOMAIN BCoreDeviceRecoveredEventTest
)

bds_add_glib_test(
    NAME barton-core-device-discovery-completed-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/barton-core-device-discovery-completed-event-test.c
    GLOG_DOMAIN BCoreDeviceDiscoveryCompletedEventTest
)

bds_add_glib_test(
    NAME barton-core-discovery-stopped-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/barton-core-discovery-stopped-event-test.c
    GLOG_DOMAIN BCoreDiscoveryStoppedEventTest
)

bds_add_glib_test(
    NAME barton-core-metadata-updated-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/barton-core-metadata-updated-event-test.c
    GLOG_DOMAIN BCoreMetadataUpdatedEventTest
)

bds_add_glib_test(
    NAME barton-core-recovery-stopped-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/barton-core-recovery-stopped-event-test.c
    GLOG_DOMAIN BCoreRecoveryStoppedEventTest
)

bds_add_glib_test(
    NAME barton-core-resource-updated-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/barton-core-resource-updated-event-test.c
    GLOG_DOMAIN BCoreResourceUpdatedEventTest
)

bds_add_glib_test(
    NAME barton-core-endpoint-removed-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/barton-core-endpoint-removed-event-test.c
    GLOG_DOMAIN BCoreEndpointRemovedEventTest
)

bds_add_glib_test(
    NAME barton-core-endpoint-added-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/barton-core-endpoint-added-event-test.c
    GLOG_DOMAIN BCoreEndpointAddedEventTest
)

bds_add_glib_test(
    NAME barton-core-device-removed-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/barton-core-device-removed-event-test.c
    GLOG_DOMAIN BCoreDeviceRemovedEventTest
)

bds_add_glib_test(
    NAME barton-core-zigbee-channel-changed-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/barton-core-zigbee-channel-changed-event-test.c
    GLOG_DOMAIN BCoreZigbeeChannelChangedEventTest
)

bds_add_glib_test(
    NAME barton-core-storage-changed-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/barton-core-storage-changed-event-test.c
    GLOG_DOMAIN BCoreStorageChangedEventTest
)

bds_add_glib_test(
    NAME barton-core-zigbee-interference-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/barton-core-zigbee-interference-event-test.c
    GLOG_DOMAIN BCoreZigbeeInterferenceEventTest
)

bds_add_glib_test(
    NAME barton-core-zigbee-pan-id-attack-changed-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/barton-core-zigbee-pan-id-attack-changed-event-test.c
    GLOG_DOMAIN BCoreZigbeePanIdAttackChangedEventTest
)

bds_add_glib_test(
    NAME barton-core-device-database-failure-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/barton-core-device-database-failure-event-test.c
    GLOG_DOMAIN BCoreDeviceDatabaseFailureEventTest
)

bds_add_glib_test(
    NAME barton-core-zigbee-remote-cli-response-received-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/events/barton-core-zigbee-remote-cli-response-received-event-test.c
    INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/src ${PRIVATE_API_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR}/src
    GLOG_DOMAIN BCoreZigbeeRemoteCliResponseReceivedEventTest
)

bds_add_glib_test(
    NAME barton-core-utils-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/barton-core-utils-test.c
    INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/src ${PRIVATE_API_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR}/src
    GLOG_DOMAIN BCoreUtilsTest
)

bds_add_cmocka_test(
    NAME barton-core-client-test
    TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/barton-core-client-test.c
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
    LINK_LIBRARIES BartonCoreStatic
    INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/src ${PRIVATE_API_INCLUDES}
)
if(TARGET barton-core-client-test)
    target_compile_definitions(barton-core-client-test PRIVATE G_LOG_DOMAIN="BCoreClientTest")
endif()

bds_add_cmocka_test(
    NAME barton-core-property-provider-test
    TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/test/provider/barton-core-property-provider-test.c
    WRAPPED_FUNCTIONS
        deviceServiceGetSystemProperty
        deviceServiceSetSystemProperty
    LINK_LIBRARIES BartonCoreStatic
    INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/src
)
if(TARGET barton-core-property-provider-test)
    target_compile_definitions(barton-core-property-provider-test PRIVATE G_LOG_DOMAIN="BCorePropertyProviderTest")
endif()

bds_add_cmocka_test(
    NAME deviceServiceConfigurationTest
    TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/src/deviceServiceConfigurationTest.c
    WRAPPED_FUNCTIONS
    LINK_LIBRARIES BartonCoreStatic
    INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/src
)

bds_add_cmocka_test(
    NAME deviceCommunicationWatchdogTest
    INCLUDES
    src
    public
    ../api/c/public
    ${PRIVATE_API_INCLUDES}
    TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/src/deviceCommunicationWatchdogTest.c
    WRAPPED_FUNCTIONS createThread b_core_property_provider_get_property_as_bool
    LINK_LIBRARIES BartonCoreStatic
    INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/src
)

bds_configure_glib()
