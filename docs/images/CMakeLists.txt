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

cmake_minimum_required(VERSION 3.10)
project(barton-docs NONE)

find_program(PLANTUML_EXEC plantuml)

if(NOT PLANTUML_EXEC)
    message(FATAL_ERROR "plantuml not found in PATH")
endif()

file(GLOB PUML_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.puml")

# Create a list of expected SVG output files
set(SVG_FILES)
foreach(PUML_FILE ${PUML_FILES})
    get_filename_component(BASENAME ${PUML_FILE} NAME_WE)
    list(APPEND SVG_FILES "${CMAKE_CURRENT_SOURCE_DIR}/${BASENAME}.svg")
endforeach()

if(PUML_FILES)
    add_custom_command(
        OUTPUT ${SVG_FILES}
        COMMAND ${CMAKE_COMMAND} -E env "DISPLAY=" ${PLANTUML_EXEC} -tsvg ${PUML_FILES}
        DEPENDS ${PUML_FILES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating PlantUML SVG diagrams from all .puml files"
    )

    add_custom_target(generate-doc-diagrams ALL
        DEPENDS ${SVG_FILES}
    )
endif()
