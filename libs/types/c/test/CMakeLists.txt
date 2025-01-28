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
# CMake definition for the fcore 'types library unit tests'
# Should not be utilized directly, but imported from our parent
# file via the 'add_subdirectory()' routine.
#
#--------------------------------------------------------------------------------------

# setup a unit test, but such that it is NOT part of the 'make all' sequence.
# allows us to only build/run unit tests on the platforms we can (meaning
# not during a cross-compile session).  should be build/ran as part of
# the 'make unitTest' command

# setup include paths
include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CROSS_OUTPUT}/include         # 3rd Party
)

bds_add_cmocka_test(
    NAME testStringHashMap
    TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/stringHashTest.c
    LINK_LIBRARIES xhTypes xhJsonHelper
)

bds_add_cmocka_test(
    NAME testTypesLib
    TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bufferTest.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hashTest.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/linkedTest.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/queueTest.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sortedTest.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/stringBufferTest.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/typesTest.c
    TEST_ARGS -a
    LINK_LIBRARIES xhTypes xhUtil
)
