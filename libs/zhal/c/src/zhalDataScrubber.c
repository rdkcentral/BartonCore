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

/*
 * Created by Raiyan Chowdhury on 2/21/25.
 */

#include <pthread.h>
#include <icUtil/regexUtils.h>
#include "zhalDataScrubber.h"

REGEX_SIMPLE_REPLACER(NETWORK_CONFIG_DATA_REPLACER, "\"networkConfigData\":\"\\([^\"]*\\)\"", NULL, "<REDACTED>");
static RegexReplacer *SENSITIVE_DATA_REPLACERS[] = { &NETWORK_CONFIG_DATA_REPLACER, NULL };

static pthread_once_t init_once = PTHREAD_ONCE_INIT;

static void initializeZhalDataScrubber()
{
    regexInitReplacers(SENSITIVE_DATA_REPLACERS);
}

char *zhalFilterJsonForLog(const cJSON *event)
{
    pthread_once(&init_once, initializeZhalDataScrubber);

    scoped_generic char *eventString = cJSON_PrintUnformatted(event);
    return regexReplace(eventString, SENSITIVE_DATA_REPLACERS);
}
