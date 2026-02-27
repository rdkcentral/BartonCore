# ------------------------------ tabstop = 4 ----------------------------------
#
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2026 Comcast Cable Communications Management, LLC
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

cmake_minimum_required(VERSION 3.16)

project(mquickjs
    VERSION 1.0.0
    DESCRIPTION "Micro QuickJS Javascript Engine"
    LANGUAGES C
)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

###############################################################################
# Stage 1: Build host tools to generate stdlib headers
#
# mquickjs requires a two-stage build:
#   1. Compile mqjs_stdlib (host tool) from mqjs_stdlib.c + mquickjs_build.c
#   2. Run mqjs_stdlib to generate mquickjs_atom.h and mqjs_stdlib.h
# These generated headers are needed to compile the actual library.
###############################################################################

# Build the stdlib generator tool (always built for the host)
add_executable(mqjs_stdlib_gen
    ${CMAKE_CURRENT_SOURCE_DIR}/mqjs_stdlib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/mquickjs_build.c
)
target_compile_definitions(mqjs_stdlib_gen PRIVATE _GNU_SOURCE)
target_compile_options(mqjs_stdlib_gen PRIVATE -O2 -fno-math-errno -fno-trapping-math)

# Generate mquickjs_atom.h by running: ./mqjs_stdlib_gen -a > mquickjs_atom.h
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mquickjs_atom.h
    COMMAND $<TARGET_FILE:mqjs_stdlib_gen> -a > ${CMAKE_CURRENT_BINARY_DIR}/mquickjs_atom.h
    DEPENDS mqjs_stdlib_gen
    COMMENT "Generating mquickjs_atom.h"
    VERBATIM
)

# Generate mqjs_stdlib.h by running: ./mqjs_stdlib_gen > mqjs_stdlib.h
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mqjs_stdlib.h
    COMMAND $<TARGET_FILE:mqjs_stdlib_gen> > ${CMAKE_CURRENT_BINARY_DIR}/mqjs_stdlib.h
    DEPENDS mqjs_stdlib_gen
    COMMENT "Generating mqjs_stdlib.h"
    VERBATIM
)

# Custom target so we can depend on the generated headers
add_custom_target(mquickjs_generated_headers
    DEPENDS
        ${CMAKE_CURRENT_BINARY_DIR}/mquickjs_atom.h
        ${CMAKE_CURRENT_BINARY_DIR}/mqjs_stdlib.h
)

###############################################################################
# Stage 2: Build the mquickjs static library
###############################################################################

set(MQUICKJS_LIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/mquickjs.c
    ${CMAKE_CURRENT_SOURCE_DIR}/dtoa.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/cutils.c
)

add_library(mquickjs STATIC ${MQUICKJS_LIB_SOURCES})
add_dependencies(mquickjs mquickjs_generated_headers)

# The generated mquickjs_atom.h must be found when compiling mquickjs.c
target_include_directories(mquickjs
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}  # for mquickjs_atom.h
        ${CMAKE_CURRENT_SOURCE_DIR}  # for source-local headers
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/mquickjs>
)

target_compile_definitions(mquickjs PRIVATE
    _GNU_SOURCE
)

target_compile_options(mquickjs PRIVATE
    -Os
    -fno-math-errno
    -fno-trapping-math
    -fwrapv
    -Wall
    -Wno-unused-parameter
)

target_link_libraries(mquickjs PUBLIC m)

###############################################################################
# Install configuration
###############################################################################

include(GNUInstallDirs)

# Install the static library
install(TARGETS mquickjs
    EXPORT mquickjs-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Install public headers
install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/mquickjs.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mquickjs_build.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mquickjs_opcode.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mquickjs_priv.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cutils.h
    ${CMAKE_CURRENT_SOURCE_DIR}/list.h
    ${CMAKE_CURRENT_SOURCE_DIR}/dtoa.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libm.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mquickjs
)

# Also install the generated headers (needed at build time by consumers)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/mquickjs_atom.h
    ${CMAKE_CURRENT_BINARY_DIR}/mqjs_stdlib.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mquickjs
)

# Install the stdlib source files needed by consumers to build their own stdlib
install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/mqjs_stdlib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/mquickjs_build.c
    DESTINATION ${CMAKE_INSTALL_DATADIR}/mquickjs
)

# Create and install CMake config files
include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/mquickjs-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/mquickjs-config.cmake" "
include(CMakeFindDependencyMacro)
find_dependency(Threads)

include(\"\${CMAKE_CURRENT_LIST_DIR}/mquickjs-targets.cmake\")
check_required_components(mquickjs)
")

install(EXPORT mquickjs-targets
    FILE mquickjs-targets.cmake
    NAMESPACE mquickjs::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mquickjs
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/mquickjs-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/mquickjs-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mquickjs
)

message(STATUS "mquickjs Configuration Summary:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
