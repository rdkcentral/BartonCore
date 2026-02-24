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

FetchContent_Declare(
    BartonCommon
    GIT_REPOSITORY https://github.com/rdkcentral/BartonCommon
    GIT_TAG cleith/dev/improveBuilding
)

# Use FetchContent_Populate + add_subdirectory with EXCLUDE_FROM_ALL to prevent
# BartonCommon's install() rules from being included in BartonCore's install target.
# This allows BartonCommon targets to be available for linking without installing
# BartonCommon headers/libraries when installing BartonCore.
#
# NOTE: EXCLUDE_FROM_ALL available to FetchContent_Declare in CMake 3.28+.
FetchContent_GetProperties(BartonCommon)
if(NOT bartoncommon_POPULATED)
    FetchContent_Populate(BartonCommon)
    add_subdirectory(${bartoncommon_SOURCE_DIR} ${bartoncommon_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()
