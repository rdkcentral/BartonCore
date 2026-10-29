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

# Adds a 'cppcheck' target over compile_commands.json when cppcheck is available.
# The target writes ${CMAKE_BINARY_DIR}/cppcheck.xml and does not fail the build on
# findings; scripts/quality/cppcheck.sh owns copying it into the report tree. Set BCORE_CPPCHECK_ONLY
# to a file listing source paths to restrict analysis to those translation units.
find_program(BCORE_CPPCHECK_EXE NAMES cppcheck)
find_program(BCORE_PYTHON3_EXE NAMES python3)
if (BCORE_CPPCHECK_EXE AND BCORE_PYTHON3_EXE)
    set(_bcoreCppcheckDb ${CMAKE_BINARY_DIR}/compile_commands.cppcheck.json)
    cmake_host_system_information(RESULT _bcoreCppcheckJobs QUERY NUMBER_OF_LOGICAL_CORES)
    add_custom_target(cppcheck
        COMMAND ${CMAKE_COMMAND} -E rm -f ${CMAKE_BINARY_DIR}/cppcheck.xml
        COMMAND ${BCORE_PYTHON3_EXE} ${CMAKE_CURRENT_LIST_DIR}/cppcheck_compile_db.py
            ${CMAKE_BINARY_DIR}/compile_commands.json ${_bcoreCppcheckDb}
        # cppcheck does not predefine compiler macros; without these, headers that
        # #error on unknown compilers/architectures (sbrm.h, nlbyteorder.h,
        # gstconfig.h) make it skip most translation units.
        COMMAND ${BCORE_CPPCHECK_EXE}
            -j${_bcoreCppcheckJobs}
            --project=${_bcoreCppcheckDb}
            --enable=warning,performance,portability
            --inline-suppr
            --suppressions-list=${CMAKE_SOURCE_DIR}/config/quality/cppcheck-suppressions.txt
            -D__GNUC__=13 -D__x86_64__
            -D__ORDER_LITTLE_ENDIAN__=1234 -D__ORDER_BIG_ENDIAN__=4321 -D__BYTE_ORDER__=1234
            -i${CMAKE_SOURCE_DIR}/third_party -i${CMAKE_BINARY_DIR}
            --xml --output-file=${CMAKE_BINARY_DIR}/cppcheck.xml
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running cppcheck"
        VERBATIM)
endif()
