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
# CMake definition for the fcore 'config library unit tests'
# Should not be utilized directly, but imported from our parent
# file via the 'add_subdirectory()' routine.
#
#--------------------------------------------------------------------------------------
include(BCoreAddCMockaTest)

# define source files (using wildcard)
# Don't dynamically link to xhConfig - we need to wrap its undefined getDynamicConfiPath symbol
file(GLOB SOURCES "src/*.c" "../src/*.c")

bcore_add_cmocka_test(
        NAME testConfigLib
        TEST_SOURCES ${SOURCES}
        LINK_LIBRARIES xhJsonHelper $<TARGET_PROPERTY:xhConfig,LINK_LIBRARIES>
        INCLUDES $<TARGET_PROPERTY:xhConfig,INCLUDE_DIRECTORIES>
        TEST_ARGS "-a"
)

bcore_add_cmocka_test(
        NAME testSimpleProtectLib
        TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/simpleProtect/simpleProtectTest.c
        LINK_LIBRARIES xhConfig
)
