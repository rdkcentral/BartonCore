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

cmake_minimum_required(VERSION 3.18)
#TODO: project version (snag from git tag)
project(brtn-ds)

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

# Before building our stuff, build matter if we need to
if (BDS_MATTER)
    # Only search default locations. Mostly this is for clients who build matter out of band
    # already. The script will check our usual matter build location.
    find_library(MATTER NAMES ${BDS_MATTER_LIB} NO_CMAKE_FIND_ROOT_PATH)

    if (${MATTER} STREQUAL "MATTER-NOTFOUND")
        # Forward SSP args to building matter
        if (BDS_BUILD_WITH_SSP)
            set(MATTER_C_FLAGS "${MATTER_C_FLAGS} -fstack-protector-strong")
            set(MATTER_CXX_FLAGS "${MATTER_CXX_FLAGS} -fstack-protector-strong")
        endif()

        # Forward ASAN args to building matter
        if (BDS_BUILD_WITH_ASAN)
            set(MATTER_C_FLAGS "${MATTER_C_FLAGS} -fsanitize=address")
            set(MATTER_CXX_FLAGS "${MATTER_CXX_FLAGS} -fsanitize=address")

            set(MATTER_LINK_FLAGS "${MATTER_LINK_FLAGS} -fsanitize=address")
        endif()

        set(MATTER_BUILD_COMMAND "./build-matter.sh")

        execute_process(
            COMMAND ${CMAKE_COMMAND} -E env CFLAGS=${MATTER_C_FLAGS} CXXFLAGS=${MATTER_CXX_FLAGS} LDFLAGS=${MATTER_LINK_FLAGS} ${MATTER_BUILD_COMMAND}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMAND_ECHO STDOUT
            COMMAND_ERROR_IS_FATAL ANY
        )
    else()
        message(STATUS "Matter found at ${MATTER}")
    endif()
endif()

# Setup optional compiler/linker global flags

if (BDS_BUILD_WITH_SSP)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector-strong")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector-strong")
endif()

if (BDS_BUILD_WITH_ASAN)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")

    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
endif()

if (NOT BDS_PROVIDE_LIBS)
    add_subdirectory(libs)
endif()

add_subdirectory(core)

if (BDS_BUILD_REFERENCE)
    add_subdirectory(reference)
endif()
