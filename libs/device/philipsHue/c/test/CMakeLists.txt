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
# CMake definition for the fcore 'device-xhPhilipsHue library unit tests'
# Should not be utilized directly, but imported from our parent
# file via the 'add_subdirectory()' routine.
#
#--------------------------------------------------------------------------------------

# setup a unit test, but such that it is NOT part of the 'make all' sequence.
# allows us to only build tests on the platforms we can (meaning
# not during a cross-compile session).  should be built as part of
# the 'make manualTest' command

# define source files (using wildcard)
file(GLOB SOURCES "src/*.c")

# build as an executable, but NOT part of the "make all" sequence
add_executable(testDevicePhilipsHueLib EXCLUDE_FROM_ALL ${SOURCES})

# add library dependencies for this test-binary
target_link_libraries(testDevicePhilipsHueLib xhPhilipsHue pthread)
target_include_directories(testDevicePhilipsHueLib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# NOTE: commented out the 'add_test' since this is really more of an interactive
#       program should be executed manually
add_dependencies(manualTest testDevicePhilipsHueLib)

