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

#--------------------------------------------------------------------------------------
#
# CMake definition for the fcore 'device descriptor library'
# Should not be utilized directly, but imported from our parent
# file via the 'add_subdirectory()' routine.
#
#--------------------------------------------------------------------------------------
include(BCoreConfigureGLib)

bcore_find_include(
    LIBXML2_INCLUDE_DIR
    libxml/parser.h
    libxml2
    "Libxml2 include directory"
    ""
)

# setup include paths
include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/public
        ${LIBXML2_INCLUDE_DIR}
)

# define source files (using wildcard)
file(GLOB SOURCES "src/*.c")

add_library(xhDeviceDescriptors STATIC ${SOURCES})
target_link_libraries(xhDeviceDescriptors xhLog xhTypes xhUtil xhXmlHelper)

# export our 'public' folder so others can include our headers
target_include_directories(xhDeviceDescriptors PUBLIC public PRIVATE)

bcore_configure_glib()

# add our test to be part of the "make manualTest"
add_subdirectory(test)
