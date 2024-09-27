# This is just to satisfy zilker. Barton device service will not use this library.
cmake_minimum_required(VERSION 3.18)
project(brtn-ds-deprecated)

file(GLOB_RECURSE BRTN_DS_DEPRECATED_API_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/deprecated/*.c)

add_library(brtnDeviceServiceDeprecatedAPI STATIC ${BRTN_DS_DEPRECATED_API_SOURCES})

target_link_libraries(brtnDeviceServiceDeprecatedAPI
        brtnDeviceServiceConfig
        xhJsonHelper
        xhDeviceDescriptors
        xhTypes
)

target_include_directories(brtnDeviceServiceDeprecatedAPI
        PRIVATE ${CROSS_OUTPUT}/include/libxml2
        PUBLIC public/deprecated
)
