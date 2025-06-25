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
project(barton-core-reference)

include(BCoreConfigureGLib)

file(GLOB_RECURSE B_CORE_REFERENCE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)

add_executable(barton-core-reference ${B_CORE_REFERENCE_SOURCES})

# add library dependencies for this binary
target_link_options(barton-core-reference PRIVATE -L ${CMAKE_BINARY_DIR}/matter-install/lib)
target_link_libraries(barton-core-reference
                        -Wl,--whole-archive BartonCoreStatic -Wl,--no-whole-archive
                        linenoise)

bcore_configure_glib()
