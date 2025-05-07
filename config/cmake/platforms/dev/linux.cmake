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

set(BDS_BUILD_WITH_SSP ON CACHE BOOL "Build with SSP")
set(BDS_BUILD_WITH_ASAN ON CACHE BOOL "Build with ASAN")

set(CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}/matter-install" CACHE PATH "Path to Matter installation")

# Matter settings
set(BDS_MATTER_USE_RANDOM_PORT ON CACHE BOOL "Use random port")
set(BDS_MATTER_DELEGATE_IMPLEMENTATIONS
    "${CMAKE_SOURCE_DIR}/core/src/subsystems/matter/delegates/dev/SelfSignedCertifierOperationalCredentialsIssuer.cpp"
    CACHE
    STRING
    "Delegate implementations")
set(BDS_MATTER_DELEGATE_HEADER_PATHS
    "${CMAKE_SOURCE_DIR}/core/src/subsystems/matter/delegates;${CMAKE_SOURCE_DIR}/core/src/subsystems/matter/delegates/dev"
    CACHE
    STRING
    "Delegate header paths")
set(BDS_MATTER_PROVIDER_IMPLEMENTATIONS
    "${CMAKE_SOURCE_DIR}/core/src/subsystems/matter/providers/dev/BartonTestDACProvider.cpp;${CMAKE_SOURCE_DIR}/core/src/subsystems/matter/providers/default/DefaultCommissionableDataProvider.cpp"
    CACHE
    STRING
    "Provider implementations")
set(BDS_MATTER_PROVIDER_HEADER_PATHS
    "${CMAKE_SOURCE_DIR}/core/src/subsystems/matter/providers;${CMAKE_SOURCE_DIR}/core/src/subsystems/matter/providers/dev;${CMAKE_SOURCE_DIR}/core/src/subsystems/matter/providers/default"
    CACHE
    STRING
    "Provider header paths")
