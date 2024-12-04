#------------------------------ tabstop = 4 ----------------------------------
#
# Copyright (C) 2018 Comcast Corporation
#
# All rights reserved.
#
# This software is protected by copyright laws of the United States
# and of foreign countries. This material may also be protected by
# patent laws of the United States and of foreign countries.
#
# This software is furnished under a license agreement and/or a
# nondisclosure agreement and may only be used or copied in accordance
# with the terms of those agreements.
#
# The mere transfer of this software does not imply any licenses of trade
# secrets, proprietary technology, copyrights, patents, trademarks, or
# any other form of intellectual property whatsoever.
#
# Comcast Corporation retains all ownership rights.
#
#------------------------------ tabstop = 4 ----------------------------------

include(CMakeParseArguments)
include(BDSConfigureGLib)


# Defines a target for unit tests in the standard way
#
# bds_add_cmocka_test(
#     NAME myTest                                              # New target name, also executable name
#     TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/src/test.c # The list of test source files
#     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test       # Working directory when test is run
#     INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/src/subdir          # Any non-public includes in addition to the defaults
#     WRAPPED_FUNCTIONS printf                                 # list of functions that you want to mock out
#     LINK_LIBRARIES myLibrary                                 # list of libraries that you need to link against
#     LINK_LIBRARIES_WHOLE myWholeLibrary                      # list of libraries that you need to link against (with --whole-archive)
# )
function(bds_add_cmocka_test)
    if(BUILD_TESTING)
        set(options NONE)
        set(oneValueArgs NAME WORKING_DIRECTORY)
        set(multiValueArgs TEST_SOURCES TEST_ARGS INCLUDES WRAPPED_FUNCTIONS LINK_LIBRARIES LINK_LIBRARIES_WHOLE)
        cmake_parse_arguments(BDS_ADD_CMOCKA_TEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

        # Check required args
        if (NOT DEFINED BDS_ADD_CMOCKA_TEST_NAME)
            message(FATAL_ERROR "Missing required argument NAME")
        elseif (NOT DEFINED BDS_ADD_CMOCKA_TEST_TEST_SOURCES)
            message(FATAL_ERROR "Missing required argument TEST_SOURCES")
        endif()

        message(STATUS "Adding unit test ${BDS_ADD_CMOCKA_TEST_NAME}")

        # default optional args
        if (NOT DEFINED BDS_ADD_CMOCKA_TEST_WORKING_DIRECTORY)
            set(BDS_ADD_CMOCKA_TEST_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
        endif()

        include(FindPkgConfig)

        pkg_check_modules(GIO REQUIRED glib-2.0>=${GLIB_MIN_VERSION} gio-2.0>=${GLIB_MIN_VERSION} gio-unix-2.0>=${GLIB_MIN_VERSION})
        list(APPEND XTRA_LIBS ${GIO_LINK_LIBRARIES})
        list(APPEND XTRA_INCLUDES ${GIO_INCLUDE_DIRS})

        add_executable(${BDS_ADD_CMOCKA_TEST_NAME}
                       ${BDS_ADD_CMOCKA_TEST_TEST_SOURCES})


        target_include_directories(${BDS_ADD_CMOCKA_TEST_NAME} PRIVATE ${BDS_ADD_CMOCKA_TEST_INCLUDES})

        #loop through and add wrap args
        if (DEFINED BDS_ADD_CMOCKA_TEST_WRAPPED_FUNCTIONS)
            set(BDS_ADD_CMOCKA_TEST_LINK_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl")

            foreach(WRAPPED_FUNCTION ${BDS_ADD_CMOCKA_TEST_WRAPPED_FUNCTIONS})
                set(BDS_ADD_CMOCKA_TEST_LINK_FLAGS "${BDS_ADD_CMOCKA_TEST_LINK_FLAGS},--wrap=${WRAPPED_FUNCTION}")
            endforeach()
        else()
            set(BDS_ADD_CMOCKA_TEST_LINK_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
        endif()

        set_target_properties(${BDS_ADD_CMOCKA_TEST_NAME} PROPERTIES LINK_FLAGS "${BDS_ADD_CMOCKA_TEST_LINK_FLAGS}")

        # add library dependencies for this test-binary
        target_link_libraries(${BDS_ADD_CMOCKA_TEST_NAME}
                                ${BDS_ADD_CMOCKA_TEST_LINK_LIBRARIES}
                                cmocka
                                ${XTRA_LIBS}
                                -Wl,--whole-archive ${BDS_ADD_CMOCKA_TEST_LINK_LIBRARIES_WHOLE} -Wl,--no-whole-archive)

        set_target_properties(${BDS_ADD_CMOCKA_TEST_NAME} PROPERTIES
                              SKIP_BUILD_RPATH false
                              BUILD_RPATH_USE_ORIGIN true
                              BUILD_RPATH ${CMAKE_PREFIX_PATH}/lib)

        add_test(NAME ${BDS_ADD_CMOCKA_TEST_NAME}
                 COMMAND ${BDS_ADD_CMOCKA_TEST_NAME} ${BDS_ADD_CMOCKA_TEST_TEST_ARGS}
                 WORKING_DIRECTORY ${BDS_ADD_CMOCKA_TEST_WORKING_DIRECTORY})
    endif()
endfunction() # SETUP_TARGET_FOR_COVERAGE
