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

#
# OpenTelemetry C++ SDK dependency management.
# Included conditionally when BCORE_OBSERVABILITY=ON.
#

include(FetchContent)

# Show git clone/checkout progress for this large dependency
set(_BCORE_SAVE_FETCHCONTENT_QUIET ${FETCHCONTENT_QUIET})
set(FETCHCONTENT_QUIET OFF)

set(OPENTELEMETRY_CPP_GIT_REPOSITORY https://github.com/open-telemetry/opentelemetry-cpp)
set(OPENTELEMETRY_CPP_GIT_TAG v1.16.1)

# Configure opentelemetry-cpp build options: minimal component set
set(WITH_OTLP_HTTP ON CACHE BOOL "Enable OTLP HTTP exporter" FORCE)
set(WITH_OTLP_GRPC OFF CACHE BOOL "Disable OTLP gRPC exporter" FORCE)
set(WITH_PROMETHEUS OFF CACHE BOOL "Disable Prometheus exporter" FORCE)
set(WITH_ZIPKIN OFF CACHE BOOL "Disable Zipkin exporter" FORCE)
set(WITH_JAEGER OFF CACHE BOOL "Disable Jaeger exporter" FORCE)
set(WITH_LOGS_PREVIEW ON CACHE BOOL "Enable Logs SDK" FORCE)
set(WITH_ABI_VERSION_1 ON CACHE BOOL "Use ABI version 1" FORCE)
# Save BUILD_TESTING before overriding for otel-cpp
set(_BCORE_SAVE_BUILD_TESTING ${BUILD_TESTING})
set(BUILD_TESTING OFF CACHE BOOL "Disable otel-cpp tests" FORCE)
set(WITH_EXAMPLES OFF CACHE BOOL "Disable otel-cpp examples" FORCE)
set(WITH_BENCHMARK OFF CACHE BOOL "Disable otel-cpp benchmarks" FORCE)
set(OTELCPP_MAINTAINER_MODE OFF CACHE BOOL "Disable maintainer mode" FORCE)
set(WITH_FUNC_TESTS OFF CACHE BOOL "Disable otel-cpp functional tests" FORCE)
set(WITH_DEPRECATED_SDK_FACTORY OFF CACHE BOOL "Use non-deprecated SDK factory methods" FORCE)

# Use libcurl for HTTP transport (already a BartonCore dependency)
set(WITH_HTTP_CLIENT_CURL ON CACHE BOOL "Use libcurl for HTTP transport" FORCE)

FetchContent_Declare(
    opentelemetry-cpp
    GIT_REPOSITORY ${OPENTELEMETRY_CPP_GIT_REPOSITORY}
    GIT_TAG ${OPENTELEMETRY_CPP_GIT_TAG}
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    GIT_SUBMODULES "third_party/nlohmann-json" "third_party/opentelemetry-proto"
)

message(STATUS "Fetching opentelemetry-cpp ${OPENTELEMETRY_CPP_GIT_TAG} ...")

# Use CMAKE_SKIP_INSTALL_RULES to prevent opentelemetry-cpp's install() rules from being
# included in BartonCore's install target, while still building all targets.
FetchContent_GetProperties(opentelemetry-cpp)
if(NOT opentelemetry-cpp_POPULATED)
    FetchContent_Populate(opentelemetry-cpp)
    set(_BCORE_SAVE_CMAKE_SKIP_INSTALL_RULES ${CMAKE_SKIP_INSTALL_RULES})
    set(CMAKE_SKIP_INSTALL_RULES ON)
    add_subdirectory(${opentelemetry-cpp_SOURCE_DIR} ${opentelemetry-cpp_BINARY_DIR})
    set(CMAKE_SKIP_INSTALL_RULES ${_BCORE_SAVE_CMAKE_SKIP_INSTALL_RULES})
    # CMAKE_SKIP_INSTALL_RULES prevents cmake_install.cmake from being generated,
    # but the parent's cmake_install.cmake still tries to include it. Create an empty one.
    file(WRITE ${opentelemetry-cpp_BINARY_DIR}/cmake_install.cmake "")
endif()

# Restore BUILD_TESTING and FETCHCONTENT_QUIET after otel-cpp
set(BUILD_TESTING ${_BCORE_SAVE_BUILD_TESTING} CACHE BOOL "Build tests" FORCE)
set(FETCHCONTENT_QUIET ${_BCORE_SAVE_FETCHCONTENT_QUIET})
