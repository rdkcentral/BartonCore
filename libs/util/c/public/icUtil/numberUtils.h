//------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2024 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
//
//------------------------------ tabstop = 4 ----------------------------------

/*-----------------------------------------------
 * numberUtils.h
 *
 * Utilities for numbers
 *
 * Author: eInfochips
 *-----------------------------------------------*/

#pragma once

#include <inttypes.h>
#include <stdbool.h>

/*
 * Generate random number between given range
 * NOTE: caller is responsible for passing first arg value lower than second arg.
 */
bool generateRandomNumberInRange(uint32_t lowerRange, uint32_t higherRange, uint64_t *randomNumber);
