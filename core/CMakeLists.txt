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
    include(BCoreMatterHelper)

    # Check for jsoncpp here rather than in DependencyVersions.cmake because the
    # Matter build process installs jsoncpp, and might not be available during
    # initial dependency resolution
    set(JSONCPP_MIN_VERSION 1.9.5)

    file(GLOB matterSubSrc "src/subsystems/matter/*.cpp")

    file(GLOB matterDriversSrc "deviceDrivers/matter/*.cpp" "deviceDrivers/matter/clusters/*.cpp" "deviceDrivers/matter/subscriptions/*.cpp")

    add_compile_definitions(CHIP_HAVE_CONFIG_H=1)

    bcore_matter_helper_provider_get_implementations(
        OUTPUT MATTER_PROVIDER_IMPLEMENTATIONS
    )

    bcore_matter_helper_delegate_get_implementations(
        OUTPUT MATTER_DELEGATE_IMPLEMENTATIONS
    )

    list(APPEND SOURCES
        ${matterSubSrc}
        ${MATTER_PROVIDER_IMPLEMENTATIONS}
        ${MATTER_DELEGATE_IMPLEMENTATIONS}
        ${matterDriversSrc}
        ${matterSrc}
        "src/subsystems/matter/providers/BartonDeviceInstanceInfoProvider.cpp" # This provider is always required
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

    bcore_find_include(
        MATTER_INCLUDE_DIR
        access/AccessRestrictionProvider.h
        matter
        "Matter include directory"
        XTRA_INCLUDES
    )

    bcore_find_package(NAME jsoncpp MIN_VERSION ${JSONCPP_MIN_VERSION} REQUIRED)

    bcore_matter_helper_provider_get_header_paths(
        OUTPUT MATTER_PROVIDER_HEADER_PATHS
    )

    bcore_matter_helper_delegate_get_header_paths(
        OUTPUT MATTER_DELEGATE_HEADER_PATHS
    )

    list(APPEND XTRA_INCLUDES
        ${OPENSSL_INCLUDE_DIRS}
        ${MATTER_PROVIDER_HEADER_PATHS}
        ${MATTER_DELEGATE_HEADER_PATHS})

    list(APPEND XTRA_LIBS ${BCORE_MATTER_LIB} ${OPENSSL_LINK_LIBRARIES} jsoncpp)
    link_directories(${CMAKE_BINARY_DIR}/matter-install/lib)
endif()

if (BCORE_THREAD)
    pkg_check_modules(DBUS dbus-1 REQUIRED)

    file(GLOB_RECURSE thrSrc "src/subsystems/thread/*.cpp" "src/subsystems/thread/*.c")
    list(APPEND SOURCES ${thrSrc})

    bcore_find_include(
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

bcore_find_include(
    LIBXML2_INCLUDE_DIR
    libxml/parser.h
    libxml2
    "Libxml2 include directory"
    XTRA_INCLUDES
)

# add library dependencies for this binary
set(bCoreLinkLibraries bCoreConfig xhLog xhTypes xhTime xhUtil
        xhConcurrent xhConfig xhUrlHelper xhXmlHelper xhJsonHelper
        xhDeviceHelper xhDeviceDescriptors xhXmlHelper
        xhCrypto ${XTRA_LIBS} cjson uuid curl xml2 z m ${BCORE_LINK_LIBRARIES})

set(BARTON_PRIVATE_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/src
                            ${CMAKE_CURRENT_SOURCE_DIR}/deviceDrivers
                            ${XTRA_INCLUDES}
                            ${CURRENT_SOURCE_GENERATED_DIR}/src
                            ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/src
                            ${PRIVATE_API_INCLUDES})

set(BARTON_PUBLIC_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../api/c/public)

#FIXME: Reduce copy-paste.
# Object library type compiles once, then can re-use those object files for
# statice/shared libraries.
add_library(bCoreObject OBJECT ${SOURCES} ${B_CORE_API_SOURCES})
target_link_libraries(bCoreObject ${bCoreLinkLibraries})
target_include_directories(bCoreObject PRIVATE
                            ${BARTON_PRIVATE_INCLUDES}
                            PUBLIC
                            ${BARTON_PUBLIC_INCLUDES}
                            )

add_library(BartonCoreStatic STATIC $<TARGET_OBJECTS:bCoreObject>)
target_link_libraries(BartonCoreStatic ${bCoreLinkLibraries})
target_include_directories(BartonCoreStatic PRIVATE
                            ${BARTON_PRIVATE_INCLUDES}
                            PUBLIC
                            ${BARTON_PUBLIC_INCLUDES}
                            )

add_library(BartonCore SHARED $<TARGET_OBJECTS:bCoreObject>)
target_link_libraries(BartonCore ${bCoreLinkLibraries})
target_include_directories(BartonCore PRIVATE
                            ${BARTON_PRIVATE_INCLUDES}
                            PUBLIC
                            ${BARTON_PUBLIC_INCLUDES}
                            )

set_target_properties(BartonCore PROPERTIES VERSION ${BARTON_CORE_SO_VERSION} SOVERSION ${BARTON_CORE_MAJOR_VERSION})
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
    set(NAMESPACE_VERSION ${BARTON_CORE_API_VERSION})
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

# temporary until zhal is relocated to its own repo
add_subdirectory(src/zhal/c)

add_subdirectory(${PROJECT_SOURCE_DIR}/api/c/test ${PROJECT_BINARY_DIR}/api/c/test)
add_subdirectory(test)

bcore_configure_glib()
