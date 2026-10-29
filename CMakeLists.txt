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

cmake_minimum_required(VERSION 3.16.5)
#TODO: project version (snag from git tag)
project(barton-core)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

execute_process(
                COMMAND git describe --tags --match [0-9]*.[0-9]*.[0-9]*
                WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
                OUTPUT_VARIABLE BARTON_CORE_VERSION
                OUTPUT_STRIP_TRAILING_WHITESPACE
                )

message(STATUS "Barton Version: ${BARTON_CORE_VERSION}")
string(REGEX MATCH "^[0-9]+\\.[0-9]+\\.[0-9]+" BARTON_CORE_SO_VERSION ${BARTON_CORE_VERSION})
message(STATUS "Barton SO Version: ${BARTON_CORE_SO_VERSION}")
string(REGEX MATCHALL "[0-9]+" BARTON_CORE_VERSION_LIST ${BARTON_CORE_SO_VERSION})

list(GET BARTON_CORE_VERSION_LIST 0 BARTON_CORE_MAJOR_VERSION)
list(GET BARTON_CORE_VERSION_LIST 1 BARTON_CORE_MINOR_VERSION)
list(GET BARTON_CORE_VERSION_LIST 2 BARTON_CORE_PATCH_VERSION)

# This is what you may also call the platform version. The focue is API/ABI compatibility.
# As such, the minor version rarely changes. This is analog to GLib/GTK/GIR namespace
# version schemeing where a semver of 4.24.3 has api version 4.0
set(BARTON_CORE_API_VERSION "${BARTON_CORE_MAJOR_VERSION}.0")
message(STATUS "Barton API Version: ${BARTON_CORE_API_VERSION}")

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)

add_compile_definitions(_GNU_SOURCE)

if ("${CMAKE_PREFIX_PATH}" STREQUAL "")
    # This is a hacky way to deal with dependencies that don't
    # publish a pkg-config or cmake package file (otbr, matter).
    # This assumes that one is compiling for development, where
    # we also own the build/installation of those dependencies.
    set(CMAKE_PREFIX_PATH ${CMAKE_INSTALL_PREFIX})
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/config/cmake/modules")
include("${PROJECT_SOURCE_DIR}/config/cmake/options.cmake")
include("${PROJECT_SOURCE_DIR}/config/cmake/DependencyVersions.cmake")

message(STATUS "Targetting system - ${CMAKE_SYSTEM_NAME}")
message(STATUS "Using C toolchain - ${CMAKE_C_COMPILER}")
message(STATUS "Using C++ toolchain - ${CMAKE_CXX_COMPILER}")

# Add custom target for running tests. This will allow us to easily invoke
# all tests, as well as wrap them with code coverage if enabled
add_custom_target(runTests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

if (BCORE_TEST_COVERAGE)
    include(CodeCoverage)
    APPEND_COVERAGE_COMPILER_FLAGS()
    # Exclude system includes, and tests
    set(COVERAGE_EXCLUDES "'/usr/*'" "'/*/test/*'" "'/*/matter-install/*'")
    # We are using LCOV 2+ which has a lot of validation checks compared to 1+.
    # For some reason, GTest causes it to get confused so we'll just treat it
    # as a warning.
    set(COVERAGE_LCOV_ARGS "--ignore-errors" "mismatch")
    wrap_target_with_coverage(NAME runTestsWithCoverage WRAPPED_TARGET runTests)
endif()

add_subdirectory(third_party)

# Setup optional compiler/linker global flags

if (BCORE_BUILD_WITH_SSP)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector-strong")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector-strong")
endif()

if (BCORE_BUILD_WITH_TSAN AND BCORE_BUILD_WITH_ASAN)
    message(FATAL_ERROR "BCORE_BUILD_WITH_TSAN cannot be combined with BCORE_BUILD_WITH_ASAN")
endif()

set(BCORE_SANITIZERS "")
if (BCORE_BUILD_WITH_ASAN)
    list(APPEND BCORE_SANITIZERS address)
endif()
if (BCORE_BUILD_WITH_UBSAN)
    list(APPEND BCORE_SANITIZERS undefined)
endif()
if (BCORE_BUILD_WITH_TSAN)
    list(APPEND BCORE_SANITIZERS thread)
endif()

if (BCORE_SANITIZERS)
    list(JOIN BCORE_SANITIZERS "," _bcoreSanList)
    set(_bcoreSanFlags "-fsanitize=${_bcoreSanList} -fno-omit-frame-pointer")
    if (BCORE_BUILD_WITH_UBSAN)
        string(APPEND _bcoreSanFlags " -fno-sanitize-recover=undefined")
        # C executables (reference app, g-ir-scanner dump) link libBartonCore with
        # the C driver, which omits clang's C++ UBSan runtime required by vptr.
        if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            string(APPEND _bcoreSanFlags " -fno-sanitize=vptr")
        endif()
    endif()
    foreach (_var CMAKE_C_FLAGS CMAKE_CXX_FLAGS CMAKE_SHARED_LINKER_FLAGS CMAKE_EXE_LINKER_FLAGS)
        set(${_var} "${${_var}} ${_bcoreSanFlags}")
    endforeach()
endif()

include(BCoreWarnings)
include(BCoreCppcheck)

add_subdirectory(libs)

add_subdirectory(core)

if (BCORE_BUILD_REFERENCE)
    add_subdirectory(reference)
endif()
