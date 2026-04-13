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
# Entrypoint for the otbr-radio container. Starts services in order:
#   1. D-Bus system bus
#   2. avahi-daemon  (mDNS/DNS-SD — required by otbr-agent built with OTBR_MDNS=avahi)
#   3. cpcd  (Silicon Labs CPC daemon — USB serial ↔ CPC socket)
#   4. otbr-agent  (Thread Border Router — CPC socket ↔ D-Bus API)
#
# Environment variables (all have defaults):
#   RADIO_DEVICE  - host path of the USB serial device, e.g. /dev/ttyACM0
#   BACKBONE_IF   - network interface to use as the Thread backbone, e.g. eth0
#   CPCD_CONF     - path to cpcd config file
#
# See docs/THREAD_BORDER_ROUTER_SUPPORT.md for USB-IP setup and hardware info.
#

set -e

RADIO_DEVICE="${RADIO_DEVICE:-/dev/ttyACM0}"
CPCD_CONF="${CPCD_CONF:-/usr/local/etc/cpcd.conf}"

# Resolve the backbone interface.
# 1. Use BACKBONE_IF from the environment if explicitly set and the interface exists.
# 2. Otherwise auto-detect from the default route.
# Never fall back silently to a wrong interface — fail fast with a clear message.
if [ -n "${BACKBONE_IF}" ] && ip link show "${BACKBONE_IF}" >/dev/null 2>&1; then
    echo "[otbr-radio] Using backbone interface from environment: ${BACKBONE_IF}"
else
    if [ -n "${BACKBONE_IF}" ]; then
        echo "[otbr-radio] WARNING: BACKBONE_IF '${BACKBONE_IF}' not found; auto-detecting..."
    fi
    BACKBONE_IF=$(ip route show default 2>/dev/null | awk '/default/ {print $5; exit}')
    if [ -z "${BACKBONE_IF}" ]; then
        echo "[otbr-radio] ERROR: Could not detect a backbone interface. Set BACKBONE_IF explicitly." >&2
        exit 1
    fi
    echo "[otbr-radio] Auto-detected backbone interface: ${BACKBONE_IF}"
fi

###############################################################################
# 1. Start D-Bus system bus
#
# The /var/run/dbus directory is mounted from the persistent named volume
# 'dbus-socket'. If the container previously crashed or was restarted, the
# pid file from the previous run may still be present, causing dbus-daemon to
# refuse to start. Clean it up if the recorded PID is no longer alive.
###############################################################################
echo "[otbr-radio] Starting D-Bus system bus..."
mkdir -p /var/run/dbus
dbusRunning=false
if [ -f /var/run/dbus/pid ]; then
    dbusPid=$(cat /var/run/dbus/pid 2>/dev/null || true)
    if [ -n "${dbusPid}" ] && kill -0 "${dbusPid}" 2>/dev/null; then
        dbusComm=$(cat "/proc/${dbusPid}/comm" 2>/dev/null || true)
        if [ "${dbusComm}" = "dbus-daemon" ] && [ -S /var/run/dbus/system_bus_socket ]; then
            dbusRunning=true
        fi
    fi
fi
if [ "${dbusRunning}" = "true" ]; then
    echo "[otbr-radio] D-Bus already running (PID ${dbusPid}), skipping start."
else
    echo "[otbr-radio] Removing stale D-Bus pid/socket files..."
    rm -f /var/run/dbus/pid /var/run/dbus/system_bus_socket
    dbus-daemon --system --fork
fi
echo "[otbr-radio] D-Bus started."

###############################################################################
# 2. Validate the USB radio device
###############################################################################
if [ ! -e "${RADIO_DEVICE}" ]; then
    echo "[otbr-radio] ERROR: USB radio device '${RADIO_DEVICE}' not found." >&2
    echo "[otbr-radio]        Check that the BRD2703 is connected and USB-IP is attached." >&2
    echo "[otbr-radio]        See docs/THREAD_BORDER_ROUTER_SUPPORT.md for setup instructions." >&2
    exit 1
fi
echo "[otbr-radio] Using USB radio device: ${RADIO_DEVICE}"

###############################################################################
# 3. Start Avahi daemon (required by otbr-agent — built with OTBR_MDNS=avahi)
###############################################################################
echo "[otbr-radio] Starting avahi-daemon..."
mkdir -p /var/run/avahi-daemon
avahi-daemon --daemonize --no-chroot
echo "[otbr-radio] avahi-daemon started."

###############################################################################
# 4. Configure and start cpcd
#
# Always write a fresh config so we fully control all keys.
# The cpcd installed config uses 'uart_device_file' (not 'uart_device'), so
# patching the existing file would silently fail leaving the wrong device path.
###############################################################################
echo "[otbr-radio] Writing cpcd config to ${CPCD_CONF}..."
mkdir -p "$(dirname "${CPCD_CONF}")"
cat > "${CPCD_CONF}" <<EOF
# cpcd configuration — written at container startup by otbr-radio-entrypoint.sh
instance_name: cpcd_0
bus_type: UART
uart_device_file: ${RADIO_DEVICE}
uart_device_baud: 115200
uart_hardflow: true
EOF

echo "[otbr-radio] Starting cpcd (instance: cpcd_0, device: ${RADIO_DEVICE})..."
CPCD_LOG="/tmp/cpcd.log"
: > "${CPCD_LOG}"
cpcd --conf "${CPCD_CONF}" > >(tee "${CPCD_LOG}") 2>&1 &
CPCD_PID=$!

# Wait until cpcd logs its ready message, meaning it has fully connected to the
# secondary and is accepting client connections.
CPCD_WAIT_MAX=30
cpcdWaitCount=0
echo "[otbr-radio] Waiting for cpcd to be ready..."
while ! grep -q "Daemon startup was successful" "${CPCD_LOG}" 2>/dev/null; do
    if ! kill -0 ${CPCD_PID} 2>/dev/null; then
        echo "[otbr-radio] ERROR: cpcd exited before becoming ready. Check device and firmware." >&2
        cat "${CPCD_LOG}" >&2
        exit 1
    fi
    if [ ${cpcdWaitCount} -ge ${CPCD_WAIT_MAX} ]; then
        echo "[otbr-radio] ERROR: cpcd did not become ready after ${CPCD_WAIT_MAX}s." >&2
        cat "${CPCD_LOG}" >&2
        exit 1
    fi
    sleep 1
    cpcdWaitCount=$((cpcdWaitCount + 1))
done
echo "[otbr-radio] cpcd ready (PID ${CPCD_PID}, waited ${cpcdWaitCount}s)."

###############################################################################
# 5. Start otbr-agent
#
# Uses spinel+cpc:// instead of the simulated spinel+hdlc+forkpty:// URI
# that is used by scripts/start-simulated-otbr.sh.
###############################################################################
echo "[otbr-radio] Starting otbr-agent (backbone: ${BACKBONE_IF})..."
exec otbr-agent \
    -I wpan0 \
    -B "${BACKBONE_IF}" \
    -d 7 \
    -v \
    "spinel+cpc://cpcd_0?iid=1&iid-list=0"
