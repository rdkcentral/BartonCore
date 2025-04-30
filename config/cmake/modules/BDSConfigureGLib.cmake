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

include_guard(GLOBAL)

function(bds_configure_glib)
    get_directory_property(MY_TARGETS BUILDSYSTEM_TARGETS)
    include(FindPkgConfig)

    pkg_check_modules(GIO REQUIRED glib-2.0>=${GLIB_MIN_VERSION} gio-2.0>=${GLIB_MIN_VERSION} gio-unix-2.0>=${GLIB_MIN_VERSION})

    if (${GIO_glib-2.0_VERSION} VERSION_LESS_EQUAL 2.66)
        pkg_check_modules(GLIB_POLYFILL REQUIRED glib-2.0-polyfill)

        list(APPEND GIO_INCLUDE_DIRS ${GLIB_POLYFILL_INCLUDE_DIRS})
        list(APPEND GIO_LINK_LIBRARIES ${GLIB_POLYFILL_LINK_LIBRARIES})
        list(APPEND XTRA_DEFINES HAVE_GLIB_POLYFILL)
    endif()

    foreach(MY_TARGET ${MY_TARGETS})
        message(STATUS "enabling glib for ${MY_TARGET}")
        set_property(TARGET ${MY_TARGET} APPEND PROPERTY INCLUDE_DIRECTORIES ${GIO_INCLUDE_DIRS})
        set_property(TARGET ${MY_TARGET} APPEND PROPERTY LINK_LIBRARIES ${GIO_LINK_LIBRARIES})
        set_property(TARGET ${MY_TARGET} APPEND PROPERTY INTERFACE_LINK_LIBRARIES ${GIO_LINK_LIBRARIES})
        set_property(TARGET ${MY_TARGET} APPEND PROPERTY COMPILE_DEFINITIONS ${XTRA_DEFINES})
    endforeach()
endfunction()
