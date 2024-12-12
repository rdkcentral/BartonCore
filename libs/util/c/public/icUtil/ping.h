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
// Created by wboyd747 on 6/17/20.
//

#ifndef _CCATX_ping_H
#define _CCATX_ping_H

#include <stdint.h>

struct sockaddr;

struct pingResults
{
    uint32_t transmitted;
    uint32_t received;

    uint32_t rtt_avg_usec;
    uint32_t rtt_min_usec;
    uint32_t rtt_max_usec;
};

/**
 * Send ICMP ping request to a specified IP address.
 *
 * @param iface Specific interface to test ping request. If NULL any interface may be used.
 * @param dst The destination host to send the ping request to.
 * @param count The total number of requests to be sent.
 * @param fwmark Special fwmark to be applied to the connection. A value of 0 disables the fwmark.
 * @param ttl The Time-to-Live count. Anything less than, or equal to, 0 will disable the TTL.
 * @param packetTimeoutMs The amount of time to wait for a ping response.
 * @param results Data representing the statistics performed during the ping.
 * @return A value of 0 is returned on success. Otherwise a positive error code.
 *         EINVAL         Destination host was not specified
 *         EPFNOSUPPORT   IPv6 is not currently supported.
 *         ENETUNREACH    The destination host could not be reached.
 */
int icPing(const char *iface,
           const struct sockaddr *dst,
           uint32_t count,
           uint32_t fwmark,
           int ttl,
           int packetTimeoutMs,
           struct pingResults *results);

#endif //_CCATX_ping_H
