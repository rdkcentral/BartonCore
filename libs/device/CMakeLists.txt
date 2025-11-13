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

#--------------------------------------------------------------------------------------
#
# CMake definition for the fcore device libraries
# Should not be utilized directly, but imported from our top-level
# file via the 'add_subdirectory()' routine.
#
#--------------------------------------------------------------------------------------

# the device libraries to compile
#
add_subdirectory(descriptors/c)

# add 'philips hue lights' if the option is set
if (BCORE_PHILIPS_HUE)
    add_subdirectory(xhPhilipsHue/c)
endif (BCORE_PHILIPS_HUE)
