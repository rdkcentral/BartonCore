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

#include "util.h"
#include <icLog/logging.h>
#include <openssl/bio.h>
#include <string.h>

#define LOG_TAG "crypto/util"

char *getMemBIOString(BIO *bp)
{
    if (bp == NULL)
    {
        return NULL;
    }

    if (BIO_method_type(bp) != BIO_TYPE_MEM)
    {
        icLogError(LOG_TAG, "'bp' is not a memory BIO");
        return NULL;
    }

    char *str = NULL;
    const char *data = NULL;

    long dataLen = BIO_get_mem_data(bp, &data);

    if (dataLen > 0)
    {
        str = malloc(dataLen + 1);
        str = strncpy(str, data, dataLen);
        str[dataLen] = '\0';
    }
    else
    {
        icLogWarn(LOG_TAG, "%s: No buffered data", __func__);
    }

    return str;
}
