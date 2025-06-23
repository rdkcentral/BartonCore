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

include_guard(GLOBAL)
include(BCoreConfigureGLib)

function(bcore_add_glib_test)
    if(BUILD_TESTING)
        set(options)
        set(oneValueArgs NAME GLOG_DOMAIN)
        set(multiValueArgs SOURCES LIBRARIES INCLUDE_DIRECTORIES)
        cmake_parse_arguments(BCORE_ADD_GLIB_TEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

        add_executable(${BCORE_ADD_GLIB_TEST_NAME} ${BCORE_ADD_GLIB_TEST_SOURCES})
        target_link_libraries(${BCORE_ADD_GLIB_TEST_NAME} PRIVATE BartonCoreStatic ${BCORE_ADD_GLIB_TEST_LIBRARIES})
        target_compile_definitions(${BCORE_ADD_GLIB_TEST_NAME} PRIVATE G_LOG_DOMAIN="${BCORE_ADD_GLIB_TEST_GLOG_DOMAIN}")
        target_include_directories(${BCORE_ADD_GLIB_TEST_NAME} PRIVATE ${BCORE_ADD_GLIB_TEST_INCLUDE_DIRECTORIES})

        message(STATUS "Adding unit test ${BCORE_ADD_GLIB_TEST_NAME}")

        add_test(NAME ${BCORE_ADD_GLIB_TEST_NAME} COMMAND ${BCORE_ADD_GLIB_TEST_NAME})
        set_property(TEST ${BCORE_ADD_GLIB_TEST_NAME} PROPERTY ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_PREFIX_PATH}/lib:${CMAKE_INSTALL_PREFIX}/lib:$ENV{LD_LIBRARY_PATH}")
    endif()
endfunction()
