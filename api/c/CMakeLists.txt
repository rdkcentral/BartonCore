# This is just to satisfy zilker. Barton device service will not use this library.
cmake_minimum_required(VERSION 3.18)
project(brtn-ds-private)

file(GLOB_RECURSE BRTN_DS_PRIVATE_API_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/private/*.c)

add_library(brtnDeviceServicePrivateAPI STATIC ${BRTN_DS_PRIVATE_API_SOURCES})

target_link_libraries(brtnDeviceServicePrivateAPI
        brtnDeviceServiceConfig
        xhJsonHelper
        xhDeviceDescriptors
        xhTypes
)

target_include_directories(brtnDeviceServicePrivateAPI
        PRIVATE ${CROSS_OUTPUT}/include/libxml2
        PUBLIC public/private
)
