cmake_minimum_required(VERSION 3.18)
#TODO: project version (snag from git tag)
project(brtn-ds)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)

add_compile_definitions(_GNU_SOURCE)

if ("${CMAKE_PREFIX_PATH}" STREQUAL "")
    # This is a hacky way to deal with dependencies that don't
    # publish a pkg-config or cmake package file (otbr, matter).
    # This assumes that one is compiling for development, where
    # we also own the build/installation of those dependencies.
    set(CMAKE_PREFIX_PATH ${CMAKE_INSTALL_PREFIX})
endif()

enable_testing()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/config/cmake/modules")
include("${PROJECT_SOURCE_DIR}/config/cmake/options.cmake")
include("${PROJECT_SOURCE_DIR}/config/cmake/DependencyVersions.cmake")

# Before building our stuff, build matter if we need to
if (BDS_MATTER)
    # Only search default locations. Mostly this is for clients who build matter out of band
    # already. The script will check our usual matter build location.
    find_library(MATTER NAMES ${BDS_MATTER_LIB})

    if (${MATTER} STREQUAL "MATTER-NOTFOUND")
        execute_process(
            COMMAND ./build-matter.sh
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
    else()
        message(STATUS "Matter found at ${MATTER}")
    endif()
endif()

if (NOT BDS_PROVIDE_LIBS)
    add_subdirectory(libs)
endif()

add_subdirectory(core)

if (BDS_BUILD_REFERENCE)
    add_subdirectory(reference)
endif()
