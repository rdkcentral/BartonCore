#!/bin/bash
# ------------------------------ tabstop = 4 ----------------------------------
#
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2026 Comcast Cable Communications Management, LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0
#
# ------------------------------ tabstop = 4 ----------------------------------

#
# Set up IPv6 routes so the barton container can reach Thread devices via
# the otbr-radio container.
#
# Thread mesh-local and on-mesh prefix addresses are all in the ULA range
# (fd00::/8).  The Docker bridge subnet (e.g. fd00:xxxx:yyyy::/64) is also
# in this range but has a more-specific /64 route, so the /8 catch-all only
# matches Thread traffic.
#
# This script is idempotent — safe to run multiple times.
# It runs as part of the devcontainer postStartCommand so routes are
# restored after container restarts.
#

set -e

# Resolve the otbr-radio container's IPv6 address on the shared bridge.
# Docker's embedded DNS makes the service name "otbr-radio" resolvable
# from other containers on the same Compose network.
OTBR_IPV6=$(getent ahostsv6 otbr-radio 2>/dev/null | grep STREAM | head -1 | awk '{print $1}')

if [ -z "${OTBR_IPV6}" ]; then
    echo "[setup-thread-routes] otbr-radio not resolvable — Thread routes not configured."
    echo "[setup-thread-routes] This is normal when the otbr-radio container is not running."
    exit 0
fi

# Add a catch-all ULA route via the otbr-radio container.
# 'replace' is idempotent — updates the route if it already exists.
sudo ip -6 route replace fd00::/8 via "${OTBR_IPV6}" dev eth0 2>/dev/null || true

echo "[setup-thread-routes] Thread route installed: fd00::/8 via ${OTBR_IPV6}"
