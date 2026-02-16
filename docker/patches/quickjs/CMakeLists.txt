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

# Read version from VERSION file
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" QUICKJS_VERSION_STRING)
string(STRIP "${QUICKJS_VERSION_STRING}" QUICKJS_VERSION_STRING)

# Convert date-based version to semantic version
# QuickJS uses YYYY-MM-DD format, we'll convert to YYYY.MM.DD for CMake
string(REPLACE "-" "." QUICKJS_VERSION "${QUICKJS_VERSION_STRING}")

project(quickjs 
    VERSION ${QUICKJS_VERSION}
    DESCRIPTION "QuickJS JavaScript Engine"
    LANGUAGES C
)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Platform detection and configuration
if(WIN32)
    set(CONFIG_WIN32 ON)
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(CONFIG_M32 ON)
    endif()
elseif(APPLE)
    set(CONFIG_DARWIN ON)
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    set(CONFIG_FREEBSD ON)
endif()

# Build options
option(CONFIG_LTO "Enable Link Time Optimization" OFF)
option(CONFIG_ASAN "Enable Address Sanitizer" OFF)
option(CONFIG_MSAN "Enable Memory Sanitizer" OFF)
option(CONFIG_UBSAN "Enable Undefined Behavior Sanitizer" OFF)
option(CONFIG_PROFILE "Enable profiling" OFF)
option(BUILD_SHARED_LIBS "Build shared library" ON)

# Compiler-specific settings
if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    set(CONFIG_CLANG ON)
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU")
    set(CONFIG_GCC ON)
endif()

# Core library source files
set(QJS_LIB_SOURCES
    quickjs.c
    dtoa.c
    libregexp.c
    libunicode.c
    cutils.c
    quickjs-libc.c
)

# Create the library (shared or static based on BUILD_SHARED_LIBS)
add_library(quickjs ${QJS_LIB_SOURCES})

# Set library properties
set_target_properties(quickjs PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    OUTPUT_NAME "quickjs"
)

# Preprocessor definitions
target_compile_definitions(quickjs PRIVATE
    _GNU_SOURCE
    CONFIG_VERSION="${QUICKJS_VERSION_STRING}"
)

# Define JS_SHARED_LIBRARY only for shared builds
if(BUILD_SHARED_LIBS)
    target_compile_definitions(quickjs PRIVATE JS_SHARED_LIBRARY)
endif()

if(WIN32)
    target_compile_definitions(quickjs PRIVATE
        __USE_MINGW_ANSI_STDIO  # for standard snprintf behavior
    )
endif()

# Check for closefrom support (Unix-like systems)
if(NOT WIN32)
    include(CheckCSourceCompiles)
    check_c_source_compiles("
        #include <stdlib.h>
        #include <unistd.h>
        int main() { closefrom(0); return 0; }
    " HAVE_CLOSEFROM)
    
    if(HAVE_CLOSEFROM)
        target_compile_definitions(quickjs PRIVATE HAVE_CLOSEFROM)
    endif()
endif()

# Compiler flags
target_compile_options(quickjs PRIVATE
    -fwrapv  # ensure that signed overflows behave as expected
)

if(CONFIG_CLANG OR CONFIG_GCC)
    target_compile_options(quickjs PRIVATE
        -Wall
        -Wextra
        -Wno-sign-compare
        -Wno-missing-field-initializers
        -Wundef
        -Wuninitialized
        -Wunused
        -Wno-unused-parameter
        -Wwrite-strings
        -Wchar-subscripts
        -funsigned-char
    )
    
    if(CONFIG_GCC)
        target_compile_options(quickjs PRIVATE
            -Wno-array-bounds
            -Wno-format-truncation
        )
        
        # Check for newer GCC warnings
        include(CheckCCompilerFlag)
        check_c_compiler_flag(-Wno-infinite-recursion HAS_WNO_INFINITE_RECURSION)
        if(HAS_WNO_INFINITE_RECURSION)
            target_compile_options(quickjs PRIVATE -Wno-infinite-recursion)
        endif()
    endif()
endif()

# Architecture-specific settings
if(CONFIG_M32)
    target_compile_options(quickjs PRIVATE
        -msse2
        -mfpmath=sse  # use SSE math for correct FP rounding
    )
    if(NOT WIN32)
        target_compile_options(quickjs PRIVATE -m32)
        target_link_options(quickjs PRIVATE -m32)
    endif()
endif()

# Build type specific flags
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(quickjs PRIVATE -O0)
elseif(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    target_compile_options(quickjs PRIVATE -Os)
else()
    target_compile_options(quickjs PRIVATE -O2)
endif()

# Link Time Optimization
if(CONFIG_LTO)
    set_target_properties(quickjs PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION TRUE
    )
endif()

# Sanitizer support
if(CONFIG_ASAN)
    target_compile_options(quickjs PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(quickjs PRIVATE -fsanitize=address -fno-omit-frame-pointer)
endif()

if(CONFIG_MSAN)
    target_compile_options(quickjs PRIVATE -fsanitize=memory -fno-omit-frame-pointer)
    target_link_options(quickjs PRIVATE -fsanitize=memory -fno-omit-frame-pointer)
endif()

if(CONFIG_UBSAN)
    target_compile_options(quickjs PRIVATE -fsanitize=undefined -fno-omit-frame-pointer)
    target_link_options(quickjs PRIVATE -fsanitize=undefined -fno-omit-frame-pointer)
endif()

# Profiling support
if(CONFIG_PROFILE)
    target_compile_options(quickjs PRIVATE -p)
    target_link_options(quickjs PRIVATE -p)
endif()

# System libraries
target_link_libraries(quickjs PRIVATE m)
if(NOT WIN32)
    target_link_libraries(quickjs PRIVATE dl pthread)
else()
    target_link_libraries(quickjs PRIVATE pthread)
endif()

# Export symbols for shared library
if(NOT WIN32)
    target_link_options(quickjs PRIVATE -rdynamic)
endif()

###############################################################################
# Build qjs (QuickJS interpreter) and qjsc (QuickJS compiler) executables
###############################################################################

# qjs - QuickJS interpreter
add_executable(qjs qjs.c)
target_link_libraries(qjs PRIVATE quickjs)
set_target_properties(qjs PROPERTIES
    OUTPUT_NAME "qjs"
)

# qjsc - QuickJS compiler
add_executable(qjsc qjsc.c)
target_link_libraries(qjsc PRIVATE quickjs)
set_target_properties(qjsc PROPERTIES
    OUTPUT_NAME "qjsc"
)

# Install configuration
include(GNUInstallDirs)

install(TARGETS quickjs qjs qjsc
    EXPORT quickjs-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES
    quickjs.h
    quickjs-libc.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/quickjs
)

# Create and install CMake config files
include(CMakePackageConfigHelpers)

# Generate version file
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/quickjs-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Create a simple config file directly
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/quickjs-config.cmake" "
include(CMakeFindDependencyMacro)

# Find required dependencies
find_dependency(Threads)

# Include the targets file
include(\"\${CMAKE_CURRENT_LIST_DIR}/quickjs-targets.cmake\")

# Check that all required components are available
check_required_components(quickjs)
")

# Install the targets and config files
install(EXPORT quickjs-targets
    FILE quickjs-targets.cmake
    NAMESPACE quickjs::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/quickjs
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/quickjs-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/quickjs-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/quickjs
)

# Print configuration summary
message(STATUS "QuickJS ${QUICKJS_VERSION_STRING} Configuration Summary:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Shared library: ${BUILD_SHARED_LIBS}")
message(STATUS "  Executables: qjs, qjsc")
message(STATUS "  LTO: ${CONFIG_LTO}")
message(STATUS "  Address Sanitizer: ${CONFIG_ASAN}")
message(STATUS "  Memory Sanitizer: ${CONFIG_MSAN}")
message(STATUS "  UB Sanitizer: ${CONFIG_UBSAN}")
message(STATUS "  Profiling: ${CONFIG_PROFILE}")
if(CONFIG_M32)
    message(STATUS "  32-bit build: ON")
endif()
