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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <icLog/logging.h>
#include "icTypes/sbrm.h"

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
            if ((flags & (O_RDWR|O_WRONLY)) != 0)
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