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

include(FetchContent)

set(BARTON_COMMON_GIT_REPOSITORY https://github.com/rdkcentral/BartonCommon)
set(BARTON_COMMON_GIT_TAG 0.1.3)

FetchContent_Declare(
    BartonCommon
    GIT_REPOSITORY ${BARTON_COMMON_GIT_REPOSITORY}
    GIT_TAG ${BARTON_COMMON_GIT_TAG}
)

# Use CMAKE_SKIP_INSTALL_RULES to prevent BartonCommon's install() rules from being
# included in BartonCore's install target, while still building all targets (including tests).
FetchContent_GetProperties(BartonCommon)
if(NOT bartoncommon_POPULATED)
    FetchContent_Populate(BartonCommon)
    set(BARTONCORE_SAVED_CMAKE_SKIP_INSTALL_RULES ${CMAKE_SKIP_INSTALL_RULES})
    set(CMAKE_SKIP_INSTALL_RULES ON)
    add_subdirectory(${bartoncommon_SOURCE_DIR} ${bartoncommon_BINARY_DIR})
    set(CMAKE_SKIP_INSTALL_RULES ${BARTONCORE_SAVED_CMAKE_SKIP_INSTALL_RULES})
    # CMAKE_SKIP_INSTALL_RULES prevents cmake_install.cmake from being generated,
    # but the parent's cmake_install.cmake still tries to include it. Create an empty one.
    file(WRITE ${bartoncommon_BINARY_DIR}/cmake_install.cmake "")
endif()
