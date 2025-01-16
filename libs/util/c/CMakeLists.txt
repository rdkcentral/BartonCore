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
# CMake definition for the fcore 'util library'
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
file(GLOB SOURCES "src/*.[ch]")
if (CONFIG_OS_DARWIN)
    # remove ping.c from OSX
    list(REMOVE_ITEM SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/ping.c)
endif()

add_library(xhUtil STATIC ${SOURCES})

target_link_libraries(xhUtil xhLog xhTypes uuid mbedcrypto)

# export our 'public' folder so others can include our headers
target_include_directories(xhUtil PUBLIC public)

add_compile_definitions(PRIVATE RESOURCES_DIR="${CMAKE_CURRENT_LIST_DIR}/test/etc")

bds_configure_glib()

bds_add_cmocka_test(
        NAME testFileUtils
        TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/src/fileTest.c
        WRAPPED_FUNCTIONS fsync fflush fwrite fread feof fopen ferror
        LINK_LIBRARIES xhUtil xhTypes
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test
)

bds_add_cmocka_test(
        NAME testStringUtils
        TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/src/stringTest.c
        LINK_LIBRARIES xhUtil xhTypes
)

bds_add_cmocka_test(
        NAME testRegexUtils
        TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/src/regexTest.c
        LINK_LIBRARIES xhUtil xhTypes
)

bds_add_cmocka_test(
        NAME testSystemCommandUtils
        TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/src/systemCommandUtilsTest.c
        LINK_LIBRARIES xhUtil
)

# add our 'unit test' to be part of the "make unitTest"
add_subdirectory(test)
