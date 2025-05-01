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
project(brtn-reference)

include(BDSConfigureGLib)

file(GLOB_RECURSE BRTN_DS_REFERENCE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)

add_executable(brtn-ds-reference ${BRTN_DS_REFERENCE_SOURCES})

# add library dependencies for this binary
target_link_options(brtn-ds-reference PRIVATE -L ${CMAKE_BINARY_DIR}/matter-install/lib)
target_link_libraries(brtn-ds-reference bDeviceService linenoise)

bds_configure_glib()
