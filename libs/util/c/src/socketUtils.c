//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2020 Comcast
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
// Comcast retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------
//
// Created by mkoch201 on 10/7/21.
//

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <icLog/logging.h>
#include <icUtil/socketUtils.h>
#include <icUtil/stringUtils.h>

#define LOG_TAG "socketUtils"

void setupShutdownFds(int *shutdownReadFd, int *shutdownWriteFd)
{
    int rc = 0;
    int shutdownFds[2];
    // Setup our shutdown pipes
    pipe(shutdownFds);
    // Set read end non blocking
    int flags = fcntl(shutdownFds[0], F_GETFL);
    flags |= O_NONBLOCK;
    rc = fcntl(shutdownFds[0], F_SETFL, flags);
    if (rc != 0)
    {
        char *errStr = strerrorSafe(rc);
        icLogWarn(LOG_TAG, "Unable to set O_NONBLOCK on shutdown pipe (read): %s", errStr);
        free(errStr);
    }

    // Set write end non blocking
    flags = fcntl(shutdownFds[1], F_GETFL);
    flags |= O_NONBLOCK;
    rc = fcntl(shutdownFds[1], F_SETFL, flags);

    if (rc != 0)
    {
        char *errStr = strerrorSafe(rc);
        icLogWarn(LOG_TAG, "Unable to set O_NONBLOCK on shutdown pipe (write): %s", errStr);
        free(errStr);
    }

    *shutdownReadFd = shutdownFds[0];
    *shutdownWriteFd = shutdownFds[1];
}

/**
 * Close the shutdown pipe created from setupShutdownFds
 */
void closeShutdownFds(int *shutdownReadFd, int *shutdownWriteFd)
{
    if (shutdownReadFd != NULL && *shutdownReadFd >= 0)
    {
        close(*shutdownReadFd);
        *shutdownReadFd = -1;
    }
    if (shutdownWriteFd != NULL && *shutdownWriteFd >= 0)
    {
        close(*shutdownWriteFd);
        *shutdownWriteFd = -1;
    }
}

/*
 * test to see if the socket is ready for reading.  performs a
 * select() call under the hood and will block up-to timeoutSecs.
 * returns true if the socket has data to read.
 *
 * @param sockFD - socket to monitor
 * @param timeoutSecs - number of seconds to wait for.  if <= 0, the call will return immediately
 */
bool canReadFromSocket(int32_t sockFD, time_t timeoutSecs)
{
    int rc = canReadFromServiceSocket(sockFD, -1, timeoutSecs);
    if (rc == 0)
    {
        return true;
    }
    return false;
}

/*
 * check if can read from either socket.  performs a
 * select() call under the hood and will block up-to timeoutSecs.
 * returns:
 *   0         - if 'serviceSock' is ready
 *   ETIMEDOUT - if 'serviceSock' was not ready within the timeoutSecs
 *   EINTR     - if 'shutdownSock' is ready
 *   EAGAIN    - all other conditions

 * @param serviceSock - first socket to monitor
 * @param shutdownSock - second socket to monitor (optional, pass < 0 to skip)
 * @param timeoutSecs - number of seconds to wait for.  if <= 0, the call will return immediately
 */
int canReadFromServiceSocket(int32_t serviceSock, int32_t shutdownSock, time_t timeoutSecs)
{
    fd_set readFds, writeFds, exceptFds;
    int rc = 0;
    int maxFD = serviceSock;
    struct timeval timeout;

    // setup the descriptors
    //
    FD_ZERO(&readFds);
    FD_ZERO(&writeFds);
    FD_ZERO(&exceptFds);
    // coverity[uninit_use]
    FD_SET(serviceSock, &readFds);
    // coverity[uninit_use]
    FD_SET(serviceSock, &exceptFds);

    if (shutdownSock >= 0)
    {
        // add the 'shutdown pipe' to the select so we can un-block if necessary
        FD_SET(shutdownSock, &readFds);
        FD_SET(shutdownSock, &exceptFds);
        if (shutdownSock > maxFD)
        {
            maxFD = shutdownSock;
        }
    }

    // apply the timeout in seconds
    if (timeoutSecs < 0)
    {
        timeoutSecs = 0;
    }
    timeout.tv_sec = timeoutSecs;
    timeout.tv_usec = 0;

    // wait up to 'timeout seconds' for something to appear on the 'serviceSock'
    rc = select(maxFD + 1, &readFds, &writeFds, &exceptFds, &timeout);
    if (rc == 0)
    {
        return ETIMEDOUT;
    }
    else if (rc > 0)
    {
        if (shutdownSock >= 0 && FD_ISSET(shutdownSock, &readFds))
        {
            return EINTR;
        }
        else if (serviceSock >= 0 && FD_ISSET(serviceSock, &readFds))
        {
            return 0;
        }
    }
    else
    {
        if (errno == EBADF)
            return EINTR;

        // error running select call
        char *errStr = strerrorSafe(errno);
        icLogError(LOG_TAG, "error testing socket %s", errStr);
        free(errStr);
        return errno;
    }

    return EAGAIN;
}

/*
 * test to see if the socket is ready for writing.  performs a
 * select() call under the hood and will block up-to timeoutSecs.
 * returns true if the socket is ready to write.
 *
 * @param sockFD - socket to monitor
 * @param timeoutSecs - number of seconds to wait for.  if <= 0, the call will return immediately
 */
bool canWriteToSocket(int32_t sockFD, time_t timeoutSecs)
{
    fd_set readFds, writeFds, exceptFds;
    int rc = 0;
    struct timeval timeout;

    // setup the descriptors
    //
    FD_ZERO(&readFds);
    FD_ZERO(&writeFds);
    FD_ZERO(&exceptFds);
    // coverity[uninit_use]
    FD_SET(sockFD, &writeFds);
    // coverity[uninit_use]
    FD_SET(sockFD, &exceptFds);

    // apply timeout in seconds
    if (timeoutSecs <= 0)
    {
        timeoutSecs = 0;
    }
    timeout.tv_sec = timeoutSecs;
    timeout.tv_usec = 0;

    rc = select(sockFD + 1, &readFds, &writeFds, &exceptFds, &timeout);
    if (rc > 0)
    {
        // see if our socket is set
        //
        if (FD_ISSET(sockFD, &writeFds) != 0)
        {
            return true;
        }
    }
    else if (rc < 0)
    {
        // error running select
        char *errStr = strerrorSafe(errno);
        icLogError(LOG_TAG, "error testing socket %s", errStr);
        free(errStr);
    }

    return false;
}

bool setTCPUserTimeout(int socket, unsigned int timeoutMillis)
{
#ifndef CONFIG_PRODUCT_FLEX
    int status = setsockopt(socket, IPPROTO_TCP, TCP_USER_TIMEOUT, &timeoutMillis, sizeof(timeoutMillis));
    if (status != 0)
    {
        char *errStr = strerrorSafe(errno);
        icLogWarn(LOG_TAG, "Failed to set TCP user timeout - %s", errStr);
        free(errStr);
    }

    return status == 0;
#else
    // uClibc doesn't support TCP_USER_TIMEOUT
    return false;
#endif
}
