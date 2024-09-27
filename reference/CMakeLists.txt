#------------------------------ tabstop = 4 ----------------------------------
#
# Copyright (C) 2024 Comcast Corporation
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

cmake_minimum_required(VERSION 3.18)
#TODO: project version (snag from git tag)
project(brtn-reference)

include(BDSConfigureGLib)

file(GLOB_RECURSE BRTN_DS_REFERENCE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)

add_executable(brtn-ds-reference ${BRTN_DS_REFERENCE_SOURCES})

# add library dependencies for this binary
target_link_libraries(brtn-ds-reference brtnDeviceServiceShared linenoise)

bds_configure_glib()