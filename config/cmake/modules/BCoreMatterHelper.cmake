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
# Created by Christian Leithner on 7/16/2025.
#

include_guard(GLOBAL)
include(CMakeParseArguments)

# This file provides helper functions for maintaining cmake tracking of matter plugin code.
# It first parses the public options defined in options.cmake but then uses internal lists
# from that point on. Users may define the public options in initial cache files or passed
# as CLI options, but should use this module at any point after that.

set(MATTER_EXTENSION_TYPES "PROVIDER;DELEGATE")

# Module may be included from a cmake file that hasn't called `project` yet (likely an initial cache)
# Figure out the path relative to the current file as that works in all cases.
get_filename_component(MATTER_EXTENSION_PARENT_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../core/src/subsystems/matter" ABSOLUTE)

# Define paths for each extension type
foreach(EXTENSION_TYPE ${MATTER_EXTENSION_TYPES})
    # Convert to lowercase for directory names
    string(TOLOWER "${EXTENSION_TYPE}" extension_type_lower)

    set(MATTER_${EXTENSION_TYPE}_DEFAULT_DIR "${MATTER_EXTENSION_PARENT_DIR}/${extension_type_lower}s/default")
    set(MATTER_${EXTENSION_TYPE}_DEV_DIR "${MATTER_EXTENSION_PARENT_DIR}/${extension_type_lower}s/dev")

    message(STATUS "MATTER_${EXTENSION_TYPE}_DEFAULT_DIR: ${MATTER_${EXTENSION_TYPE}_DEFAULT_DIR}")
    message(STATUS "MATTER_${EXTENSION_TYPE}_DEV_DIR: ${MATTER_${EXTENSION_TYPE}_DEV_DIR}")

    set(MATTER_HELPER_${EXTENSION_TYPE}_HEADER_PATHS
        "${MATTER_EXTENSION_PARENT_DIR}/${extension_type_lower}s/"
        "${MATTER_${EXTENSION_TYPE}_DEFAULT_DIR}"
        "${MATTER_${EXTENSION_TYPE}_DEV_DIR}"
    )

    message(STATUS "MATTER_HELPER_${EXTENSION_TYPE}_HEADER_PATHS: ${MATTER_HELPER_${EXTENSION_TYPE}_HEADER_PATHS}")
endforeach()

# Custom associative dictionary for tracking class hiearchy dependencies of well-known
# extensions (providers/delegates, etc). Format for individial list items is:
# "ProviderName:DependencyFilePath[,DependencyFilePath,...]"
set(MATTER_HELPER_PROVIDER_DEPENDENCIES
    "DefaultCommissionableDataProvider:${MATTER_EXTENSION_PARENT_DIR}/providers/BartonCommissionableDataProvider.cpp"
)

set(MATTER_HELPER_DELEGATE_DEPENDENCIES
)

# Private
# Validates that the provided extension type is one of the allowed types
# Usage:
# _matter_helper_validate_extension_type(EXTENSION_TYPE "PROVIDER|DELEGATE")
function(_matter_helper_validate_extension_type)
    set(oneValueArgs EXTENSION_TYPE)

    cmake_parse_arguments(HELPER "" "${oneValueArgs}" "" ${ARGN})

    if (NOT HELPER_EXTENSION_TYPE)
        message(FATAL_ERROR "No extension type specified for _matter_helper_validate_extension_type")
    endif()

    list(FIND MATTER_EXTENSION_TYPES ${HELPER_EXTENSION_TYPE} extension_type_index)
    if (extension_type_index EQUAL -1)
        message(FATAL_ERROR "Invalid extension type: ${HELPER_EXTENSION_TYPE}. Must be one of: ${MATTER_EXTENSION_TYPES}")
    endif()
endfunction()

# Private
# Adds an item to a list if it does not already exist in the list.
function(_matter_helper_append_if_not_exists)
    set(oneValueArgs LIST ITEM)

    cmake_parse_arguments(HELPER "" "${oneValueArgs}" "" ${ARGN})

    if (NOT HELPER_LIST OR NOT HELPER_ITEM)
        message(FATAL_ERROR "No list or item specified for _matter_helper_append_if_not_exists")
    endif()

    list(FIND ${HELPER_LIST} ${HELPER_ITEM} index)

    if (index EQUAL -1)
        message(TRACE "Adding item ${HELPER_ITEM} to list: ${HELPER_LIST}")
        set(TEMP_LIST ${${HELPER_LIST}})
        list(APPEND TEMP_LIST ${HELPER_ITEM})
        set(${HELPER_LIST} "${TEMP_LIST}" PARENT_SCOPE)
    else()
        message(TRACE "Item ${HELPER_ITEM} already exists in list: ${HELPER_LIST}")
    endif()
endfunction()

# Private
# Given a well-known extension implementation, adds well-known dependencies documented in
# MATTER_HELPER_<EXTENSION_TYPE>_DEPENDENCIES to BCORE_MATTER_<EXTENSION_TYPE>_IMPLEMENTATIONS
function(_matter_helper_add_extension_dependency_implementations)
    set(oneValueArgs EXTENSION_TYPE NAME)

    cmake_parse_arguments(HELPER "" "${oneValueArgs}" "" ${ARGN})

    if (NOT HELPER_NAME)
        message(FATAL_ERROR "No name specified for _matter_helper_add_extension_dependency_implementations")
    endif()

    _matter_helper_validate_extension_type(EXTENSION_TYPE ${HELPER_EXTENSION_TYPE})

    # Get the dependencies and implementations variables for this extension type
    set(dependencies_var "MATTER_HELPER_${HELPER_EXTENSION_TYPE}_DEPENDENCIES")
    set(implementations_var "BCORE_MATTER_${HELPER_EXTENSION_TYPE}_IMPLEMENTATIONS")

    set(temp_implementations ${${implementations_var}})

    foreach(dependency_association IN LISTS ${dependencies_var})
        string(FIND "${dependency_association}" ":" char_index)
        string(SUBSTRING "${dependency_association}" 0 ${char_index} dependency_name)
        math(EXPR char_index "${char_index} + 1")
        string(SUBSTRING "${dependency_association}" ${char_index} -1 dependency_list)
        string(REPLACE "," ";" dependency_list "${dependency_list}")

        if (dependency_name STREQUAL HELPER_NAME)
            foreach(dependency IN LISTS dependency_list)
                message(TRACE "Adding ${HELPER_EXTENSION_TYPE} dependency implementation: ${dependency}")
                _matter_helper_append_if_not_exists(
                    LIST temp_implementations
                    ITEM "${dependency}"
                )
            endforeach()
        endif()
    endforeach()

    set(${implementations_var} "${temp_implementations}" CACHE STRING "" FORCE)
endfunction()

# Private
# Given a well-known default extension implementation, adds it and any documented dependencies to
# BCORE_MATTER_<EXTENSION_TYPE>_IMPLEMENTATIONS
function(_matter_helper_add_default_extension_implementation)
    set(oneValueArgs EXTENSION_TYPE NAME)

    cmake_parse_arguments(HELPER "" "${oneValueArgs}" "" ${ARGN})

    if (NOT HELPER_NAME)
        message(FATAL_ERROR "No name specified for _matter_helper_add_default_extension_implementation")
    endif()

    _matter_helper_validate_extension_type(EXTENSION_TYPE ${HELPER_EXTENSION_TYPE})

    set(default_dir "MATTER_${HELPER_EXTENSION_TYPE}_DEFAULT_DIR")
    set(implementations_var "BCORE_MATTER_${HELPER_EXTENSION_TYPE}_IMPLEMENTATIONS")

    set(temp_implementations ${${implementations_var}})

    message(TRACE "Adding default ${HELPER_EXTENSION_TYPE} implementation: ${HELPER_NAME}")
    _matter_helper_append_if_not_exists(
        LIST temp_implementations
        ITEM "${${default_dir}}/${HELPER_NAME}.cpp"
    )

    set(${implementations_var} "${temp_implementations}" CACHE STRING "" FORCE)

    _matter_helper_add_extension_dependency_implementations(
        EXTENSION_TYPE ${HELPER_EXTENSION_TYPE}
        NAME ${HELPER_NAME}
    )
endfunction()

# Private
# Given a well-known dev extension implementation, adds it and any documented dependencies to
# BCORE_MATTER_<EXTENSION_TYPE>_IMPLEMENTATIONS
function(_matter_helper_add_dev_extension_implementation)
    set(oneValueArgs EXTENSION_TYPE NAME)

    cmake_parse_arguments(HELPER "" "${oneValueArgs}" "" ${ARGN})

    if (NOT HELPER_NAME)
        message(FATAL_ERROR "No name specified for _matter_helper_add_dev_extension_implementation")
    endif()

    _matter_helper_validate_extension_type(EXTENSION_TYPE ${HELPER_EXTENSION_TYPE})

    set(dev_dir "MATTER_${HELPER_EXTENSION_TYPE}_DEV_DIR")
    set(implementations_var "BCORE_MATTER_${HELPER_EXTENSION_TYPE}_IMPLEMENTATIONS")

    set(temp_implementations ${${implementations_var}})

    _matter_helper_append_if_not_exists(
        LIST temp_implementations
        ITEM "${${dev_dir}}/${HELPER_NAME}.cpp"
    )

    set(${implementations_var} "${temp_implementations}" CACHE STRING "" FORCE)

    _matter_helper_add_extension_dependency_implementations(
        EXTENSION_TYPE ${HELPER_EXTENSION_TYPE}
        NAME ${HELPER_NAME}
    )
endfunction()

# Add a header path to the list of search paths for the specified extension type.
# Usage:
# bcore_matter_helper_add_header_path(EXTENSION_TYPE "PROVIDER|DELEGATE" PATH "/Path/To/Header")
function(bcore_matter_helper_add_header_path)
    set(oneValueArgs EXTENSION_TYPE PATH)

    cmake_parse_arguments(HELPER "" "${oneValueArgs}" "" ${ARGN})

    if (NOT HELPER_PATH)
        message(FATAL_ERROR "No path specified for bcore_matter_helper_add_header_path")
    endif()

    _matter_helper_validate_extension_type(EXTENSION_TYPE ${HELPER_EXTENSION_TYPE})

    set(header_paths_var "BCORE_MATTER_${HELPER_EXTENSION_TYPE}_HEADER_PATHS")
    set(temp_header_list ${${header_paths_var}})

    _matter_helper_append_if_not_exists(
        LIST temp_header_list
        ITEM "${HELPER_PATH}"
    )

    set(${header_paths_var} "${temp_header_list}" CACHE STRING "" FORCE)
endfunction()

# Private
# Add common extension code and header paths to the public options.
function(_matter_helper_initialize)

    foreach(extension_type ${MATTER_EXTENSION_TYPES})
        set(HEADER_PATHS_VAR "MATTER_HELPER_${extension_type}_HEADER_PATHS")

        foreach(header_path IN LISTS ${HEADER_PATHS_VAR})
            bcore_matter_helper_add_header_path(
                EXTENSION_TYPE ${extension_type}
                PATH "${header_path}"
            )
        endforeach()

    endforeach()

endfunction()

# Query if a given provider is selected.
# Sets the variable specified in SELECTED to TRUE if the provider is found,
# otherwise sets it to FALSE.
# Usage:
# bcore_matter_helper_provider_selected(PROVIDER ProviderName SELECTED variable_name)
# Example:
# bcore_matter_helper_provider_selected(PROVIDER BartonTestDACProvider SELECTED isTestDACProviderSelected)
function(bcore_matter_helper_provider_selected)
    set(oneValueArgs PROVIDER SELECTED)

    cmake_parse_arguments(HELPER "" "${oneValueArgs}" "" ${ARGN})

    if (NOT HELPER_PROVIDER)
        message(FATAL_ERROR "No provider specified for bcore_matter_helper_provider_selected")
    endif()

    set(${HELPER_SELECTED} FALSE PARENT_SCOPE)

    foreach(provider IN LISTS BCORE_MATTER_PROVIDER_IMPLEMENTATIONS)
        get_filename_component(provider_name ${provider} NAME_WE)

        if (provider_name STREQUAL HELPER_PROVIDER)
            set(${HELPER_SELECTED} TRUE PARENT_SCOPE)
        endif()
    endforeach()
endfunction()

# Query if a given delegate is selected.
# Sets the variable specified in SELECTED to TRUE if the delegate is found,
# otherwise sets it to FALSE.
# Usage:
# bcore_matter_helper_delegate_selected(DELEGATE DelegateName SELECTED variable_name)
# Example:
# bcore_matter_helper_delegate_selected(DELEGATE SelfSignedCertifierOperationalCredentialsIssuer SELECTED isSelfSignedCertifierSelected)
function(bcore_matter_helper_delegate_selected)
    set(oneValueArgs DELEGATE SELECTED)

    cmake_parse_arguments(HELPER "" "${oneValueArgs}" "" ${ARGN})

    if (NOT HELPER_DELEGATE)
        message(FATAL_ERROR "No delegate specified for bcore_matter_helper_delegate_selected")
    endif()

    set(${HELPER_SELECTED} FALSE PARENT_SCOPE)

    foreach(delegate IN LISTS BCORE_MATTER_DELEGATE_IMPLEMENTATIONS)
        get_filename_component(delegate_name ${delegate} NAME_WE)

        if (delegate_name STREQUAL HELPER_DELEGATE)
            set(${HELPER_SELECTED} TRUE PARENT_SCOPE)
        endif()
    endforeach()
endfunction()

# Add one or more extension implementations to the list of matter
# entities selected.
# Dev/Default entities must be specified by name.
# Custom entities must be specified by file path.
# Any DEFAULT or DEV extension selected will automatically add its
# dependencies (class hierarchy) to the list of implementations.
#
# EXTENSION_TYPE - The type of extension (PROVIDER, DELEGATE, etc.)
# DEFAULT - Any source file located under the <extension_type_lower>s/default directory.
# DEV - Any source file located under the <extension_type_lower>s/dev directory.
# CUSTOM - Any source file located anywhere on the filesystem.
#
# Usage:
# bcore_matter_helper_add_implementation(
#     EXTENSION_TYPE "PROVIDER|DELEGATE"
#     DEFAULT "ExtensionName1" "ExtensionName2"
#     DEV "DevExtensionName1" "DevExtensionName2"
#     CUSTOM "/Path/To/CustomImplementation1.cpp" "/Path/To/CustomImplementation2.cpp"
# )
function(bcore_matter_helper_add_implementation)
    set(oneValueArgs EXTENSION_TYPE)
    set(multiValueArgs DEFAULT DEV CUSTOM)

    cmake_parse_arguments(HELPER "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    _matter_helper_validate_extension_type(EXTENSION_TYPE ${HELPER_EXTENSION_TYPE})

    if (NOT HELPER_DEFAULT AND NOT HELPER_DEV AND NOT HELPER_CUSTOM)
        message(FATAL_ERROR "No implementation specified for bcore_matter_helper_add_implementation")
    endif()

    # Process DEFAULT implementations
    foreach(extension_impl IN LISTS HELPER_DEFAULT)
        _matter_helper_add_default_extension_implementation(
            EXTENSION_TYPE ${HELPER_EXTENSION_TYPE}
            NAME ${extension_impl}
        )
    endforeach()

    # Process DEV implementations
    foreach(extension_impl IN LISTS HELPER_DEV)
        _matter_helper_add_dev_extension_implementation(
            EXTENSION_TYPE ${HELPER_EXTENSION_TYPE}
            NAME ${extension_impl}
        )
    endforeach()

    # Process CUSTOM implementations
    set(implementations_var "BCORE_MATTER_${HELPER_EXTENSION_TYPE}_IMPLEMENTATIONS")
    set(temp_implementations ${${implementations_var}})

    foreach(extension_impl IN LISTS HELPER_CUSTOM)
        _matter_helper_append_if_not_exists(
            LIST temp_implementations
            ITEM "${extension_impl}"
        )
    endforeach()

    set(${implementations_var} "${temp_implementations}" CACHE STRING "" FORCE)
endfunction()

# Get the list of implementations for the specified extension type.
# Usage:
# bcore_matter_helper_get_implementations(EXTENSION_TYPE "PROVIDER|DELEGATE" OUTPUT variable_name)
# Example:
# bcore_matter_helper_get_implementations(EXTENSION_TYPE PROVIDER OUTPUT providerImpls)
function(bcore_matter_helper_get_implementations)
    set(oneValueArgs EXTENSION_TYPE OUTPUT)

    cmake_parse_arguments(HELPER "" "${oneValueArgs}" "" ${ARGN})

    if (NOT HELPER_OUTPUT)
        message(FATAL_ERROR "No output variable specified for bcore_matter_helper_get_implementations")
    endif()

    _matter_helper_validate_extension_type(EXTENSION_TYPE ${HELPER_EXTENSION_TYPE})

    set(implementations_var "BCORE_MATTER_${HELPER_EXTENSION_TYPE}_IMPLEMENTATIONS")

    set(${HELPER_OUTPUT} "${${implementations_var}}" PARENT_SCOPE)
endfunction()

# Get the list of header paths for the specified extension type.
# Usage:
# bcore_matter_helper_get_header_paths(EXTENSION_TYPE "PROVIDER|DELEGATE" OUTPUT variable_name)
# Example:
# bcore_matter_helper_get_header_paths(EXTENSION_TYPE PROVIDER OUTPUT providerHeaderPaths)
function(bcore_matter_helper_get_header_paths)
    set(oneValueArgs EXTENSION_TYPE OUTPUT)

    cmake_parse_arguments(HELPER "" "${oneValueArgs}" "" ${ARGN})

    if (NOT HELPER_OUTPUT)
        message(FATAL_ERROR "No output variable specified for bcore_matter_helper_get_header_paths")
    endif()

    _matter_helper_validate_extension_type(EXTENSION_TYPE ${HELPER_EXTENSION_TYPE})

    set(header_paths_var "BCORE_MATTER_${HELPER_EXTENSION_TYPE}_HEADER_PATHS")

    set(${HELPER_OUTPUT} "${${header_paths_var}}" PARENT_SCOPE)
endfunction()

_matter_helper_initialize()
