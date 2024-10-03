//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2020. Comcast Corporation
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


//
// Created by Christian Leithner on 4/16/20.
//
// Helper functions for working with sockets.
//

#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <icLog/logging.h>
#include <icTypes/sbrm.h>
#include <icUtil/stringUtils.h>
#include <urlHelper/socketHelper.h>

#define LOG_TAG "socketHelper"

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
// TODO: Provide a better configuration interface for this is the need arises. Maybe separate functions or some kind
//  of bitmask input?
int socketHelperConfigure(int socketFd, const char *interface, bool useCell, bool disableSigPipe)
{
    if (socketFd < 0)
    {
        icLogError(LOG_TAG, "%s: Invalid socket fd provided", __FUNCTION__);
        errno = EBADF;

        return SOCKET_HELPER_ERROR_BAD_FD;
    }

    int retVal = SOCKET_HELPER_ERROR_NONE;

    // If an interface was provided, try to bind to it
#ifdef SO_BINDTODEVICE
    if (stringIsEmpty(interface) == false)
    {
        icLogTrace(LOG_TAG, "%s: binding socket to interface %s", __FUNCTION__, interface);
        if (setsockopt(socketFd, SOL_SOCKET, SO_BINDTODEVICE, interface, strlen(interface) + 1) != 0)
        {
            AUTO_CLEAN(free_generic__auto) char *errStr = strerrorSafe(errno);
            icLogError(
                LOG_TAG, "%s: could not set SO_BINDTODEVICE to %s: [%d][%s]", __FUNCTION__, interface, errno, errStr);

            retVal |= SOCKET_HELPER_ERROR_CONF_SO_BINDTODEVICE;
        }
    }
#endif

    // Attempt to disable SIGPIPE signal for this socket if we can.
#ifdef SO_NOSIGPIPE
    // not available on some platforms.  hopefully if not, they support MSG_NOSIGNAL -
    // which could be used on the send() calls
    if (disableSigPipe == true)
    {
        int enable = 1;
        icLogTrace(LOG_TAG, "%s: attempting to enable SO_NOSIGPIPE", __FUNCTION__);
        if (setsockopt(socketFd, SOL_SOCKET, SO_NOSIGPIPE, &enable, sizeof(enable)) != 0)
        {
            AUTO_CLEAN(free_generic__auto) char *errStr = strerrorSafe(errno);
            icLogWarn(LOG_TAG, "%s: unable to set SO_NOSIGPIPE flag [%d][%s]", __FUNCTION__, errno, errStr);

            retVal |= SOCKET_HELPER_ERROR_CONF_SO_NOSIGPIPE;
        }
    }
#endif

    return retVal;
}

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
socketHelperError socketHelperTryConnectHost(int socketFd, const char *hostname, uint16_t port)
{
    if (socketFd < 0)
    {
        icLogError(LOG_TAG, "%s: Invalid socket fd provided", __FUNCTION__);
        errno = EBADF;

        return SOCKET_HELPER_ERROR_BAD_FD;
    }

    socketHelperError retVal = SOCKET_HELPER_ERROR_NONE;

    // Convert the hostname/port into a sockaddr type and perform basic connect.
#if (defined HAVE_GETADDRINFO || defined CONFIG_OS_DARWIN)
    struct addrinfo hints;
    struct addrinfo *addr_res, *addr_ptr;
    char port_str[6];

    hints.ai_flags = AI_CANONNAME;
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    hints.ai_addrlen = 0;
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    snprintf(port_str, 6, "%i", port);

    if (getaddrinfo(hostname, port_str, &hints, &addr_res) != 0)
    {
        AUTO_CLEAN(free_generic__auto) char *errStr = strerrorSafe(errno);
        icLogError(LOG_TAG,
                   "%s: Could not get address info for provided host/port %s:%" PRIu16 " [%d][%s]",
                   __FUNCTION__,
                   hostname,
                   port,
                   errno,
                   errStr);

        return SOCKET_HELPER_ERROR_HOSTNAME_TRANSLATION;
    }

    for (addr_ptr = addr_res; (addr_ptr != NULL) && (retVal == false); addr_ptr = addr_ptr->ai_next)
    {
        if (isIcLogPriorityTrace() == true)
        {
            char ipAddr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(addr_ptr->ai_addr), ipAddr, INET_ADDRSTRLEN);
            icLogTrace(LOG_TAG,
                       "%s: Performing socket connection to %s (%s) at port %" PRIu16,
                       __FUNCTION__,
                       hostname,
                       ipAddr,
                       port);
        }

        retVal = socketHelperTryConnectAddr(socketFd, addr_ptr->ai_addr, addr_ptr->ai_addrlen);
    }

    freeaddrinfo(addr_res);
#else
    // Do not free hostent as it may point to static data.
    struct hostent *host;
    struct sockaddr_in sin;

    host = gethostbyname(hostname);
    if (host == NULL || (host->h_addr_list[0] == NULL))
    {
        AUTO_CLEAN(free_generic__auto) char *errStr = strerrorSafe(errno);
        icLogError(LOG_TAG,
                   "%s: Could not get address info for provided host/port %s:%" PRIu16 " [%d][%s]",
                   __FUNCTION__,
                   hostname,
                   port,
                   errno,
                   errStr);

        return SOCKET_HELPER_ERROR_HOSTNAME_TRANSLATION;
    }

    memcpy(&sin.sin_addr, host->h_addr, (size_t) host->h_length);

    sin.sin_family = (sa_family_t) host->h_addrtype;
    sin.sin_port = htons(port);

    if (isIcLogPriorityTrace() == true)
    {
        char ipAddr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(sin.sin_addr), ipAddr, INET_ADDRSTRLEN);
        icLogTrace(LOG_TAG,
                   "%s: Performing socket connection to %s (%s) at port %" PRIu16,
                   __FUNCTION__,
                   hostname,
                   ipAddr,
                   port);
    }

    retVal = socketHelperTryConnectAddr(socketFd, (struct sockaddr *) &sin, sizeof(struct sockaddr_in));
#endif

    return retVal;
}

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
socketHelperError socketHelperTryConnectAddr(int socketFd, const struct sockaddr *addr, socklen_t addrlen)
{
    if (socketFd < 0)
    {
        icLogError(LOG_TAG, "%s: Invalid socket fd provided", __FUNCTION__);
        errno = EBADF;

        return SOCKET_HELPER_ERROR_BAD_FD;
    }

    if (connect(socketFd, addr, addrlen) != 0)
    {
        AUTO_CLEAN(free_generic__auto) char *errStr = strerrorSafe(errno);
        icLogError(LOG_TAG, "%s: Failed to connect to address [%d][%s]", __FUNCTION__, errno, errStr);

        return SOCKET_HELPER_ERROR_CONNECT;
    }

    return SOCKET_HELPER_ERROR_NONE;
}

/*
 * Attempts a simple connect to the provided address.
 */
bool performSimpleConnect(bool useCell, const char *interface, const char *hostname, uint16_t port)
{
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd == -1)
    {
        AUTO_CLEAN(free_generic__auto) char *errStr = strerrorSafe(errno);
        icLogError(LOG_TAG, "%s: Failed to create socket. [%d][%s]", __FUNCTION__, errno, errStr);
        return false;
    }

    int rc = socketHelperConfigure(fd, interface, useCell, true);
    if (rc != SOCKET_HELPER_ERROR_NONE)
    {
        icLogError(LOG_TAG, "%s: Failed to configure socket.", __FUNCTION__);
        close(fd);
        return false;
    }

    if (socketHelperTryConnectHost(fd, hostname, port) != SOCKET_HELPER_ERROR_NONE)
    {
        icLogError(LOG_TAG, "%s: Failed to connect to server", __FUNCTION__);
        close(fd);

        return false;
    }

    // Cleanup
    if (fd >= 0)
    {
        shutdown(fd, SHUT_RDWR);
        close(fd);
    }
    return true;
}
