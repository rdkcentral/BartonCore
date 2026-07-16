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

cmake_minimum_required(VERSION 3.16.5)
#TODO: project version (snag from git tag)
project(barton-core-reference)

include(BCoreConfigureGLib)

option(BCORE_REFERENCE_CAMERA_SUPPORT "Enable the reference app camera stream command with GStreamer WebRTC support" OFF)

file(GLOB_RECURSE B_CORE_REFERENCE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)

if (NOT BCORE_REFERENCE_CAMERA_SUPPORT)
    list(FILTER B_CORE_REFERENCE_SOURCES EXCLUDE REGEX "cameraCategory\\.c$|cameraDeviceSession\\.c$|cameraWebrtcClient\\.c$|cameraMediaServer\\.c$")
endif()

add_executable(barton-core-reference ${B_CORE_REFERENCE_SOURCES})

# add library dependencies for this binary
target_link_options(barton-core-reference PRIVATE -L ${CMAKE_BINARY_DIR}/matter-install/lib)
target_link_libraries(barton-core-reference BartonCore linenoise)

if (BCORE_REFERENCE_CAMERA_SUPPORT)
    find_package(PkgConfig REQUIRED)
    # gio-2.0 provides GSocketService for the HTTP media server that streams to the browser.
    pkg_check_modules(GST REQUIRED gstreamer-1.0 gstreamer-webrtc-1.0 gstreamer-sdp-1.0 gstreamer-app-1.0)
    pkg_check_modules(GIO REQUIRED gio-2.0)
    target_include_directories(barton-core-reference PRIVATE ${GST_INCLUDE_DIRS} ${GIO_INCLUDE_DIRS})
    target_link_libraries(barton-core-reference ${GST_LIBRARIES} ${GIO_LIBRARIES})
    target_compile_definitions(barton-core-reference PRIVATE BCORE_REFERENCE_CAMERA_SUPPORT)
endif()

bcore_configure_glib()
