//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2025 Comcast Corporation
//
// All rights reserved.
//
// This software is protected by copyright laws of the United States
// and of foreign countries. This material may also be protected by
// patent laws of the United States and of foreign countries.
//
// This software is furnished under a license agreement and/or a
// nondisclosure agreement and may only be used or copied in accordance
// with the terms of those agreements.
//
// The mere transfer of this software does not imply any licenses of trade
// secrets, proprietary technology, copyrights, patents, trademarks, or
// any other form of intellectual property whatsoever.
//
// Comcast Corporation retains all ownership rights.
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
