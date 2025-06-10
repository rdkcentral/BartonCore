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

include(BCoreAddCMockaTest)

# define source files (using wildcard)
file(GLOB SOURCES "src/*.c")
#Remove these tests from the parent
list(FILTER SOURCES EXCLUDE REGEX ".*fileTest.c$")
list(FILTER SOURCES EXCLUDE REGEX ".*stringTest.c$")
list(FILTER SOURCES EXCLUDE REGEX ".*pingTest.c$")
list(FILTER SOURCES EXCLUDE REGEX ".*regexTest.c$")
list(FILTER SOURCES EXCLUDE REGEX ".*systemCommandUtilsTest.c$")
list(FILTER SOURCES EXCLUDE REGEX ".*stateMachineTest.c$")

bds_add_cmocka_test(
        NAME testUtilLib
        TEST_SOURCES ${SOURCES}
        LINK_LIBRARIES xhUtil xhTypes
        TEST_ARGS "-a"
)

#bds_add_cmocka_test(NAME testPing TEST_SOURCES src/pingTest.c LINK_LIBRARIES xhUtil)

bds_add_cmocka_test(
    NAME testStateMachine
    TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/stateMachineTest.c
    LINK_LIBRARIES xhUtil
)
