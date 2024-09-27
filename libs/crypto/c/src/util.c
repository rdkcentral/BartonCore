//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2020 Comcast Corporation
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
