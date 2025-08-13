# ------------------------------ tabstop = 4 ----------------------------------
#
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2025 Comcast Cable Communications Management, LLC
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

#
# Created by Christian Leithner on 4/21/2025.
#

set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type")

set(BCORE_BUILD_WITH_SSP ON CACHE BOOL "Build with SSP")
set(BCORE_BUILD_WITH_ASAN ON CACHE BOOL "Build with ASAN")

set(CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}/matter-install" CACHE PATH "Path to Matter installation")

# Matter settings
include(${CMAKE_SOURCE_DIR}/config/cmake/modules/BCoreMatterHelper.cmake)

set(BCORE_MATTER_USE_RANDOM_PORT ON CACHE BOOL "Use random port")

bcore_matter_helper_add_implementation(
    EXTENSION_TYPE DELEGATE
    DEV SelfSignedCertifierOperationalCredentialsIssuer
)

bcore_matter_helper_add_implementation(
    EXTENSION_TYPE PROVIDER
    DEFAULT DefaultCommissionableDataProvider
    DEV BartonTestDACProvider
)
