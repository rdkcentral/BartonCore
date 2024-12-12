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
# CMake definition for the fcore 'zhal library'
# Should not be utilized directly, but imported from our parent
# file via the 'add_subdirectory()' routine.
#
#--------------------------------------------------------------------------------------


# setup include paths
include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/public
        ${ZILKER_INCLUDE_DIR}
        ${CROSS_OUTPUT}/include     # 3rd Party
)

# define source files (using wildcard)
file(GLOB SOURCES "src/*.c")

add_library(zhal STATIC ${SOURCES})
target_link_libraries(zhal xhLog xhTypes xhUtil xhConcurrent cjson m ${CONFIG_LDFLAG})

# export our 'public' folder so others can include our headers
target_include_directories(zhal PUBLIC public)

bds_add_cmocka_test(
        NAME testZhalImpl
        TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/src/zhalImplUnitTest.c
        WRAPPED_FUNCTIONS connect send recv zhalNetworkTerm
        LINK_LIBRARIES zhal
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test
        INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# add our test to be part of the "make manualTest"
add_subdirectory(test)

bds_configure_glib()
