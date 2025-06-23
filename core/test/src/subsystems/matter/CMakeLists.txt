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

include(BCoreAddCppTest)

# Avoid throwing the kitchen sink into sources and LIBS.
# Only build units that are actually under test - this
# can be useful for discovering unintended tight coupling.

file(GLOB TEST_SRC *.cpp)
list(APPEND TEST_SRC ${PROJECT_SOURCE_DIR}/core/src/subsystems/matter/ReadPrepareParamsBuilder.cpp
                     ${PROJECT_SOURCE_DIR}/core/src/subsystems/matter/MatterCommon.cpp
                     ${PROJECT_SOURCE_DIR}/core/src/subsystems/matter/providers/BartonCommissionableDataProvider.cpp
                     ${PROJECT_SOURCE_DIR}/core/src/subsystems/matter/providers/default/DefaultCommissionableDataProvider.cpp
                     ${PROJECT_SOURCE_DIR}/api/c/src/provider/barton-core-property-provider.c
                     ${PROJECT_SOURCE_DIR}/api/c/src/provider/barton-core-default-property-provider.c
                     ${PROJECT_SOURCE_DIR}/core/src/deviceServiceConfiguration.c)

pkg_check_modules(OPENSSL REQUIRED openssl)

# This executable should contain a collection of mini integration (mostly unit)
# tests that validate classes meant to make some deficient SDK classes more usable.
# Add more executables for truly isolated units that don't link to other libraries.
bcore_add_cpp_test(NAME testMatterSDKTooling
                LIBS ${BCORE_MATTER_LIB} ${OPENSSL_LINK_LIBRARIES} gmock crypto xhLog xhUtil xhDeviceDescriptors xhConfig
                INCLUDES ${BARTON_PRIVATE_INCLUDES}
                         ${CMAKE_BINARY_DIR}/matter-install/include/matter
                         ${PROJECT_SOURCE_DIR}/api/c/public
                         ${PRIVATE_API_INCLUDES}
                SOURCES ${TEST_SRC}
)

if (BUILD_TESTING)
    target_link_options(testMatterSDKTooling PRIVATE
        -Wl,--wrap=deviceServiceGetSystemProperty
        -Wl,--wrap=deviceServiceSetSystemProperty
        -Wl,--wrap=deviceServiceGetAllSystemProperties
        -Wl,--wrap=deviceServiceConfigurationGetPropertyProvider
    )
endif()

bcore_configure_glib()
