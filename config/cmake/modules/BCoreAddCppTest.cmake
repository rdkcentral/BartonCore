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

include(CMakeParseArguments)
include(BCoreConfigureGLib)

# Add a C++ unit test executable target.
# Arguments:
# - NAME the executable name (e.g., testMyTest).
# - SOURCES The sources with which to generate the test executable.
# - LIBS Libraries to link the test executable against
# - INCLUDES Header files to be included
function(bcore_add_cpp_test)
    if(BUILD_TESTING)
        set(singleValueArgs NAME)
        set(multiValueArgs SOURCES LIBS INCLUDES)
        cmake_parse_arguments(BCORE_ADD_CPP_TEST "" "${singleValueArgs}" "${multiValueArgs}" ${ARGN})

        add_executable(${BCORE_ADD_CPP_TEST_NAME} ${BCORE_ADD_CPP_TEST_SOURCES})
        target_link_libraries(${BCORE_ADD_CPP_TEST_NAME} gtest gmock gmock_main ${BCORE_ADD_CPP_TEST_LIBS})
        target_include_directories(${BCORE_ADD_CPP_TEST_NAME} PRIVATE ${BCORE_ADD_CPP_TEST_INCLUDES})
        set_target_properties(${BCORE_ADD_CPP_TEST_NAME} PROPERTIES
                                                      SKIP_BUILD_RPATH false
                                                      BUILD_RPATH_USE_ORIGIN true
                                                      BUILD_RPATH ${CMAKE_PREFIX_PATH}/lib)
        include(GoogleTest)
        gtest_discover_tests(${BCORE_ADD_CPP_TEST_NAME})
    endif()
endfunction()
