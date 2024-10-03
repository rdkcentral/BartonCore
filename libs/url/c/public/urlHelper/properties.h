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

#ifndef ZILKER_PROPERTIES_H
#define ZILKER_PROPERTIES_H

#define URL_HELPER_PROP_NAMESPACE                "cpe.url"
#define URL_HELPER_PROP_REUSE_NAMESPACE          URL_HELPER_PROP_NAMESPACE ".reuse"

/**
 * Enable TLS session ID caching.
 * This accelerates handshakes when reconnecting.
 * @note This only takes effect on process start.
 */
#define URL_HELPER_PROP_REUSE_TLS_SESSION_ENABLE URL_HELPER_PROP_REUSE_NAMESPACE ".tlsSession.enable"

/**
 * Enable connection caching. Requests for a hostname will attempt to reuse
 * any existing connection to accelerate back-to-back requests.
 * @note This only takes effect on process start.
 */
#define URL_HELPER_PROP_REUSE_CONN_ENABLE        URL_HELPER_PROP_REUSE_NAMESPACE ".conn.enable"

/**
 * Set the maximum idle time for a host's cached connection.
 * When this is exceeded, a new connection is made.
 */
#define URL_HELPER_PROP_REUSE_CONN_IDLE_SECS     URL_HELPER_PROP_REUSE_NAMESPACE ".conn.idleSecs"

/**
 * Cache DNS resolutions. Beware of resource record TTLs and
 * network (e.g., cell vs wifi) configuration changes, which may
 * cause entries to become invalid.
 * @note This only takes effect on process start.
 */
#define URL_HELPER_PROP_REUSE_DNS_ENABLE         URL_HELPER_PROP_REUSE_NAMESPACE ".dns.enable"

/**
 * Enable resource caching.
 * When disabled, requests will not attempt to use the cache.
 */
#define URL_HELPER_PROP_REUSE_ENABLE             URL_HELPER_PROP_REUSE_NAMESPACE ".enable"


#endif // ZILKER_PROPERTIES_H
