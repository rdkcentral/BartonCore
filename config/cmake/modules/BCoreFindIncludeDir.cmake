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
# Created by Kevin Funderburg on 4/25/2025.
#

# This macro locates a required include directory containing a specific header
# file and adds it to a specified include list.
#
# Parameters:
#   VAR_NAME     - Name of the variable to store the found path
#   HEADER_FILE  - Header file to search for (e.g., "file.h")
#   PATH_SUFFIX  - Optional subdirectory to look in (can be empty string)
#   DESCRIPTION  - Human-readable description used in error messages
#   INCLUDE_LIST - Name of list variable to append the found path to
#
macro(bcore_find_include VAR_NAME HEADER_FILE PATH_SUFFIX DESCRIPTION INCLUDE_LIST)
    find_path(${VAR_NAME} ${HEADER_FILE} PATH_SUFFIXES ${PATH_SUFFIX})

    if (${VAR_NAME} STREQUAL "${VAR_NAME}-NOTFOUND")
        message(FATAL_ERROR "${DESCRIPTION} not found.")
    else()
        list(APPEND ${INCLUDE_LIST} ${${VAR_NAME}})
    endif()

endmacro()
