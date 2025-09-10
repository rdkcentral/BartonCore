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

# Code adapted from the openthread etc/cmake/options.cmake file
# Copyright (c) 2019, The OpenThread Authors.
# All rights reserved.
# Licensed under the BSD-3 License

# Define an interface library, which will not contain source code but instead provide compile definitions
add_library(bCoreConfig INTERFACE)

function(bcore_option)
    # Declare an (ON/OFF) Barton cmake config with `NAME`
    # mapping to `DEFINITION`. Parameter `DESCRIPTION`
    # provides the help string for this Barton cmake config.
    # A compile-time definition is only created if the
    # cmake config is set to ON. A caller can optionally
    # supply `ENABLE` to set the explicit initial value (ON)
    # for the cmake config. Otherwise, the initial value is
    # OFF. If the cmake config is already set, the value does
    # not change.

    set(optionValueArgs ENABLE DISABLE)
    set(singleValueArgs NAME DEFINITION DESCRIPTION)
    cmake_parse_arguments(BCORE_OPTION "${optionValueArgs}" "${singleValueArgs}" "${multiValueArgs}" ${ARGN})

    if (BCORE_OPTION_ENABLE)
        set(${BCORE_OPTION_NAME} ON CACHE BOOL "${BCORE_OPTION_DESCRIPTION}")
    else()
        set(${BCORE_OPTION_NAME} OFF CACHE BOOL "${BCORE_OPTION_DESCRIPTION}")
    endif()

    if (${BCORE_OPTION_NAME})
        message(STATUS "${BCORE_OPTION_NAME}=ON --> ${BCORE_OPTION_DEFINITION}=1")
        target_compile_definitions(bCoreConfig INTERFACE "${BCORE_OPTION_DEFINITION}=1")
    else()
        message(STATUS "${BCORE_OPTION_NAME}=OFF --> ${BCORE_OPTION_DEFINITION} not defined")
    endif()
endfunction()

function(bcore_string_option)
    # Declare a string Barton cmake config with `NAME`
    # mapping to `DEFINITION`. Parameter `DESCRIPTION`
    # provides the help string for this Barton cmake config.
    # A compile-time definition is only created if the
    # cmake config is set to a value. A caller can optionally
    # supply `VALUE` to set the explicit initial value
    # for the cmake config. If the cmake config is already
    # set, the value does not change.

    set(singleValueArgs NAME DEFINITION DESCRIPTION)
    set(multiValueArgs VALUE)
    cmake_parse_arguments(BCORE_OPTION "${optionValueArgs}" "${singleValueArgs}" "${multiValueArgs}" ${ARGN})

    if (BCORE_OPTION_VALUE)
        set(${BCORE_OPTION_NAME} ${BCORE_OPTION_VALUE} CACHE STRING "${BCORE_OPTION_DESCRIPTION}")
    endif()

    if (${BCORE_OPTION_NAME})
        # If the value of ${${BCORE_OPTION_NAME}} is a list, join it with escaped semi-colons to ensure
        # COMPILER_DEFINITIONS (https://cmake.org/cmake/help/latest/prop_tgt/COMPILE_DEFINITIONS.html)
        # property does not improperly malform the definition. Single strings (non-lists) will be
        # unaffected by this call.
        string(JOIN "\;" JOINED_OPTION ${${BCORE_OPTION_NAME}})
        message(STATUS "${BCORE_OPTION_NAME}=${${BCORE_OPTION_NAME}} --> ${BCORE_OPTION_DEFINITION}=\"${JOINED_OPTION}\"")
        target_compile_definitions(bCoreConfig INTERFACE "${BCORE_OPTION_DEFINITION}=\"${JOINED_OPTION}\"")
    else()
        message(STATUS "${BCORE_OPTION_NAME} unset --> ${BCORE_OPTION_DEFINITION} not defined")
    endif()
endfunction()

function(bcore_int_option)
    # Declare an integer Barton cmake config with `NAME`
    # mapping to `DEFINITION`. Parameter `DESCRIPTION`
    # provides the help string for this Barton cmake config.
    # A compile-time definition is only created if the
    # cmake config is set to a value. A caller can optionally
    # supply `VALUE` to set the explicit initial value
    # for the cmake config. If the cmake config is already
    # set, the value does not change.

    set(singleValueArgs NAME DEFINITION DESCRIPTION VALUE)
    cmake_parse_arguments(BCORE_OPTION "${optionValueArgs}" "${singleValueArgs}" "${multiValueArgs}" ${ARGN})

    if (BCORE_OPTION_VALUE)
        set(${BCORE_OPTION_NAME} ${BCORE_OPTION_VALUE} CACHE STRING "${BCORE_OPTION_DESCRIPTION}")
    endif()

    if (${BCORE_OPTION_NAME})
        if ("${${BCORE_OPTION_NAME}}" MATCHES "^[0-9]+$")
            message(STATUS "${BCORE_OPTION_NAME}=${${BCORE_OPTION_NAME}} --> ${BCORE_OPTION_DEFINITION}=${${BCORE_OPTION_NAME}}")
            target_compile_definitions(bCoreConfig INTERFACE "${BCORE_OPTION_DEFINITION}=${${BCORE_OPTION_NAME}}")
        else()
            message(FATAL_ERROR "${BCORE_OPTION_NAME}=${${BCORE_OPTION_NAME}} - invalid value, must be integer")
        endif()
    else()
        message(STATUS "${BCORE_OPTION_NAME} unset --> ${BCORE_OPTION_DEFINITION} not defined")
    endif()
endfunction()

message(STATUS "- - - - - - - - - - - - - - - - ")
message(STATUS "Barton Core ON/OFF Configs")

bcore_option(NAME BCORE_ZIGBEE
           DEFINITION BARTON_CONFIG_ZIGBEE
           DESCRIPTION "Enable Zigbee support"
           ENABLE)
bcore_option(NAME BCORE_THREAD
           DEFINITION BARTON_CONFIG_THREAD
           DESCRIPTION "Enable Thread support"
           ENABLE)
bcore_option(NAME BCORE_MATTER
           DEFINITION BARTON_CONFIG_MATTER
           DESCRIPTION "Enable Matter support"
           ENABLE)
bcore_option(NAME BCORE_PHILIPS_HUE
           DEFINITION BARTON_CONFIG_PHILIPS_HUE
           DESCRIPTION "Enable Philips Hue support")
bcore_option(NAME BCORE_GEN_GIR
           DEFINITION BARTON_CONFIG_GEN_GIR
           DESCRIPTION "Enable generating GIR and typelib information")
bcore_option(NAME BCORE_GENERATE_DEFAULT_LABELS
           DEFINITION BARTON_CONFIG_GENERATE_DEFAULT_LABELS
           DESCRIPTION "Enable generating default labels for devices that support the label resource")
bcore_option(NAME BCORE_MATTER_USE_RANDOM_PORT
           DEFINITION BARTON_CONFIG_MATTER_USE_RANDOM_PORT
           DESCRIPTION "Use a random oerational communication port for Matter. If not set, 5540 will be used.")
bcore_option(NAME BCORE_BUILD_REFERENCE
           DEFINITION BARTON_CONFIG_BUILD_REFERENCE
           DESCRIPTION "Build the reference application"
           ENABLE)
bcore_option(NAME BCORE_BUILD_WITH_SSP
           DEFINITION BARTON_CONFIG_BUILD_WITH_SSP
           DESCRIPTION "Build Barton with stack smash protection")
bcore_option(NAME BCORE_BUILD_WITH_ASAN
           DEFINITION BARTON_CONFIG_BUILD_WITH_ASAN
           DESCRIPTION "Build Barton with Address Sanitizer")
bcore_option(NAME BCORE_TEST_COVERAGE
           DEFINITION BARTON_CONFIG_TEST_COVERAGE
           DESCRIPTION "Enable test coverage")
bcore_option(NAME BCORE_MATTER_SKIP_SDK
           DEFINITION BARTON_CONFIG_MATTER_SKIP_SDK
           DESCRIPTION "When building Matter, skip building the SDK (as a client may be building it separately already)")
bcore_option(NAME BCORE_MATTER_USE_DEFAULT_COMMISSIONABLE_DATA
           DEFINITION BARTON_CONFIG_USE_DEFAULT_COMMISSIONABLE_DATA
           DESCRIPTION "Use default commissionable data values instead of ones provided by the client")

message(STATUS "- - - - - - - - - - - - - - - - ")

message(STATUS "- - - - - - - - - - - - - - - - ")
message(STATUS "Barton Core PRIVATE ON/OFF Configs. These options should not be used by new clients.")

bcore_option(NAME BCORE_PROVIDE_LIBS
           DEFINITION BARTON_CONFIG_PROVIDE_LIBS
           DESCRIPTION "Choose if private libraries are provided by the client.")
bcore_option(NAME BCORE_SUPPORT_ALARMS
           DEFINITION BARTON_CONFIG_SUPPORT_ALARMS
           DESCRIPTION "Whether alarms are supported by the client.")
bcore_option(NAME BCORE_M1LTE
           DEFINITION BARTON_CONFIG_M1LTE
           DESCRIPTION "Indicates to Barton M1LTE is included.")
bcore_option(NAME BCORE_SETUP_WIZARD
           DEFINITION BARTON_CONFIG_SETUP_WIZARD
           DESCRIPTION "Support for behavioral changes if there is an \"activation period\"")
bcore_option(NAME BCORE_SUPPORT_ZIGBEE_TELEMETRY
           DEFINITION BARTON_CONFIG_SUPPORT_ZIGBEE_TELEMETRY
           DESCRIPTION "Support for old zigbee telemetry feature.")

message(STATUS "- - - - - - - - - - - - - - - - ")

message(STATUS "- - - - - - - - - - - - - - - - ")
message(STATUS "Barton Core String Configs.")

bcore_string_option(NAME BCORE_MATTER_LIB
                  DEFINITION BARTON_CONFIG_MATTER_LIB
                  DESCRIPTION "Name of the provided Matter library."
                  VALUE "BartonMatter")

# We're not going to use BCoreMatterHelper to set up defaults (as BCoreMatterHelper applies FORCE
# sets). However, include it to populate default header paths.
# Default selected implementations still need to be set up in below lists.
include(BCoreMatterHelper)

set(MATTER_PROVIDER_DELEGATE_PARENT_DIR "${PROJECT_SOURCE_DIR}/core/src/subsystems/matter")
set(MATTER_PROVIDER_DEFAULT_DIR "${MATTER_PROVIDER_DELEGATE_PARENT_DIR}/providers/default")
set(MATTER_DELEGATE_DEFAULT_DIR "${MATTER_PROVIDER_DELEGATE_PARENT_DIR}/delegates/default")

bcore_string_option(NAME BCORE_MATTER_PROVIDER_IMPLEMENTATIONS
                  DEFINITION BARTON_CONFIG_MATTER_PROVIDER_IMPLEMENTATIONS
                  DESCRIPTION "List of paths to source files that implement Matter provider interfaces."
                  VALUE "${MATTER_PROVIDER_DEFAULT_DIR}/BartonTestDACProvider.cpp"
                        "${MATTER_PROVIDER_DELEGATE_PARENT_DIR}/providers/BartonCommissionableDataProvider.cpp"
                        "${MATTER_PROVIDER_DEFAULT_DIR}/DefaultCommissionableDataProvider.cpp")

bcore_string_option(NAME BCORE_MATTER_DELEGATE_IMPLEMENTATIONS
                  DEFINITION BARTON_CONFIG_MATTER_DELEGATE_IMPLEMENTATIONS
                  DESCRIPTION "List of paths to source files that implement Matter delegate interfaces."
                  VALUE "${MATTER_DELEGATE_DEFAULT_DIR}/CertifierOperationalCredentialsIssuer.cpp")

bcore_string_option(NAME BCORE_MATTER_PROVIDER_HEADER_PATHS
                  DEFINITION BARTON_CONFIG_MATTER_PROVIDER_HEADER_PATHS
                  DESCRIPTION "List of paths to directories containing matter provider header files.")

bcore_string_option(NAME BCORE_MATTER_DELEGATE_HEADER_PATHS
                  DEFINITION BARTON_CONFIG_MATTER_DELEGATE_HEADER_PATHS
                  DESCRIPTION "List of paths to directories containing matter delegate header files")

bcore_string_option(NAME BCORE_MATTER_BLE_CONTROLLER_DEVICE_NAME
                    DEFINITION BARTON_CONFIG_MATTER_BLE_CONTROLLER_DEVICE_NAME
                    DESCRIPTION "Name of the Matter BLE controller device."
                    VALUE "Matter-Controller")

bcore_string_option(NAME BCORE_LINK_LIBRARIES
                  DEFINITION BARTON_CONFIG_LINK_LIBRARIES
                  DESCRIPTION "List of additional libraries to link against when building the Barton Core. This allows custom client code to depend on external libraries as well.")

message(STATUS "- - - - - - - - - - - - - - - - ")

message(STATUS "- - - - - - - - - - - - - - - - ")
message(STATUS "Barton Core Integer Configs.")

bcore_int_option(NAME BCORE_ZIGBEE_STARTUP_TIMEOUT_SECONDS
               DEFINITION BARTON_CONFIG_ZIGBEE_STARTUP_TIMEOUT_SECONDS
               DESCRIPTION "The amount of time to wait for Zigbee to startup."
               VALUE 120)

message(STATUS "- - - - - - - - - - - - - - - - ")

# Check removed/replaced options

macro(bcore_removed_option NAME error)
    # This macro checks for a remove option and emits an error
    # if the option is set.
    get_property(is_set CACHE ${NAME} PROPERTY VALUE SET)
    if (is_set)
        message(FATAL_ERROR "Removed option ${NAME} is set - ${error}")
    endif()
endmacro()
