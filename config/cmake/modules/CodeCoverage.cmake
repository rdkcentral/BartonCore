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

# Copyright (c) 2012 - 2017, Lars Bilke
# All rights reserved.
# Licensed under the BSD-3 License

#
# CHANGES:
#
# 2012-01-31, Lars Bilke
# - Enable Code Coverage
#
# 2013-09-17, Joakim Söderberg
# - Added support for Clang.
# - Some additional usage instructions.
#
# 2016-02-03, Lars Bilke
# - Refactored functions to use named parameters
#
# 2017-06-02, Lars Bilke
# - Merged with modified version from github.com/ufz/ogs
#
# 2024-12-17, Christian Leithner
# - Added support for lcov arguments via setting COVERAGE_LCOV_ARGS
#
# USAGE:
#
# 1. Copy this file into your cmake modules path.
#
# 2. Add the following line to your CMakeLists.txt:
#      include(CodeCoverage)
#
# 3. Append necessary compiler flags:
#      APPEND_COVERAGE_COMPILER_FLAGS()
#
# 4. If you need to exclude additional directories from the report, specify them
#    using the COVERAGE_EXCLUDES variable before calling SETUP_TARGET_FOR_COVERAGE.
#    Example:
#      set(COVERAGE_EXCLUDES 'dir1/*' 'dir2/*')
#
# 5. Use the functions described below to create a custom make target which
#    runs your test executable and produces a code coverage report.
#
# 6. Build a Debug build:
#      cmake -DCMAKE_BUILD_TYPE=Debug ..
#      make
#      make my_coverage_target
#

include(CMakeParseArguments)

# Check prereqs
find_program( GCOV_PATH gcov )
find_program( LCOV_PATH  NAMES lcov lcov.bat lcov.exe lcov.perl PATHS ${CMAKE_PREFIX_PATH}/bin)
find_program( GENHTML_PATH NAMES genhtml genhtml.perl genhtml.bat PATHS ${CMAKE_PREFIX_PATH}/bin)
find_program( SIMPLE_PYTHON_EXECUTABLE python3 )
find_program(LCOV_COBERTURA_PATH lcov_cobertura.py PATHS ${CMAKE_MODULE_PATH})
find_program( CPP_FILT_PATH c++filt)

if(NOT GCOV_PATH)
    message(FATAL_ERROR "gcov not found! Aborting...")
endif() # NOT GCOV_PATH

if("${CMAKE_CXX_COMPILER_ID}" MATCHES "(Apple)?[Cc]lang")
    if("${CMAKE_CXX_COMPILER_VERSION}" VERSION_LESS 3)
        message(FATAL_ERROR "Clang version must be 3.0.0 or greater! Aborting...")
    endif()
elseif(NOT CMAKE_COMPILER_IS_GNUCXX)
    message(FATAL_ERROR "Compiler is not GNU gcc! Aborting...")
endif()

set(COVERAGE_COMPILER_FLAGS "-g -O0 --coverage -fprofile-arcs -ftest-coverage"
        CACHE INTERNAL "")

set(CMAKE_CXX_FLAGS_COVERAGE
        ${COVERAGE_COMPILER_FLAGS}
        CACHE STRING "Flags used by the C++ compiler during coverage builds."
        FORCE )
set(CMAKE_C_FLAGS_COVERAGE
        ${COVERAGE_COMPILER_FLAGS}
        CACHE STRING "Flags used by the C compiler during coverage builds."
        FORCE )
set(CMAKE_EXE_LINKER_FLAGS_COVERAGE
        ""
        CACHE STRING "Flags used for linking binaries during coverage builds."
        FORCE )
set(CMAKE_SHARED_LINKER_FLAGS_COVERAGE
        ""
        CACHE STRING "Flags used by the shared libraries linker during coverage builds."
        FORCE )
mark_as_advanced(
        CMAKE_CXX_FLAGS_COVERAGE
        CMAKE_C_FLAGS_COVERAGE
        CMAKE_EXE_LINKER_FLAGS_COVERAGE
        CMAKE_SHARED_LINKER_FLAGS_COVERAGE )

if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(WARNING "Code coverage results with an optimised (non-Debug) build may be misleading")
endif() # NOT CMAKE_BUILD_TYPE STREQUAL "Debug"

if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    link_libraries(gcov)
else()
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
endif()

# Creates a new target that will wrap an existing target with code coverage
# gathering/reporting
# NOTE! The executable should always have a ZERO as exit code otherwise
# the coverage generation will not complete.
#
# WRAP_TARGET_WITH_COVERAGE(
#     NAME                                     # Name for coverage target
#     WRAPPED_TARGET target                    # Target to wrap
# )
function(WRAP_TARGET_WITH_COVERAGE)

    set(options NONE)
    set(oneValueArgs WRAPPED_TARGET NAME)
    set(multiValueArgs EXECUTABLE EXECUTABLE_ARGS DEPENDENCIES)
    cmake_parse_arguments(Coverage "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT LCOV_PATH)
        message(FATAL_ERROR "lcov not found! Aborting...")
    else()
        message(STATUS "  using 'lcov' at ${LCOV_PATH}")
    endif() # NOT LCOV_PATH

    if(NOT GENHTML_PATH)
        message(FATAL_ERROR "genhtml not found! Aborting...")
    else()
        message(STATUS "  using 'genhtml' at ${GENHTML_PATH}")
    endif() # NOT GENHTML_PATH

    if(NOT LCOV_COBERTURA_PATH)
        message(FATAL_ERROR "lcov_cobertura.py not found! Aborting...")
    else()
        message(STATUS "  using 'lcov_cobertura.py' at ${LCOV_COBERTURA_PATH}")
    endif() # NOT LCOV_COBERTURA_PATH

    if(NOT CPP_FILT_PATH)
        message(FATAL_ERROR "c++filt not found! Aborting...")
    else()
        message(STATUS "  using 'c++filt' at ${CPP_FILT_PATH}")
    endif() # NOT CPP_FILT

    # Setup target
    add_custom_target(prepareCoverage_${Coverage_NAME}
                      # Print lcov info
                      COMMAND ${LCOV_PATH} --version
                      # Cleanup lcov
                      COMMAND ${LCOV_PATH} ${COVERAGE_LCOV_ARGS} -d . --zerocounters
                      # Create baseline to make sure untouched files show up in the report
                      COMMAND ${LCOV_PATH} ${COVERAGE_LCOV_ARGS} -d . -c -i -o ${Coverage_NAME}.base

                      WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
                      )
    # Run prepare step before running wrapped target
    add_dependencies(${Coverage_WRAPPED_TARGET} prepareCoverage_${Coverage_NAME})

    add_custom_target(${Coverage_NAME}
                      # Capturing lcov counters and generating report
                      COMMAND ${LCOV_PATH} ${COVERAGE_LCOV_ARGS} -d . --capture --output-file ${Coverage_NAME}.info
                      # add baseline counters
                      COMMAND ${LCOV_PATH} ${COVERAGE_LCOV_ARGS} -a ${Coverage_NAME}.base -a ${Coverage_NAME}.info --output-file ${Coverage_NAME}.total
                      COMMAND ${LCOV_PATH} ${COVERAGE_LCOV_ARGS} --remove ${Coverage_NAME}.total ${COVERAGE_EXCLUDES} --output-file ${PROJECT_BINARY_DIR}/${Coverage_NAME}.info.cleaned
                      COMMAND ${GENHTML_PATH} -o ${Coverage_NAME} ${PROJECT_BINARY_DIR}/${Coverage_NAME}.info.cleaned
                      COMMAND ${SIMPLE_PYTHON_EXECUTABLE} ${LCOV_COBERTURA_PATH} --demangle -b ${PROJECT_SOURCE_DIR} -o ${Coverage_NAME}Cobertura.xml ${PROJECT_BINARY_DIR}/${Coverage_NAME}.info.cleaned
                      COMMAND ${CMAKE_COMMAND} -E remove ${Coverage_NAME}.base ${Coverage_NAME}.info ${Coverage_NAME}.total ${PROJECT_BINARY_DIR}/${Coverage_NAME}.info.cleaned

                      WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
                      COMMENT "Resetting code coverage counters to zero.\nProcessing code coverage counters and generating report."
                      )

    add_dependencies(${Coverage_NAME} ${Coverage_WRAPPED_TARGET})

    # Show info where to find the report
    add_custom_command(TARGET ${Coverage_NAME} POST_BUILD
            COMMAND ;
            COMMENT "Open ./${Coverage_NAME}/index.html in your browser to view the coverage report."
            )

endfunction() # WRAP_TARGET_WITH_COVERAGE

function(APPEND_COVERAGE_COMPILER_FLAGS)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COVERAGE_COMPILER_FLAGS}" PARENT_SCOPE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COVERAGE_COMPILER_FLAGS}" PARENT_SCOPE)
    message(STATUS "Appending code coverage compiler flags: ${COVERAGE_COMPILER_FLAGS}")
endfunction() # APPEND_COVERAGE_COMPILER_FLAGS
