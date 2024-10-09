//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2019 Comcast Corporation
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
//

#include "resourceContainer.h"
#include <icLog/logging.h>
#include <string.h>

#define LOG_TAG "resourceContainer"

int findEnumForLabel(const char *needle, const char *const haystack[], size_t haystackLength)
{
    int out = INVALID_ENUM;

    if (needle == NULL || haystack == NULL)
    {
        return out;
    }

    for (int i = 0; i < haystackLength; i++)
    {
        if (strcmp(haystack[i], needle) == 0)
        {
            out = i;
            break;
        }
    }

    if (out == INVALID_ENUM)
    {
        icLogWarn(LOG_TAG, "No valid enum value found for %s", needle);
    }

    return out;
}
