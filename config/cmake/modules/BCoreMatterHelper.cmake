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

# Module may be included from a cmake file that hasn't called `project` yet (likely an initial cache)
# Figure out the path relative to the current file as that works in all cases.
get_filename_component(MATTER_PROVIDER_DELEGATE_PARENT_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../core/src/subsystems/matter" ABSOLUTE)

set(MATTER_PROVIDER_DEFAULT_DIR "${MATTER_PROVIDER_DELEGATE_PARENT_DIR}/providers/default")
set(MATTER_PROVIDER_DEV_DIR "${MATTER_PROVIDER_DELEGATE_PARENT_DIR}/providers/dev")
set(MATTER_DELEGATE_DEFAULT_DIR "${MATTER_PROVIDER_DELEGATE_PARENT_DIR}/delegates/default")
set(MATTER_DELEGATE_DEV_DIR "${MATTER_PROVIDER_DELEGATE_PARENT_DIR}/delegates/dev")

# Custom associative dictionary for tracking class hiearchy dependencies of well-known
# providers/delegates. Format for individial list items is:
# "ProviderName:DependencyFilePath[,DependencyFilePath,...]"
set(MATTER_HELPER_PROVIDER_DEPENDENCIES
    "DefaultCommissionableDataProvider:${MATTER_PROVIDER_DELEGATE_PARENT_DIR}/providers/BartonCommissionableDataProvider.cpp"
)

set(MATTER_HELPER_DELEGATE_DEPENDENCIES
)

set(MATTER_HELPER_PROVIDER_HEADER_PATHS
    "${MATTER_PROVIDER_DELEGATE_PARENT_DIR}/providers"
    "${MATTER_PROVIDER_DEFAULT_DIR}"
    "${MATTER_PROVIDER_DEV_DIR}"
)

set(MATTER_HELPER_DELEGATE_HEADER_PATHS
    "${MATTER_PROVIDER_DELEGATE_PARENT_DIR}/delegates/"
    "${MATTER_DELEGATE_DEFAULT_DIR}"
    "${MATTER_DELEGATE_DEV_DIR}"
)

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
# Given a well-known provider, adds well-known dependencies documented in
# MATTER_HELPER_PROVIDER_DEPENDENCIES to BCORE_MATTER_PROVIDER_IMPLEMENTATIONS
function(_matter_helper_add_provider_dependency_implementations)
    set(oneValueArgs NAME)

    cmake_parse_arguments(HELPER "" "${oneValueArgs}" "" ${ARGN})

    if (NOT HELPER_NAME)
        message(FATAL_ERROR "No name specified for _matter_helper_add_provider_dependency_implementations")
    endif()

    set(TEMP_PROVIDER_IMPLEMENTATIONS ${BCORE_MATTER_PROVIDER_IMPLEMENTATIONS})

    foreach(dependency_association IN LISTS MATTER_HELPER_PROVIDER_DEPENDENCIES)
        string(FIND "${dependency_association}" ":" char_index)
        string(SUBSTRING "${dependency_association}" 0 ${char_index} dependency_name)
        math(EXPR char_index "${char_index} + 1")
        string(SUBSTRING "${dependency_association}" ${char_index} -1 dependency_list)
        string(REPLACE "," ";" dependency_list "${dependency_list}")

        if (dependency_name STREQUAL HELPER_NAME)
            foreach(dependency IN LISTS dependency_list)
                message(TRACE "Adding provider dependency implementation: ${dependency}")
                _matter_helper_append_if_not_exists(
                    LIST TEMP_PROVIDER_IMPLEMENTATIONS
                    ITEM "${dependency}"
                )
            endforeach()
        endif()
    endforeach()

    set(BCORE_MATTER_PROVIDER_IMPLEMENTATIONS "${TEMP_PROVIDER_IMPLEMENTATIONS}" CACHE STRING "" FORCE)
endfunction()

# Private
# Given a well-known default provider, adds it and any documented dependencies to
# BCORE_MATTER_PROVIDER_IMPLEMENTATIONS
function(_matter_helper_add_default_provider_implementation)
    set(oneValueArgs NAME)

    cmake_parse_arguments(HELPER "" "${oneValueArgs}" "" ${ARGN})

    if (NOT HELPER_NAME)
        message(FATAL_ERROR "No name specified for _matter_helper_add_default_provider_implementation")
    endif()

    set(TEMP_PROVIDER_IMPLEMENTATIONS ${BCORE_MATTER_PROVIDER_IMPLEMENTATIONS})

    message(TRACE "Adding default provider implementation: ${HELPER_NAME}")
    _matter_helper_append_if_not_exists(
        LIST TEMP_PROVIDER_IMPLEMENTATIONS
        ITEM "${MATTER_PROVIDER_DEFAULT_DIR}/${HELPER_NAME}.cpp"
    )

    set(BCORE_MATTER_PROVIDER_IMPLEMENTATIONS "${TEMP_PROVIDER_IMPLEMENTATIONS}" CACHE STRING "" FORCE)

    _matter_helper_add_provider_dependency_implementations(NAME ${HELPER_NAME})
endfunction()

# Private
# Given a well-known dev provider, adds it and any documented dependencies to
# BCORE_MATTER_PROVIDER_IMPLEMENTATIONS
function(_matter_helper_add_dev_provider_implementation)
    set(oneValueArgs NAME)

    cmake_parse_arguments(HELPER "" "${oneValueArgs}" "" ${ARGN})

    if (NOT HELPER_NAME)
        message(FATAL_ERROR "No name specified for _matter_helper_add_dev_provider_implementation")
    endif()

    set(TEMP_PROVIDER_IMPLEMENTATIONS ${BCORE_MATTER_PROVIDER_IMPLEMENTATIONS})

    message(TRACE "Adding dev provider implementation: ${HELPER_NAME}")
    _matter_helper_append_if_not_exists(
        LIST TEMP_PROVIDER_IMPLEMENTATIONS
        ITEM "${MATTER_PROVIDER_DEV_DIR}/${HELPER_NAME}.cpp"
    )

    set(BCORE_MATTER_PROVIDER_IMPLEMENTATIONS "${TEMP_PROVIDER_IMPLEMENTATIONS}" CACHE STRING "" FORCE)

    _matter_helper_add_provider_dependency_implementations(NAME ${HELPER_NAME})
endfunction()

# Private
# Given a well-known delegate, adds well-known dependencies documented in
# MATTER_HELPER_DELEGATE_DEPENDENCIES to BCORE_MATTER_DELEGATE_IMPLEMENTATIONS
function(_matter_helper_add_delegate_dependency_implementations)
    set(oneValueArgs NAME)

    cmake_parse_arguments(HELPER "" "${oneValueArgs}" "" ${ARGN})

    if (NOT HELPER_NAME)
        message(FATAL_ERROR "No name specified for _matter_helper_add_delegate_dependency_implementations")
    endif()

    set(TEMP_DELEGATE_IMPLEMENTATIONS ${BCORE_MATTER_DELEGATE_IMPLEMENTATIONS})

    foreach(dependency_association IN LISTS MATTER_HELPER_DELEGATE_DEPENDENCIES)
        string(FIND "${dependency_association}" ":" char_index)
        string(SUBSTRING "${dependency_association}" 0 ${char_index} dependency_name)
        math(EXPR char_index "${char_index} + 1")
        string(SUBSTRING "${dependency_association}" ${char_index} -1 dependency_list)
        string(REPLACE "," ";" dependency_list "${dependency_list}")

        if (dependency_name STREQUAL HELPER_NAME)
            foreach(dependency IN LISTS dependency_list)
                message(TRACE "Adding delegate dependency implementation: ${dependency}")
                _matter_helper_append_if_not_exists(
                    LIST BCORE_MATTER_DELEGATE_IMPLEMENTATIONS
                    ITEM "${dependency}"
                )
            endforeach()
        endif()
    endforeach()

    set(BCORE_MATTER_DELEGATE_IMPLEMENTATIONS "${TEMP_DELEGATE_IMPLEMENTATIONS}" CACHE STRING "" FORCE)
endfunction()

# Private
# Given a well-known default delegate, adds it and any documented dependencies to
# BCORE_MATTER_DELEGATE_IMPLEMENTATIONS
function(_matter_helper_add_default_delegate_implementation)
    set(oneValueArgs NAME)

    cmake_parse_arguments(HELPER "" "${oneValueArgs}" "" ${ARGN})

    if (NOT HELPER_NAME)
        message(FATAL_ERROR "No name specified for _matter_helper_add_default_delegate_implementation")
    endif()

    set(TEMP_DELEGATE_IMPLEMENTATIONS ${BCORE_MATTER_DELEGATE_IMPLEMENTATIONS})

    message(TRACE "Adding default delegate implementation: ${HELPER_NAME}")
    _matter_helper_append_if_not_exists(
        LIST TEMP_DELEGATE_IMPLEMENTATIONS
        ITEM "${MATTER_DELEGATE_DEFAULT_DIR}/${HELPER_NAME}.cpp"
    )

    set(BCORE_MATTER_DELEGATE_IMPLEMENTATIONS "${TEMP_DELEGATE_IMPLEMENTATIONS}" CACHE STRING "" FORCE)

    _matter_helper_add_delegate_dependency_implementations(NAME ${HELPER_NAME})
endfunction()

# Private
# Given a well-known dev delegate, adds it and any documented dependencies to
# BCORE_MATTER_DELEGATE_IMPLEMENTATIONS
function(_matter_helper_add_dev_delegate_implementation)
    set(oneValueArgs NAME)

    cmake_parse_arguments(HELPER "" "${oneValueArgs}" "" ${ARGN})

    if (NOT HELPER_NAME)
        message(FATAL_ERROR "No name specified for _matter_helper_add_dev_delegate_implementation")
    endif()

    set(TEMP_DELEGATE_IMPLEMENTATIONS ${BCORE_MATTER_DELEGATE_IMPLEMENTATIONS})

    message(TRACE "Adding dev delegate implementation: ${HELPER_NAME}")
    _matter_helper_append_if_not_exists(
        LIST TEMP_DELEGATE_IMPLEMENTATIONS
        ITEM "${MATTER_DELEGATE_DEV_DIR}/${HELPER_NAME}.cpp"
    )

    set(BCORE_MATTER_DELEGATE_IMPLEMENTATIONS "${TEMP_DELEGATE_IMPLEMENTATIONS}" CACHE STRING "" FORCE)

    _matter_helper_add_delegate_dependency_implementations(NAME ${HELPER_NAME})
endfunction()

# Private
# Add common provider/delegate code and header paths to the public options.
function(_matter_helper_initialize)
    foreach(header_path IN LISTS MATTER_HELPER_PROVIDER_HEADER_PATHS)
        bcore_matter_helper_provider_add_header_path(
            PATH "${header_path}"
        )
    endforeach()

    foreach(header_path IN LISTS MATTER_HELPER_DELEGATE_HEADER_PATHS)
        bcore_matter_helper_delegate_add_header_path(
            PATH "${header_path}"
        )
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

# Add one or more provider implementations to the list of matter
# providers selected.
# Dev/Default providers must be specified by name.
# Custom providers must be specified by file path.
# Any DEFAULT or DEV provider selected will automatically add its
# dependencies (class hierarchy) to the list of implementations.
#
# DEFAULT - Any source file that is located under the providers/default directory.
# DEV - Any source file that is located under the providers/dev directory.
# CUSTOM - Any source file that is located anywhere on the filesystem.
#
# Usage:
# bcore_matter_helper_provider_add_implementation(
#     DEFAULT "ProviderName1" "ProviderName2"
#     DEV "DevProviderName1" "DevProviderName2"
#     CUSTOM "/Path/To/CustomProviderImplementation1.cpp" "/Path/To/CustomProviderImplementation2.cpp"
# )
function(bcore_matter_helper_provider_add_implementation)
    set(multiValueArgs DEFAULT DEV CUSTOM)

    cmake_parse_arguments(HELPER "" "" "${multiValueArgs}" ${ARGN})

    if (NOT HELPER_DEFAULT AND NOT HELPER_DEV AND NOT HELPER_CUSTOM)
        message(FATAL_ERROR "No implementation specified for bcore_matter_helper_provider_add_implementation")
    endif()

    foreach(provider_impl IN LISTS HELPER_DEFAULT)
        _matter_helper_add_default_provider_implementation(NAME ${provider_impl})
    endforeach()

    foreach(provider_impl IN LISTS HELPER_DEV)
        _matter_helper_add_dev_provider_implementation(NAME ${provider_impl})
    endforeach()

    set(TEMP_PROVIDER_IMPLEMENTATIONS ${BCORE_MATTER_PROVIDER_IMPLEMENTATIONS})

    foreach(provider_impl IN LISTS HELPER_CUSTOM)
        _matter_helper_append_if_not_exists(
            LIST TEMP_PROVIDER_IMPLEMENTATIONS
            ITEM "${provider_impl}"
        )
    endforeach()

    set(BCORE_MATTER_PROVIDER_IMPLEMENTATIONS "${TEMP_PROVIDER_IMPLEMENTATIONS}" CACHE STRING "" FORCE)
endfunction()

# Add one or more delegate implementations to the list of matter
# delegates selected.
# Dev/Default delegates must be specified by name.
# Custom delegates must be specified by file path.
# Any DEFAULT or DEV delegate selected will automatically add its
# dependencies (class hierarchy) to the list of implementations.
#
# DEFAULT - Any source file that is located under the delegates/default directory.
# DEV - Any source file that is located under the delegates/dev directory.
# CUSTOM - Any source file that is located anywhere on the filesystem.
#
# Usage:
# bcore_matter_helper_delegate_add_implementation(
#     DEFAULT "DelegateName1" "DelegateName2"
#     DEV "DevDelegateName1" "DevDelegateName2"
#     CUSTOM "/Path/To/CustomDelegateImplementation1.cpp" "/Path/To/CustomDelegateImplementation2.cpp"
# )
function(bcore_matter_helper_delegate_add_implementation)
    set(multiValueArgs DEFAULT DEV CUSTOM)

    cmake_parse_arguments(HELPER "" "" "${multiValueArgs}" ${ARGN})

    if (NOT HELPER_DEFAULT AND NOT HELPER_DEV AND NOT HELPER_CUSTOM)
        message(FATAL_ERROR "No implementation specified for bcore_matter_helper_delegate_add_implementation")
    endif()

    foreach(delegate_impl IN LISTS HELPER_DEFAULT)
        _matter_helper_add_default_delegate_implementation(NAME ${delegate_impl})
    endforeach()

    foreach(delegate_impl IN LISTS HELPER_DEV)
        _matter_helper_add_dev_delegate_implementation(NAME ${delegate_impl})
    endforeach()

    set(TEMP_DELEGATE_IMPLEMENTATIONS ${BCORE_MATTER_DELEGATE_IMPLEMENTATIONS})

    foreach(delegate_impl IN LISTS HELPER_CUSTOM)
        _matter_helper_append_if_not_exists(
            LIST TEMP_DELEGATE_IMPLEMENTATIONS
            ITEM "${delegate_impl}"
        )
    endforeach()

    set(BCORE_MATTER_DELEGATE_IMPLEMENTATIONS "${TEMP_DELEGATE_IMPLEMENTATIONS}" CACHE STRING "" FORCE)
endfunction()

# Add a header path to the list of search paths for
# provider headers.
# Usage:
# bcore_matter_helper_provider_add_header_path(PATH "/Path/To/Header")
function(bcore_matter_helper_provider_add_header_path)
    set(oneValueArgs PATH)

    cmake_parse_arguments(HELPER "" "${oneValueArgs}" "" ${ARGN})

    if (NOT HELPER_PATH)
        message(FATAL_ERROR "No path specified for bcore_matter_helper_provider_add_header_path")
    endif()

    set(TEMP_PROVIDER_HEADER_LIST ${BCORE_MATTER_PROVIDER_HEADER_PATHS})

    _matter_helper_append_if_not_exists(
        LIST TEMP_PROVIDER_HEADER_LIST
        ITEM "${HELPER_PATH}"
    )

    set(BCORE_MATTER_PROVIDER_HEADER_PATHS "${TEMP_PROVIDER_HEADER_LIST}" CACHE STRING "" FORCE)
endfunction()

# Add a header path to the list of search paths for
# delegate headers.
# Usage:
# bcore_matter_helper_delegate_add_header_path(PATH "/Path/To/Header")
function(bcore_matter_helper_delegate_add_header_path)
    set(oneValueArgs PATH)

    cmake_parse_arguments(HELPER "" "${oneValueArgs}" "" ${ARGN})

    if (NOT HELPER_PATH)
        message(FATAL_ERROR "No path specified for bcore_matter_helper_delegate_add_header_path")
    endif()

    set(TEMP_DELEGATE_HEADER_LIST ${BCORE_MATTER_DELEGATE_HEADER_PATHS})

    _matter_helper_append_if_not_exists(
        LIST TEMP_DELEGATE_HEADER_LIST
        ITEM "${HELPER_PATH}"
    )

    set(BCORE_MATTER_DELEGATE_HEADER_PATHS "${TEMP_DELEGATE_HEADER_LIST}" CACHE STRING "" FORCE)
endfunction()

# Get the list of provider implementations.
# Usage:
# bcore_matter_helper_provider_get_implementations(OUTPUT variable_name)
function(bcore_matter_helper_provider_get_implementations)
    set(oneValueArgs OUTPUT)

    cmake_parse_arguments(HELPER "" "${oneValueArgs}" "" ${ARGN})

    if (NOT HELPER_OUTPUT)
        message(FATAL_ERROR "No output variable specified for bcore_matter_helper_provider_get_implementations")
    endif()

    set(${HELPER_OUTPUT} "${BCORE_MATTER_PROVIDER_IMPLEMENTATIONS}" PARENT_SCOPE)
endfunction()

# Get the list of delegate implementations.
# Usage:
# bcore_matter_helper_delegate_get_implementations(OUTPUT variable_name)
function(bcore_matter_helper_delegate_get_implementations)
    set(oneValueArgs OUTPUT)

    cmake_parse_arguments(HELPER "" "${oneValueArgs}" "" ${ARGN})

    if (NOT HELPER_OUTPUT)
        message(FATAL_ERROR "No output variable specified for bcore_matter_helper_delegate_get_implementations")
    endif()

    set(${HELPER_OUTPUT} "${BCORE_MATTER_DELEGATE_IMPLEMENTATIONS}" PARENT_SCOPE)
endfunction()

# Get the list of header paths for provider headers.
# Usage:
# bcore_matter_helper_provider_get_header_paths(OUTPUT variable_name)
function(bcore_matter_helper_provider_get_header_paths)
    set(oneValueArgs OUTPUT)

    cmake_parse_arguments(HELPER "" "${oneValueArgs}" "" ${ARGN})

    if (NOT HELPER_OUTPUT)
        message(FATAL_ERROR "No output variable specified for bcore_matter_helper_provider_get_header_paths")
    endif()

    set(${HELPER_OUTPUT} "${BCORE_MATTER_PROVIDER_HEADER_PATHS}" PARENT_SCOPE)
endfunction()

# Get the list of header paths for delegate headers.
# Usage:
# bcore_matter_helper_delegate_get_header_paths(OUTPUT variable_name)
function(bcore_matter_helper_delegate_get_header_paths)
    set(oneValueArgs OUTPUT)

    cmake_parse_arguments(HELPER "" "${oneValueArgs}" "" ${ARGN})

    if (NOT HELPER_OUTPUT)
        message(FATAL_ERROR "No output variable specified for bcore_matter_helper_delegate_get_header_paths")
    endif()

    set(${HELPER_OUTPUT} "${BCORE_MATTER_DELEGATE_HEADER_PATHS}" PARENT_SCOPE)
endfunction()

_matter_helper_initialize()
