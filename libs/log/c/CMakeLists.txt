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
# CMake definition for the fcore 'log library'
# Should not be utilized directly, but imported from our parent
# file via the 'add_subdirectory()' routine.
#
#--------------------------------------------------------------------------------------

# setup include paths
include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/public
        ${ZILKER_INCLUDE_DIR}
        ${CROSS_OUTPUT}/include     # 3rd Party
)

# rdk needs some additional include paths
if (CONFIG_PLATFORM_RDK)
    include_directories(
        ${CROSS_OUTPUT}/include/ccsp
        ${CROSS_OUTPUT}/include/dbus-1.0
        ${CROSS_OUTPUT}/lib/dbus-1.0/include
    )
endif()

set(LOG_SRC src/loggingDebug.c)

# Add the logging common file
set(LOG_SRC ${LOG_SRC} src/loggingCommon.c)

add_library(xhLog STATIC ${LOG_SRC})

bds_configure_glib()

# export our 'public' folder so others can include our headers
target_include_directories(xhLog PUBLIC public)

#TODO: test target
