//------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2026 Comcast Cable Communications Management, LLC
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

#include "deviceServiceHash.h"
#include "icUtil/stringUtils.h"

#include <errno.h>
#include <stdio.h>

#include <glib.h>

#define LOG_TAG     "deviceServiceHash"
#define logFmt(fmt) "%s: " fmt, __func__
#include <icLog/logging.h>

char *deviceServiceHashComputeFileMd5HexString(const char *filePath)
{
    char *result = NULL;

    if (filePath == NULL)
    {
        icError("filePath is NULL");
        return result;
    }

    errno = 0;
    FILE *f = fopen(filePath, "rb");

    if (f == NULL)
    {
        g_autofree char *errMsg = strerrorSafe(errno);
        icError("failed to open file '%s' - %s", filePath, errMsg);
        return result;
    }

    GChecksum *cksum = g_checksum_new(G_CHECKSUM_MD5);

    if (cksum == NULL)
    {
        icError("failed to allocate checksum for file '%s'", filePath);
        fclose(f);
        return result;
    }

    guchar buf[4096];
    gsize n;

    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
    {
        g_checksum_update(cksum, buf, n);
    }

    if (ferror(f))
    {
        icError("error reading file '%s' while computing MD5", filePath);
    }
    else
    {
        result = g_strdup(g_checksum_get_string(cksum));
    }

    g_checksum_free(cksum);
    fclose(f);

    return result;
}
