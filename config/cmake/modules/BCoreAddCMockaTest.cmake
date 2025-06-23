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


# Defines a target for unit tests in the standard way
#
# bcore_add_cmocka_test(
#     NAME myTest                                              # New target name, also executable name
#     TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/src/test.c # The list of test source files
#     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test       # Working directory when test is run
#     INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/src/subdir          # Any non-public includes in addition to the defaults
#     WRAPPED_FUNCTIONS printf                                 # list of functions that you want to mock out
#     LINK_LIBRARIES myLibrary                                 # list of libraries that you need to link against
#     LINK_LIBRARIES_WHOLE myWholeLibrary                      # list of libraries that you need to link against (with --whole-archive)
# )
function(bcore_add_cmocka_test)
    if(BUILD_TESTING)
        set(options NONE)
        set(oneValueArgs NAME WORKING_DIRECTORY)
        set(multiValueArgs TEST_SOURCES TEST_ARGS INCLUDES WRAPPED_FUNCTIONS LINK_LIBRARIES LINK_LIBRARIES_WHOLE)
        cmake_parse_arguments(BCORE_ADD_CMOCKA_TEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

        # Check required args
        if (NOT DEFINED BCORE_ADD_CMOCKA_TEST_NAME)
            message(FATAL_ERROR "Missing required argument NAME")
        elseif (NOT DEFINED BCORE_ADD_CMOCKA_TEST_TEST_SOURCES)
            message(FATAL_ERROR "Missing required argument TEST_SOURCES")
        endif()

        message(STATUS "Adding unit test ${BCORE_ADD_CMOCKA_TEST_NAME}")

        # default optional args
        if (NOT DEFINED BCORE_ADD_CMOCKA_TEST_WORKING_DIRECTORY)
            set(BCORE_ADD_CMOCKA_TEST_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
        endif()

        include(FindPkgConfig)

        pkg_check_modules(GIO REQUIRED glib-2.0>=${GLIB_MIN_VERSION} gio-2.0>=${GLIB_MIN_VERSION} gio-unix-2.0>=${GLIB_MIN_VERSION})
        list(APPEND XTRA_LIBS ${GIO_LINK_LIBRARIES})
        list(APPEND XTRA_INCLUDES ${GIO_INCLUDE_DIRS})

        add_executable(${BCORE_ADD_CMOCKA_TEST_NAME}
                       ${BCORE_ADD_CMOCKA_TEST_TEST_SOURCES})


        target_include_directories(${BCORE_ADD_CMOCKA_TEST_NAME} PRIVATE ${BCORE_ADD_CMOCKA_TEST_INCLUDES})

        #loop through and add wrap args
        if (DEFINED BCORE_ADD_CMOCKA_TEST_WRAPPED_FUNCTIONS)
            set(BCORE_ADD_CMOCKA_TEST_LINK_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl")

            foreach(WRAPPED_FUNCTION ${BCORE_ADD_CMOCKA_TEST_WRAPPED_FUNCTIONS})
                set(BCORE_ADD_CMOCKA_TEST_LINK_FLAGS "${BCORE_ADD_CMOCKA_TEST_LINK_FLAGS},--wrap=${WRAPPED_FUNCTION}")
            endforeach()
        else()
            set(BCORE_ADD_CMOCKA_TEST_LINK_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
        endif()

        set_target_properties(${BCORE_ADD_CMOCKA_TEST_NAME} PROPERTIES LINK_FLAGS "${BCORE_ADD_CMOCKA_TEST_LINK_FLAGS}")

        # add library dependencies for this test-binary
        target_link_libraries(${BCORE_ADD_CMOCKA_TEST_NAME}
                                ${BCORE_ADD_CMOCKA_TEST_LINK_LIBRARIES}
                                cmocka
                                ${XTRA_LIBS}
                                -Wl,--whole-archive ${BCORE_ADD_CMOCKA_TEST_LINK_LIBRARIES_WHOLE} -Wl,--no-whole-archive)

        set_target_properties(${BCORE_ADD_CMOCKA_TEST_NAME} PROPERTIES
                              SKIP_BUILD_RPATH false
                              BUILD_RPATH_USE_ORIGIN true
                              BUILD_RPATH ${CMAKE_PREFIX_PATH}/lib)

        add_test(NAME ${BCORE_ADD_CMOCKA_TEST_NAME}
                 COMMAND ${BCORE_ADD_CMOCKA_TEST_NAME} ${BCORE_ADD_CMOCKA_TEST_TEST_ARGS}
                 WORKING_DIRECTORY ${BCORE_ADD_CMOCKA_TEST_WORKING_DIRECTORY})
    endif()

    bcore_configure_glib()
endfunction() # SETUP_TARGET_FOR_COVERAGE
