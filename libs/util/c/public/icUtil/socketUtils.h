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
//
// Created by mkoch201 on 10/7/21.
//

#ifndef ZILKER_SOCKETUTILS_H
#define ZILKER_SOCKETUTILS_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/**
 * Setup a pipe to use for shutdown down signals to select.  Initializes the fds with the pipe and sets them to
 * non blocking
 * @param shutdownReadFd Populated with the read fd
 * @param shutdownWriteFd Populated with the write fd
 */
void setupShutdownFds(int *shutdownReadFd, int *shutdownWriteFd);

/**
 * Close the shutdown pipe created from setupShutdownFds
 */
void closeShutdownFds(int *shutdownReadFd, int *shutdownWriteFd);

/*
 * test to see if the socket is ready for reading.  performs a
 * select() call under the hood and will block up-to timeoutSecs.
 * returns true if the socket has data to read.
 *
 * @param sockFD - socket to monitor
 * @param timeoutSecs - number of seconds to wait for.  if <= 0, the call will return immediately
 */
bool canReadFromSocket(int32_t sockFD, time_t timeoutSecs);

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
int canReadFromServiceSocket(int32_t serviceSock, int32_t shutdownSock, time_t timeoutSecs);

/*
 * test to see if the socket is ready for writing.  performs a
 * select() call under the hood and will block up-to timeoutSecs.
 * returns true if the socket is ready to write.
 *
 * @param sockFD - socket to monitor
 * @param timeoutSecs - number of seconds to wait for.  if <= 0, the call will return immediately
 */
bool canWriteToSocket(int32_t sockFD, time_t timeoutSecs);

/**
 * Set TCP user timeout on a socket
 * @param socket the socket to set the timeout on
 * @param timeoutMillis the timeout in millis
 * @return true if successful, false otherwise
 */
bool setTCPUserTimeout(int socket, unsigned int timeoutMillis);

#endif // ZILKER_SOCKETUTILS_H
