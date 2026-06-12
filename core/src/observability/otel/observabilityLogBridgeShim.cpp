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
 * Linker-wrap shim for icLogMsg → OTel log bridge.
 *
 * When BCORE_OBSERVABILITY is enabled, the BartonCore library is linked with
 * -Wl,--wrap=icLogMsg. This causes all calls to icLogMsg within BartonCore to
 * be redirected to __wrap_icLogMsg, which:
 *   1. Calls the real icLogMsg (__real_icLogMsg) for normal stdout/syslog output
 *   2. Forwards the message to the OTel log bridge for structured export
 *
 * This achieves dual-output (icLog + OTel) without modifying BartonCommon.
 */

#include "observability/observabilityLogBridge.h"

#ifdef BARTON_CONFIG_OBSERVABILITY

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* The real icLogMsg from the BartonCommon logging backend */
extern "C" void __real_icLogMsg(const char *file,
                                size_t filelen,
                                const char *func,
                                size_t funclen,
                                long line,
                                const char *categoryName,
                                int priority,
                                const char *format,
                                ...);

extern "C" void __wrap_icLogMsg(const char *file,
                                size_t filelen,
                                const char *func,
                                size_t funclen,
                                long line,
                                const char *categoryName,
                                int priority,
                                const char *format,
                                ...)
{
    va_list args;

    /* 1. Forward to the real icLog backend (stdout/syslog/etc.) */
    va_start(args, format);
    /* We must use the variadic version — the real icLogMsg is variadic too.
     * Since we cannot forward va_list to a variadic function portably,
     * we format the message ourselves and call __real_icLogMsg with "%s". */
    char msgBuf[2048];
    vsnprintf(msgBuf, sizeof(msgBuf), format, args);
    va_end(args);

    __real_icLogMsg(file, filelen, func, funclen, line, categoryName, priority, "%s", msgBuf);

    /* 2. Forward to the OTel log bridge */
    observabilityLogBridgeEmit(categoryName, priority, file, func, (int) line, msgBuf);
}

#endif /* BARTON_CONFIG_OBSERVABILITY */
