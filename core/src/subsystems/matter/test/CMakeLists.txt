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

include(BDSAddCppTest)

# Avoid throwing the kitchen sink into sources and LIBS.
# Only build units that are actually under test - this
# can be useful for discovering unintended tight coupling.

file(GLOB TEST_SRC *.cpp)
list(APPEND TEST_SRC ../ReadPrepareParamsBuilder.cpp
                     ../MatterCommon.cpp
                     ../CustomCommissionableDataProvider.cpp
                     device-service-mock-property-provider.c
                     ${CMAKE_SOURCE_DIR}/api/c/src/provider/device-service-property-provider.c)

pkg_check_modules(OPENSSL REQUIRED openssl)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--wrap=deviceServiceGetSystemProperty")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--wrap=deviceServiceSetSystemProperty")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--wrap=deviceServiceGetAllSystemProperties")

# This executable should contain a collection of mini integration (mostly unit)
# tests that validate classes meant to make some deficient SDK classes more usable.
# Add more executables for truly isolated units that don't link to other libraries.
bds_add_cpp_test(NAME testMatterSDKTooling
                LIBS ${BDS_MATTER_LIB} ${OPENSSL_LINK_LIBRARIES} gmock crypto xhLog xhUtil xhDeviceDescriptors xhConfig
                INCLUDES ${CMAKE_SOURCE_DIR}/src
                         ${CMAKE_CURRENT_SOURCE_DIR}/..
                         ${CMAKE_BINARY_DIR}/matter-install/include/matter
                         ${CMAKE_SOURCE_DIR}/api/c/public
                         ${PRIVATE_API_INCLUDES}
                SOURCES ${TEST_SRC}
)

bds_configure_glib()
