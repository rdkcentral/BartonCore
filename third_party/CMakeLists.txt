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
# Created by Christian Leithner on 11/12/2025.
#

if (BCORE_BUILD_THIRD_PARTY_BARTON_COMMON)
    add_subdirectory(barton_common)
else()
    # When not building BartonCommon as a third-party dependency, find it via
    # CMake package config. This allows external repositories to build
    # BartonCommon separately and have BartonCore discover it via CMAKE_PREFIX_PATH.
    find_package(BartonCommon REQUIRED)
endif()

# TODO: Integrate matter more strongly with CMake.
if (BCORE_MATTER AND NOT BCORE_MATTER_SKIP_SDK)
    # Only search default locations. Mostly this is for clients who build matter out of band
    # already. The script will check our usual matter build location.
    find_library(MATTER NAMES ${BCORE_MATTER_LIB})

    if (${MATTER} STREQUAL "MATTER-NOTFOUND")
        # Forward SSP args to building matter
        if (BCORE_BUILD_WITH_SSP)
            set(MATTER_SSP_ARG "-s")
        endif()

        # Forward ASAN args to building matter
        if (BCORE_BUILD_WITH_ASAN)
            set(MATTER_ASAN_ARG "-a")
        endif()

        # TODO: This config directory should be the same as what is available at runtime via providers. Perhaps this should be a public build option
        set(MATTER_BUILD_COMMAND ./build-matter.sh -c $ENV{HOME}/.brtn-ds/matter ${MATTER_SSP_ARG} ${MATTER_ASAN_ARG})

        execute_process(
            COMMAND ${MATTER_BUILD_COMMAND}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMAND_ECHO STDOUT
            # COMMAND_ERROR_IS_FATAL ANY -- Re-enable with cmake 3.19+
        )
    else()
        message(STATUS "Matter found at ${MATTER}")
    endif()
endif()
