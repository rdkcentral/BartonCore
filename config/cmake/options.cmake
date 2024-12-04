#
#  Copyright (c) 2019, The OpenThread Authors.
#  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are met:
#  1. Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#  3. Neither the NAME of the copyright holder nor the
#     NAMEs of its contributors may be used to endorse or promote products
#     derived from this software without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
#  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
#  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
#  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
#  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
#  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
#  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#  POSSIBILITY OF SUCH DAMAGE.
#

# This cmake file is heavily inspired by the openthread etc/cmake/options.cmake file.
# Therefore, inheriting its license.


# Define an interface library, which will not contain source code but instead provide compile definitions
add_library(brtnDeviceServiceConfig INTERFACE)

macro(bds_option)
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
    cmake_parse_arguments(BDS_OPTION "${optionValueArgs}" "${singleValueArgs}" "${multiValueArgs}" ${ARGN})

    if (BDS_OPTION_ENABLE)
        set(${BDS_OPTION_NAME} ON CACHE BOOL "${BDS_OPTION_DESCRIPTION}")
    else()
        set(${BDS_OPTION_NAME} OFF CACHE BOOL "${BDS_OPTION_DESCRIPTION}")
    endif()

    if (${BDS_OPTION_NAME})
        message(STATUS "${BDS_OPTION_NAME}=ON --> ${BDS_OPTION_DEFINITION}=1")
        target_compile_definitions(brtnDeviceServiceConfig INTERFACE "${BDS_OPTION_DEFINITION}=1")
    else()
        message(STATUS "${BDS_OPTION_NAME}=OFF --> ${BDS_OPTION_DEFINITION} not defined")
    endif()
endmacro()

macro(bds_string_option)
    # Declare a string Barton cmake config with `NAME`
    # mapping to `DEFINITION`. Parameter `DESCRIPTION`
    # provides the help string for this Barton cmake config.
    # A compile-time definition is only created if the
    # cmake config is set to a value. A caller can optionally
    # supply `VALUE` to set the explicit initial value
    # for the cmake config. If the cmake config is already
    # set, the value does not change.

    set(singleValueArgs NAME DEFINITION DESCRIPTION VALUE)
    cmake_parse_arguments(BDS_OPTION "${optionValueArgs}" "${singleValueArgs}" "${multiValueArgs}" ${ARGN})

    if (BDS_OPTION_VALUE)
        set(${BDS_OPTION_NAME} ${BDS_OPTION_VALUE} CACHE STRING "${BDS_OPTION_DESCRIPTION}")
    endif()

    if (${BDS_OPTION_NAME})
        message(STATUS "${BDS_OPTION_NAME}=${${BDS_OPTION_NAME}} --> ${BDS_OPTION_DEFINITION}=\"${${BDS_OPTION_NAME}}\"")
        target_compile_definitions(brtnDeviceServiceConfig INTERFACE "${BDS_OPTION_DEFINITION}=\"${${BDS_OPTION_NAME}}\"")
    else()
        message(STATUS "${BDS_OPTION_NAME} unset --> ${BDS_OPTION_DEFINITION} not defined")
    endif()
endmacro()

macro(bds_int_option)
    # Declare an integer Barton cmake config with `NAME`
    # mapping to `DEFINITION`. Parameter `DESCRIPTION`
    # provides the help string for this Barton cmake config.
    # A compile-time definition is only created if the
    # cmake config is set to a value. A caller can optionally
    # supply `VALUE` to set the explicit initial value
    # for the cmake config. If the cmake config is already
    # set, the value does not change.

    set(singleValueArgs NAME DEFINITION DESCRIPTION VALUE)
    cmake_parse_arguments(BDS_OPTION "${optionValueArgs}" "${singleValueArgs}" "${multiValueArgs}" ${ARGN})

    if (BDS_OPTION_VALUE)
        set(${BDS_OPTION_NAME} ${BDS_OPTION_VALUE} CACHE STRING "${BDS_OPTION_DESCRIPTION}")
    endif()

    if (${BDS_OPTION_NAME})
        if ("${${BDS_OPTION_NAME}}" MATCHES "^[0-9]+$")
            message(STATUS "${BDS_OPTION_NAME}=${${BDS_OPTION_NAME}} --> ${BDS_OPTION_DEFINITION}=${${BDS_OPTION_NAME}}")
            target_compile_definitions(brtnDeviceServiceConfig INTERFACE "${BDS_OPTION_DEFINITION}=${${BDS_OPTION_NAME}}")
        else()
            message(FATAL_ERROR "${BDS_OPTION_NAME}=${${BDS_OPTION_NAME}} - invalid value, must be integer")
        endif()
    else()
        message(STATUS "${BDS_OPTION_NAME} unset --> ${BDS_OPTION_DEFINITION} not defined")
    endif()
endmacro()

message(STATUS "- - - - - - - - - - - - - - - - ")
message(STATUS "Barton Device Service ON/OFF Configs")

bds_option(NAME BDS_ZIGBEE
           DEFINITION BARTON_CONFIG_ZIGBEE
           DESCRIPTION "Enable Zigbee support"
           ENABLE)
bds_option(NAME BDS_THREAD
           DEFINITION BARTON_CONFIG_THREAD
           DESCRIPTION "Enable Thread support"
           ENABLE)
bds_option(NAME BDS_MATTER
           DEFINITION BARTON_CONFIG_MATTER
           DESCRIPTION "Enable Matter support"
           ENABLE)
bds_option(NAME BDS_PHILIPS_HUE
           DEFINITION BARTON_CONFIG_PHILIPS_HUE
           DESCRIPTION "Enable Philips Hue support")
bds_option(NAME BDS_GEN_GIR
           DEFINITION BARTON_CONFIG_GEN_GIR
           DESCRIPTION "Enable generating GIR and typelib information"
           ENABLE)
bds_option(NAME BDS_GENERATE_DEFAULT_LABELS
           DEFINITION BARTON_CONFIG_GENERATE_DEFAULT_LABELS
           DESCRIPTION "Enable generating default labels for devices that support the label resource")
bds_option(NAME BDS_MATTER_USE_RANDOM_PORT
           DEFINITION BARTON_CONFIG_MATTER_USE_RANDOM_PORT
           DESCRIPTION "Use a random oerational communication port for Matter. If not set, 5540 will be used.")
bds_option(NAME BDS_BUILD_REFERENCE
           DEFINITION BARTON_CONFIG_BUILD_REFERENCE
           DESCRIPTION "Build the reference application"
           ENABLE)
bds_option(NAME BDS_BUILD_WITH_SSP
           DEFINITION BARTON_CONFIG_BUILD_WITH_SSP
           DESCRIPTION "Build Barton with stack smash protection")
bds_option(NAME BDS_BUILD_WITH_ASAN
           DEFINITION BARTON_CONFIG_BUILD_WITH_ASAN
           DESCRIPTION "Build Barton with Address Sanitizer")

message(STATUS "- - - - - - - - - - - - - - - - ")

message(STATUS "- - - - - - - - - - - - - - - - ")
message(STATUS "Barton Device Service PRIVATE ON/OFF Configs. These options should not be used by new clients.")

bds_option(NAME BDS_PROVIDE_LIBS
           DEFINITION BARTON_CONFIG_PROVIDE_LIBS
           DESCRIPTION "Choose if private libraries are provided by the client.")
bds_option(NAME BDS_SUPPORT_ALARMS
           DEFINITION BARTON_CONFIG_SUPPORT_ALARMS
           DESCRIPTION "Whether alarms are supported by the client.")
bds_option(NAME BDS_MATTER_SELF_SIGNED_OP_CREDS_ISSUER
           DEFINITION BARTON_CONFIG_MATTER_SELF_SIGNED_OP_CREDS_ISSUER
           DESCRIPTION "Use a self-signed CA for the Matter operational credentials issuer.")
bds_option(NAME BDS_M1LTE
           DEFINITION BARTON_CONFIG_M1LTE
           DESCRIPTION "Indicates to Barton M1LTE is included.")
bds_option(NAME BDS_SETUP_WIZARD
           DEFINITION BARTON_CONFIG_SETUP_WIZARD
           DESCRIPTION "Support for behavioral changes if there is an \"activation period\"")
bds_option(NAME BDS_SUPPORT_SOFTWARE_WATCHDOG
           DEFINITION BARTON_CONFIG_SUPPORT_SOFTWARE_WATCHDOG
           DESCRIPTION "Support for zigbee watchdog feature using the software watchdog library.")
bds_int_option(NAME BDS_SOFTWARE_WATCHDOG_TROUBLE_CODE_ZIGBEE_CORE
              DEFINITION BARTON_CONFIG_SOFTWARE_TROUBLE_CODE_ZIGBEE_CORE_WATCHDOG
              DESCRIPTION "The trouble code to use when reporting a zigbee core failure to the software watchdog library."
              VALUE 11)
bds_option(NAME BDS_SUPPORT_ZIGBEE_TELEMETRY
           DEFINITION BARTON_CONFIG_SUPPORT_ZIGBEE_TELEMETRY
           DESCRIPTION "Support for old zigbee telemetry feature.")

message(STATUS "- - - - - - - - - - - - - - - - ")

message(STATUS "- - - - - - - - - - - - - - - - ")
message(STATUS "Barton Device Service String Configs.")

bds_string_option(NAME BDS_MATTER_LIB
                  DEFINITION BARTON_CONFIG_MATTER_LIB
                  DESCRIPTION "soname of the provided Matter library.")

message(STATUS "- - - - - - - - - - - - - - - - ")

message(STATUS "- - - - - - - - - - - - - - - - ")
message(STATUS "Barton Device Service Integer Configs.")

bds_int_option(NAME BDS_ZIGBEE_STARTUP_TIMEOUT_SECONDS
               DEFINITION BARTON_CONFIG_ZIGBEE_STARTUP_TIMEOUT_SECONDS
               DESCRIPTION "The amount of time to wait for Zigbee to startup."
               VALUE 120)

message(STATUS "- - - - - - - - - - - - - - - - ")

# Check removed/replaced options

macro(bds_removed_option NAME error)
    # This macro checks for a remove option and emits an error
    # if the option is set.
    get_property(is_set CACHE ${NAME} PROPERTY VALUE SET)
    if (is_set)
        message(FATAL_ERROR "Removed option ${NAME} is set - ${error}")
    endif()
endmacro()
