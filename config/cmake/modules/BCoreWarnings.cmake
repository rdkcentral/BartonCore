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

# Warning flags for first-party targets only. Link with
# $<BUILD_INTERFACE:bCoreWarnings> so third_party/ and the Matter SDK are
# unaffected and installed targets do not reference it.
#
# BCORE_EXTRA_WARNINGS is private and temporary: consumers that inject -Werror
# (e.g. zilker) would fail on existing -Wextra findings, so the flags are opt-in
# (enabled by the dev platform file and presets) until those findings are fixed.
# Remove the option and make the flags unconditional at that point.
option(BCORE_EXTRA_WARNINGS "Internal: enable -Wall -Wextra on first-party targets" OFF)
mark_as_advanced(BCORE_EXTRA_WARNINGS)
add_library(bCoreWarnings INTERFACE)
if (BCORE_EXTRA_WARNINGS)
    target_compile_options(bCoreWarnings INTERFACE -Wall -Wextra)
endif()
if (BCORE_WERROR)
    target_compile_options(bCoreWarnings INTERFACE -Werror)
endif()
