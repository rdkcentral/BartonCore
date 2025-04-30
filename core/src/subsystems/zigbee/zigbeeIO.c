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

#include "subsystems/zigbee/zigbeeIO.h"
#include <errno.h>
#include <inttypes.h>
#include <memory.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef CONFIG_OS_DARWIN
#include <machine/endian.h>
// re-define the missing functions in OS X to just return the value
#define le16toh(tmp) tmp
#define le32toh(tmp) tmp
#define htole16(tmp) tmp
#define htole32(tmp) tmp
#else
#include <endian.h>
#endif


#include <icLog/logging.h>

#define LOG_TAG "zigbeeIO"

extern inline void zigbeeIODestroy__auto(ZigbeeIOContext **ctx);

/* Private Data */

struct _ZigbeeIOContext
{
    uint8_t *cur;
    const uint8_t *end;
    const ZigbeeIOMode mode;
};

/* Private functions */
static void checkEnd(ZigbeeIOContext *ctx, size_t size)
{
    if (!ctx->cur || !ctx->end || ctx->cur + size > ctx->end)
    {
        ctx->cur = NULL;
        errno = ESPIPE;
    }
}

static bool canPerformOperation(ZigbeeIOContext *ctx, size_t size, ZigbeeIOMode mode)
{
    if (ctx->mode != mode)
    {
        ctx->cur = NULL;
        errno = EPERM;
        icLogError(LOG_TAG, "%s operation not allowed on this ZIO context", mode == ZIO_READ ? "read" : "write");
    }
    else if (errno == 0)
    {
        checkEnd(ctx, size);
    }

    return errno == 0;
}

static void readVal(void *out, ZigbeeIOContext *ctx, size_t size)
{
    if (canPerformOperation(ctx, size, ZIO_READ))
    {
        switch (size)
        {
            case sizeof(uint8_t):
                memcpy(out, ctx->cur, size);
                break;
            case sizeof(uint16_t):
            {
                uint16_t tmp;
                memcpy(&tmp, ctx->cur, size);
                *((uint16_t *) out) = le16toh(tmp);
                break;
            }
            case sizeof(uint32_t):
            {
                uint32_t tmp;
                memcpy(&tmp, ctx->cur, size);
                *((uint32_t *) out) = le32toh(tmp);
                break;
            }
            default:
                icLogError(LOG_TAG, "%zu-bit values not supported", size * 8);
                errno = EINVAL;
                ctx->cur = NULL;
                break;
        }
        ctx->cur += size;
    }
}

static void writeVal(ZigbeeIOContext *ctx, void *val, size_t size)
{
    if (canPerformOperation(ctx, size, ZIO_WRITE))
    {
        switch (size)
        {
            case sizeof(uint8_t):
                memcpy(ctx->cur, val, size);
                break;
            case sizeof(uint16_t):
            {
                uint16_t tmp = htole16(*((uint16_t *) val));
                memcpy(ctx->cur, &tmp, size);
            }
            break;
            case sizeof(uint32_t):
            {
                uint32_t tmp = htole32(*((uint32_t *) val));
                memcpy(ctx->cur, &tmp, size);
            }
            break;
            default:
                errno = EINVAL;
                ctx->cur = NULL;
                break;
        }
        ctx->cur += size;
    }
}

/* Public functions */

ZigbeeIOContext *zigbeeIOInit(uint8_t payload[], const size_t payloadLen, ZigbeeIOMode mode)
{
    ZigbeeIOContext *ctx = calloc(1, sizeof(ZigbeeIOContext));
    ctx->cur = payload;
    ctx->end = payload + payloadLen;

    if (mode == ZIO_WRITE)
    {
        memset(payload, 0, payloadLen);
    }
    memcpy((void *) &ctx->mode, &mode, sizeof(ZigbeeIOMode));
    errno = 0;

    return ctx;
}

void zigbeeIODestroy(ZigbeeIOContext *ctx)
{
    if (!errno && ctx->cur != ctx->end)
    {
        icLogWarn(
            LOG_TAG, "Partial %s on payload: result may not be correct", ctx->mode == ZIO_READ ? "read" : "write");
    }
    free(ctx);
}

uint8_t zigbeeIOGetUint8(ZigbeeIOContext *ctx)
{
    uint8_t val = 0;

    readVal(&val, ctx, sizeof(uint8_t));

    return val;
}

void zigbeeIOPutUint8(ZigbeeIOContext *ctx, uint8_t val)
{
    writeVal(ctx, &val, sizeof(uint8_t));
}

int8_t zigbeeIOGetInt8(ZigbeeIOContext *ctx)
{
    int8_t val = 0;

    readVal(&val, ctx, sizeof(int8_t));

    return val;
}

void zigbeeIOPutInt8(ZigbeeIOContext *ctx, int8_t val)
{
    writeVal(ctx, &val, sizeof(int8_t));
}

char *zigbeeIOGetString(ZigbeeIOContext *ctx)
{
    char *out = NULL;
    uint8_t strlen = zigbeeIOGetUint8(ctx);

    if (canPerformOperation(ctx, strlen, ZIO_READ))
    {
        out = calloc(strlen + 1, 1);
        strncpy(out, ctx->cur, strlen);
        out[strlen] = 0;
        ctx->cur += strlen;
    }

    return out;
}

void zigbeeIOGetBytes(ZigbeeIOContext *ctx, void *buf, uint8_t len)
{
    if (canPerformOperation(ctx, len, ZIO_READ))
    {
        memcpy(buf, ctx->cur, len);
        ctx->cur += len;
    }
}

void zigbeeIOPutString(ZigbeeIOContext *ctx, char *str)
{
    size_t len = strlen(str);

    if (len > UINT8_MAX)
    {
        errno = EINVAL;
        ctx->cur = NULL;
    }
    else
    {
        writeVal(ctx, (uint8_t *) &len, sizeof(uint8_t));
        strncpy(ctx->cur, str, len);
    }
}

void zigbeeIOPutBytes(ZigbeeIOContext *ctx, void *buf, uint8_t len)
{
    if (canPerformOperation(ctx, len, ZIO_WRITE))
    {
        memcpy(ctx->cur, buf, len);
        ctx->cur += len;
    }
}

uint16_t zigbeeIOGetUint16(ZigbeeIOContext *ctx)
{
    uint16_t out = 0;

    readVal(&out, ctx, sizeof(uint16_t));

    return out;
}

void zigbeeIOPutUint16(ZigbeeIOContext *ctx, uint16_t val)
{
    writeVal(ctx, &val, sizeof(uint16_t));
}


int16_t zigbeeIOGetInt16(ZigbeeIOContext *ctx)
{
    int16_t out = 0;

    readVal(&out, ctx, sizeof(int16_t));

    return out;
}

void zigbeeIOPutInt16(ZigbeeIOContext *ctx, int16_t val)
{
    writeVal(ctx, &val, sizeof(int16_t));
}

uint32_t zigbeeIOGetUint32(ZigbeeIOContext *ctx)
{
    uint32_t out = 0;

    readVal(&out, ctx, sizeof(uint32_t));

    return out;
}

void zigbeeIOPutUint32(ZigbeeIOContext *ctx, uint32_t val)
{
    writeVal(ctx, &val, sizeof(uint32_t));
}

int32_t zigbeeIOGetInt32(ZigbeeIOContext *ctx)
{
    int32_t out = 0;

    readVal(&out, ctx, sizeof(int32_t));

    return out;
}

void zigbeeIOPutInt32(ZigbeeIOContext *ctx, int32_t val)
{
    writeVal(ctx, &val, sizeof(int32_t));
}

/**
 * Returns the remaining size in the context.
 *
 * @param ctx
 * @return
 */
size_t zigbeeIOGetRemainingSize(ZigbeeIOContext *ctx)
{
    size_t out = 0;

    if (ctx != NULL && ctx->cur != NULL && ctx->end != NULL && ctx->end >= ctx->cur)
    {
        out = ctx->end - ctx->cur;
    }
    else
    {
        errno = EINVAL;
    }

    return out;
}
