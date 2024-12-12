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
