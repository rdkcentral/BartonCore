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

include(FetchContent)

set(ZHAL_GIT_REPOSITORY https://github.com/tleacmcsa/zhal)
set(ZHAL_GIT_TAG b130c1baa8bf844b1731b5a3c74629674384add3)

# BartonCore only consumes the zhal-client library; skip zhal's own test suite.
set(ZHAL_BUILD_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    zhal
    GIT_REPOSITORY ${ZHAL_GIT_REPOSITORY}
    GIT_TAG ${ZHAL_GIT_TAG}
)

# Use CMAKE_SKIP_INSTALL_RULES to prevent zhal's install() rules from being
# included in BartonCore's install target, while still building its targets.
FetchContent_GetProperties(zhal)
if(NOT zhal_POPULATED)
    FetchContent_Populate(zhal)
    set(BARTONCORE_SAVED_CMAKE_SKIP_INSTALL_RULES ${CMAKE_SKIP_INSTALL_RULES})
    set(CMAKE_SKIP_INSTALL_RULES ON)
    add_subdirectory(${zhal_SOURCE_DIR} ${zhal_BINARY_DIR})
    set(CMAKE_SKIP_INSTALL_RULES ${BARTONCORE_SAVED_CMAKE_SKIP_INSTALL_RULES})
    # CMAKE_SKIP_INSTALL_RULES prevents cmake_install.cmake from being generated,
    # but the parent's cmake_install.cmake still tries to include it. Create an empty one.
    file(WRITE ${zhal_BINARY_DIR}/cmake_install.cmake "")
endif()
