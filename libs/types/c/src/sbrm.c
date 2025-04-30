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

#include "icTypes/sbrm.h"
#include <errno.h>
#include <fcntl.h>
#include <icLog/logging.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define LOG_CAT "SBRM"

extern inline void free_generic__auto(void *p);

void fclose__auto(FILE **fp)
{
    if (*fp == NULL)
    {
        return;
    }

    int fd = fileno(*fp);
    if (fd >= 0)
    {
        int flags = fcntl(fd, F_GETFL);
        if (flags != -1)
        {
            /**
             * Only attempt to flush and sync if any write bits are set on the fd.
             * fclose() will naturally discard unread data, so it is never necessary to explicitly fflush()
             * an input stream when it leaves scope. Implementations not conformant to POSIX.1-2008 (ISO C) will
             * complain with EBADF.
             * There is no readonly bit to test for (like darkness, readonly is the absence of write bits).
             */
            if ((flags & (O_RDWR | O_WRONLY)) != 0)
            {
                if (fflush(*fp) != 0)
                {
                    icLogWarn(LOG_CAT, "Failed to flush stream for fd %d: errno %d!", fd, errno);
                }

                if (fsync(fd) != 0)
                {
                    icLogWarn(LOG_CAT, "Failed to sync fd %d: errno %d. Data has been lost!", fd, errno);
                }
            }
        }
        else
        {
            icLogWarn(LOG_CAT, "Failed to get file status flags on fd %d: errno %d", fd, errno);
        }
    }
    else
    {
        icLogWarn(LOG_CAT, "fp is not a valid stream");
    }

    fclose(*fp);
}
