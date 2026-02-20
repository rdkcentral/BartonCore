# ------------------------------ tabstop = 4 ----------------------------------
#
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2026 Comcast Cable Communications Management, LLC
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
# CMake module for building the matter.js cluster bundle for SBMD QuickJS runtime.
#
# This module provides functions to build and install the matter.js cluster
# JavaScript bundle that provides TLV encoding/decoding for SBMD drivers.
#
# Controlled by: BCORE_MATTER_USE_MATTERJS cmake option
#

include_guard(GLOBAL)

# Default output location for the matter.js cluster bundle
set(MATTERJS_CLUSTERS_OUTPUT_DIR "${CMAKE_BINARY_DIR}/matterjs-clusters" CACHE PATH
    "Directory for the matter.js cluster bundle output")

# Version of matter.js to use
set(MATTERJS_VERSION "v0.16.9" CACHE STRING
    "Version/tag of matter.js to build the cluster bundle from")

# Path to the built bundle file
set(MATTERJS_CLUSTERS_BUNDLE "${MATTERJS_CLUSTERS_OUTPUT_DIR}/matter-clusters.js")

#
# Function: bcore_matterjs_build_clusters
#
# Builds the matter.js cluster bundle for QuickJS.
# This function creates a custom target that runs the build script.
#
# Usage:
#   bcore_matterjs_build_clusters()
#
function(bcore_matterjs_build_clusters)
    # Check if matter.js integration is enabled
    if(NOT BCORE_MATTER_USE_MATTERJS)
        message(STATUS "matter.js integration disabled (BCORE_MATTER_USE_MATTERJS=OFF)")
        set(BCORE_HAS_MATTERJS_CLUSTERS FALSE CACHE BOOL "matter.js cluster bundle is available" FORCE)
        return()
    endif()

    set(BUILD_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/build-matterjs-clusters.sh")

    # Check if npm is available
    find_program(NPM_EXECUTABLE npm)
    if(NOT NPM_EXECUTABLE)
        message(WARNING "npm not found - matter.js cluster bundle will not be built. "
                        "SBMD drivers will use legacy JSON/TLV conversion.")
        return()
    endif()

    # Create custom command to build the bundle
    add_custom_command(
        OUTPUT "${MATTERJS_CLUSTERS_BUNDLE}"
        COMMAND "${BUILD_SCRIPT}" -o "${MATTERJS_CLUSTERS_OUTPUT_DIR}" -v "${MATTERJS_VERSION}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Building matter.js cluster bundle for QuickJS SBMD runtime..."
        VERBATIM
    )

    # Create custom target that depends on the bundle
    add_custom_target(matterjs_clusters
        DEPENDS "${MATTERJS_CLUSTERS_BUNDLE}"
    )

    # Set a compile definition indicating the bundle is available
    set(BCORE_HAS_MATTERJS_CLUSTERS TRUE CACHE BOOL "matter.js cluster bundle is available" FORCE)

    message(STATUS "matter.js cluster bundle will be built to: ${MATTERJS_CLUSTERS_BUNDLE}")
endfunction()

#
# Function: bcore_matterjs_get_bundle_path
#
# Gets the path to the matter.js cluster bundle.
#
# Usage:
#   bcore_matterjs_get_bundle_path(OUTPUT <variable>)
#
# Arguments:
#   OUTPUT - Variable to store the bundle path
#
function(bcore_matterjs_get_bundle_path)
    set(oneValueArgs OUTPUT)
    cmake_parse_arguments(ARG "" "${oneValueArgs}" "" ${ARGN})

    if(NOT ARG_OUTPUT)
        message(FATAL_ERROR "bcore_matterjs_get_bundle_path: OUTPUT argument is required")
    endif()

    set(${ARG_OUTPUT} "${MATTERJS_CLUSTERS_BUNDLE}" PARENT_SCOPE)
endfunction()

#
# Function: bcore_matterjs_embed_bundle
#
# Generates a C header file with the matter.js bundle embedded as a string.
# This allows the bundle to be compiled into the binary.
#
# Usage:
#   bcore_matterjs_embed_bundle(
#       OUTPUT <header_file>
#       VARIABLE_NAME <name>
#   )
#
# Arguments:
#   OUTPUT - Path to the generated header file
#   VARIABLE_NAME - Name of the C variable to use
#
function(bcore_matterjs_embed_bundle)
    set(oneValueArgs OUTPUT VARIABLE_NAME)
    cmake_parse_arguments(ARG "" "${oneValueArgs}" "" ${ARGN})

    if(NOT ARG_OUTPUT)
        message(FATAL_ERROR "bcore_matterjs_embed_bundle: OUTPUT argument is required")
    endif()

    if(NOT ARG_VARIABLE_NAME)
        set(ARG_VARIABLE_NAME "kMatterJsClustersBundle")
    endif()

    set(EMBED_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/embed-js-as-header.py")

    add_custom_command(
        OUTPUT "${ARG_OUTPUT}"
        COMMAND "${CMAKE_COMMAND}" -E echo "Embedding matter.js cluster bundle as C header..."
        COMMAND python3 "${EMBED_SCRIPT}"
            --input "${MATTERJS_CLUSTERS_BUNDLE}"
            --output "${ARG_OUTPUT}"
            --variable "${ARG_VARIABLE_NAME}"
        DEPENDS "${MATTERJS_CLUSTERS_BUNDLE}" "${EMBED_SCRIPT}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Generating embedded C header for matter.js cluster bundle"
        VERBATIM
    )
endfunction()
