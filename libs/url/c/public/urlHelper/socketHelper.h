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
// Created by Christian Leithner on 4/16/20.
//

#ifndef ZILKER_SOCKETHELPER_H
#define ZILKER_SOCKETHELPER_H

typedef enum
{
    SOCKET_HELPER_FIRST = 0U,
    SOCKET_HELPER_ERROR_NONE = SOCKET_HELPER_FIRST,
    SOCKET_HELPER_ERROR_BAD_FD = 1U << 0U,
    SOCKET_HELPER_ERROR_HOSTNAME_TRANSLATION = 1U << 1U,
    SOCKET_HELPER_ERROR_CONNECT = 1U << 2U,
    SOCKET_HELPER_ERROR_CONF_SO_MARK = 1U << 3U,
    SOCKET_HELPER_ERROR_CONF_SO_BINDTODEVICE = 1U << 4U,
    SOCKET_HELPER_ERROR_CONF_SO_NOSIGPIPE = 1U << 5U,
    SOCKET_HELPER_LAST = SOCKET_HELPER_ERROR_CONF_SO_NOSIGPIPE
} socketHelperError;

/**
 * Configure a socket with common options.
 *
 * If interface is provided and not NULL or empty, attempts to set SO_BINDTODEVICE with that interface.
 * If useCell is true and this platform supports cellular routing, attempts to set SO_MARK.
 * If disableSigPipe is true, attempts to enable SO_NOSIGPIPE
 *
 * @note This function will not close the provided socket, even on error.
 *
 * @param socketFd - The file descriptor returned by the "socket" in "socket.h"
 * @param interface - The name of the network interface to bind to. This parameter is optional and can be NULL if no
 *                    binding is desired. Note there are constraints on what SO_BINDTODEVICE can do. See socket man page
 *                    for details.
 * @param useCell - Whether or not to configure the socket for cell.
 * @param disableSigPipe - Whether to enable SO_NOSIGPIPE
 * @return SOCKET_HELPER_ERROR_NONE on successful configuration. On error, errno is set and this function returns a mask
 *          of socketHelperErrors corresponding to where the errors occurred.
 * @see socketHelperError
 */
int socketHelperConfigure(int socketFd, const char *interface, bool useCell, bool disableSigPipe);

/**
 * Attempts to connect to the provided hostname/port on the provided socket.
 *
 * @note This function will not close the provided socket, even on error.
 *
 * @param socketFd - The file descriptor returned by the "socket" in "socket.h"
 * @param hostname - The hostname to connect to.
 * @param port - The port to connect to.
 * @return SOCKET_HELPER_ERROR_NONE on successful connect. On error, errno is set and this function returns the
 *         socketHelperError corresponding to where the error occurred.
 * @see socketHelperError
 */
socketHelperError socketHelperTryConnectHost(int socketFd, const char *hostname, uint16_t port);

/**
 * Attempts to connect to the provided sockaddr on the provided socket.
 *
 * @note This function will not close the provided socket, even on error.
 *
 * @param socketFd - The file descriptor returned by the "socket" in "socket.h"
 * @param addr - The sockaddr type to connect to.
 * @param socklen_t - The size of the provided sockaddr type
 * @return SOCKET_HELPER_ERROR_NONE on successful connect. On error, errno is set and this function returns the
 *         socketHelperError corresponding to where the error occurred.
 * @see socketHelperError
 */
socketHelperError socketHelperTryConnectAddr(int socketFd, const struct sockaddr *addr, socklen_t addrlen);

/*
 * Attempts a simple connect to the provided address.
 */
bool performSimpleConnect(bool useCell, const char *interface, const char *hostname, uint16_t port);

#endif // ZILKER_SOCKETHELPER_H
