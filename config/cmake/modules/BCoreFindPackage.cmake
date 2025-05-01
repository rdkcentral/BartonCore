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

include_guard(GLOBAL)
include(CMakeParseArguments)
include(FindPkgConfig)

function(bds_find_package)
    set(options REQUIRED)
    set(oneValueArgs NAME MIN_VERSION MAX_VERSION)
    set(multiValueArgs SOURCES LIBRARIES INCLUDE_DIRECTORIES)
    cmake_parse_arguments(BCORE_FIND_PACKAGE "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if (BCORE_FIND_PACKAGE_MIN_VERSION)
        set(PKG_CONFIG_VERSION_CHECK ">=${BCORE_FIND_PACKAGE_MIN_VERSION}")

        if (BCORE_FIND_PACKAGE_MAX_VERSION)
            set(FIND_PACKAGE_VERSION_CHECK ${BCORE_FIND_PACKAGE_MIN_VERSION}...${BCORE_FIND_PACKAGE_MAX_VERSION})
        else()
            set(FIND_PACKAGE_VERSION_CHECK ${BCORE_FIND_PACKAGE_MIN_VERSION})
        endif()
    endif()

    if (BCORE_FIND_PACKAGE_REQUIRED)
        set(PACKAGE_REQUIRED "REQUIRED")
    endif()

    pkg_check_modules(BCORE_PACKAGE ${BCORE_FIND_PACKAGE_NAME}${PKG_CONFIG_VERSION_CHECK})

    if (NOT BCORE_PACKAGE_FOUND)
        find_package(${BCORE_FIND_PACKAGE_NAME} ${FIND_PACKAGE_VERSION_CHECK})

        if (NOT ${BCORE_FIND_PACKAGE_NAME}_FOUND)
            # Last ditch, try find_library
            find_library(BCORE_LIBRARY NAMES ${BCORE_FIND_PACKAGE_NAME} ${PACKAGE_REQUIRED})
        endif()
    endif()
endfunction()
