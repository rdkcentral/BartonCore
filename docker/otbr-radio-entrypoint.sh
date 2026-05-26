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
#   3. cpcd  (CPC daemon — USB serial ↔ CPC socket)
#      OR CPC socket proxy client (when CPC_REMOTE_HOST is set)
#   4. otbr-agent  (Thread Border Router — CPC socket ↔ D-Bus API)
#
# Environment variables:
#   RADIO_DEVICE              - host path of the USB serial device. The compose
#                               overlay requires this to be set explicitly.
#                               Not required when CPC_REMOTE_HOST is set.
#   CPC_REMOTE_HOST           - when set, connect to a remote cpcd via the CPC
#                               socket proxy instead of running cpcd locally.
#                               The remote host must be running
#                               cpc_remote_access.sh in server mode.
#   CPC_REMOTE_PORT           - base port for the CPC socket proxy
#                               (default: 15000)
#   CPC_INSTANCE              - CPC daemon instance name (default: cpcd_0)
#   BACKBONE_IF               - network interface to use as the Thread backbone,
#                               e.g. eth0
#   CPCD_CONF                 - path to cpcd config file
#   DBUS_DIR                  - private shared D-Bus socket directory
#   DBUS_SOCKET_PATH          - socket path for the private system bus
#   DBUS_SYSTEM_BUS_ADDRESS   - D-Bus address used by otbr-agent and clients
#
# See docs/REMOTE_RADIO_FOR_DEVELOPMENT.md for USB-IP setup and hardware info.
#

set -e

# RADIO_DEVICE is intentionally not defaulted here.
# If it is unset or empty the validation section below will exit with a
# clear error message (unless CPC_REMOTE_HOST is set).
RADIO_DEVICE="${RADIO_DEVICE}"
CPC_REMOTE_HOST="${CPC_REMOTE_HOST:-}"
CPC_REMOTE_PORT="${CPC_REMOTE_PORT:-15000}"
CPC_INSTANCE="${CPC_INSTANCE:-cpcd_0}"
CPCD_CONF="${CPCD_CONF:-/usr/local/etc/cpcd.conf}"
DBUS_DIR="${DBUS_DIR:-/var/run/otbr-dbus}"
DBUS_SOCKET_PATH="${DBUS_SOCKET_PATH:-${DBUS_DIR}/system_bus_socket}"
DBUS_SYSTEM_BUS_ADDRESS="${DBUS_SYSTEM_BUS_ADDRESS:-unix:path=${DBUS_SOCKET_PATH}}"
export DBUS_SYSTEM_BUS_ADDRESS

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
# 1. Start a private D-Bus system bus
#
# The shared 'dbus-socket' named volume is mounted at ${DBUS_DIR}. This is a
# private socket directory used only by the otbr-radio and barton containers;
# it is not the host's /var/run/dbus.
#
# Keep only the socket in the shared volume. Do not create pid files there.
# If the previous container instance exited uncleanly, remove any stale socket
# before starting a new private dbus-daemon bound to the explicit address.
###############################################################################
echo "[otbr-radio] Starting private D-Bus system bus at ${DBUS_SYSTEM_BUS_ADDRESS}..."
mkdir -p "${DBUS_DIR}"
if [ -S "${DBUS_SOCKET_PATH}" ]; then
    echo "[otbr-radio] Removing stale D-Bus socket ${DBUS_SOCKET_PATH}..."
    rm -f "${DBUS_SOCKET_PATH}"
fi
dbus-daemon --config-file=/etc/otbr-dbus.conf --fork --nopidfile
echo "[otbr-radio] Private D-Bus started."

###############################################################################
# 2. Validate radio access and start CPC
#
# Two modes are supported:
#   a) Local radio (default): RADIO_DEVICE must point to a USB serial device.
#      cpcd is started locally to manage the CPC link.
#   b) Remote CPC: CPC_REMOTE_HOST is set.  A CPC socket proxy client connects
#      to a remote cpcd over TCP, creating local Unix sockets that libcpc
#      applications (bt_host_cpc_hci_bridge, otbr-agent) use transparently.
###############################################################################
CPC_SOCKET_DIR="${CPC_SOCKET_DIR:-/dev/shm}"

if [ -n "${CPC_REMOTE_HOST}" ]; then
    #--------------------------------------------------------------------------
    # Remote CPC mode — connect to cpcd running on a remote host via the
    # CPC socket proxy.
    #--------------------------------------------------------------------------
    echo "[otbr-radio] Remote CPC mode: connecting to cpcd on ${CPC_REMOTE_HOST}:${CPC_REMOTE_PORT}"

    CPC_PROXY_SCRIPT="/opt/cpc-proxy/cpc_socket_proxy.py"
    CPC_SOCKET_BASE="${CPC_SOCKET_DIR}/cpcd/${CPC_INSTANCE}"
    mkdir -p "${CPC_SOCKET_BASE}"

    # Start proxy for the control socket.
    echo "[otbr-radio] Starting CPC proxy: ctrl.cpcd.sock (port ${CPC_REMOTE_PORT})"
    python3 "${CPC_PROXY_SCRIPT}" client \
        -s "${CPC_SOCKET_BASE}/ctrl.cpcd.sock" \
        -H "${CPC_REMOTE_HOST}" -p "${CPC_REMOTE_PORT}" &

    # Start proxy for the reset socket.
    CPC_RESET_PORT=$((CPC_REMOTE_PORT + 1))
    echo "[otbr-radio] Starting CPC proxy: reset.cpcd.sock (port ${CPC_RESET_PORT})"
    python3 "${CPC_PROXY_SCRIPT}" client \
        -s "${CPC_SOCKET_BASE}/reset.cpcd.sock" \
        -H "${CPC_REMOTE_HOST}" -p "${CPC_RESET_PORT}" &

    # Start proxies for the Bluetooth RCP endpoint.
    # bt_host_cpc_hci_bridge opens SL_CPC_ENDPOINT_BLUETOOTH_RCP which is 14
    # in cpcd v4.4.6 (host library sl_cpc.h).
    CPC_BT_EP=14
    CPC_BT_DATA_PORT=$((CPC_REMOTE_PORT + 100 + CPC_BT_EP))
    CPC_BT_EVENT_PORT=$((CPC_REMOTE_PORT + 400 + CPC_BT_EP))
    echo "[otbr-radio] Starting CPC proxy: ep${CPC_BT_EP}.cpcd.sock (port ${CPC_BT_DATA_PORT})"
    python3 "${CPC_PROXY_SCRIPT}" client \
        -s "${CPC_SOCKET_BASE}/ep${CPC_BT_EP}.cpcd.sock" \
        -H "${CPC_REMOTE_HOST}" -p "${CPC_BT_DATA_PORT}" &
    echo "[otbr-radio] Starting CPC proxy: ep${CPC_BT_EP}.event.cpcd.sock (port ${CPC_BT_EVENT_PORT})"
    python3 "${CPC_PROXY_SCRIPT}" client \
        -s "${CPC_SOCKET_BASE}/ep${CPC_BT_EP}.event.cpcd.sock" \
        -H "${CPC_REMOTE_HOST}" -p "${CPC_BT_EVENT_PORT}" &

    # Start proxies for the OpenThread/Spinel endpoint.
    # otbr-agent uses spinel+cpc://cpcd_0 which maps to
    # SL_CPC_ENDPOINT_15_4 (12) in cpcd v4.4.6 for multi-PAN RCP firmware.
    CPC_SPINEL_EP=12
    CPC_SPINEL_DATA_PORT=$((CPC_REMOTE_PORT + 100 + CPC_SPINEL_EP))
    CPC_SPINEL_EVENT_PORT=$((CPC_REMOTE_PORT + 400 + CPC_SPINEL_EP))
    echo "[otbr-radio] Starting CPC proxy: ep${CPC_SPINEL_EP}.cpcd.sock (port ${CPC_SPINEL_DATA_PORT})"
    python3 "${CPC_PROXY_SCRIPT}" client \
        -s "${CPC_SOCKET_BASE}/ep${CPC_SPINEL_EP}.cpcd.sock" \
        -H "${CPC_REMOTE_HOST}" -p "${CPC_SPINEL_DATA_PORT}" &
    echo "[otbr-radio] Starting CPC proxy: ep${CPC_SPINEL_EP}.event.cpcd.sock (port ${CPC_SPINEL_EVENT_PORT})"
    python3 "${CPC_PROXY_SCRIPT}" client \
        -s "${CPC_SOCKET_BASE}/ep${CPC_SPINEL_EP}.event.cpcd.sock" \
        -H "${CPC_REMOTE_HOST}" -p "${CPC_SPINEL_EVENT_PORT}" &

    # Give the proxies a moment to establish connections.
    sleep 2

    # Verify the control socket was created.
    if [ ! -S "${CPC_SOCKET_BASE}/ctrl.cpcd.sock" ]; then
        echo "[otbr-radio] ERROR: CPC proxy failed to create ctrl.cpcd.sock." >&2
        echo "[otbr-radio]        Check that ${CPC_REMOTE_HOST}:${CPC_REMOTE_PORT} is reachable" >&2
        echo "[otbr-radio]        and cpc_remote_access.sh server is running." >&2
        exit 1
    fi
    echo "[otbr-radio] CPC proxy client connected to ${CPC_REMOTE_HOST}."

else
    #--------------------------------------------------------------------------
    # Local radio mode — validate RADIO_DEVICE and start cpcd.
    #--------------------------------------------------------------------------
    if [ -z "${RADIO_DEVICE}" ]; then
        echo "[otbr-radio] ERROR: Neither RADIO_DEVICE nor CPC_REMOTE_HOST is set." >&2
        echo "[otbr-radio]        Set RADIO_DEVICE for a local radio:" >&2
        echo "[otbr-radio]            export RADIO_DEVICE=/dev/ttyACM0" >&2
        echo "[otbr-radio]        Or set CPC_REMOTE_HOST for a remote cpcd:" >&2
        echo "[otbr-radio]            export CPC_REMOTE_HOST=10.0.0.1" >&2
        exit 1
    fi

    if [ ! -e "${RADIO_DEVICE}" ]; then
        echo "[otbr-radio] ERROR: USB radio device '${RADIO_DEVICE}' not found." >&2
        echo "[otbr-radio]        Check that the thread radio is connected and USB-IP is attached." >&2
        echo "[otbr-radio]        See docs/REMOTE_RADIO_FOR_DEVELOPMENT.md for setup instructions." >&2
        exit 1
    fi
    echo "[otbr-radio] Using USB radio device: ${RADIO_DEVICE}"
fi

###############################################################################
# 3. Start Avahi daemon (required by otbr-agent — built with OTBR_MDNS=avahi)
###############################################################################
echo "[otbr-radio] Starting avahi-daemon..."
mkdir -p /var/run/avahi-daemon
avahi-daemon --daemonize --no-chroot
echo "[otbr-radio] avahi-daemon started."

###############################################################################
# 4. Configure and start cpcd (local radio mode only)
#
# Always write a fresh config so we fully control all keys.
# The cpcd installed config uses 'uart_device_file' (not 'uart_device'), so
# patching the existing file would silently fail leaving the wrong device path.
#
# Skipped when CPC_REMOTE_HOST is set — the CPC proxy handles connectivity.
###############################################################################
if [ -z "${CPC_REMOTE_HOST}" ]; then
    echo "[otbr-radio] Writing cpcd config to ${CPCD_CONF}..."
    mkdir -p "$(dirname "${CPCD_CONF}")"
    cat > "${CPCD_CONF}" <<EOF
# cpcd configuration — written at container startup by otbr-radio-entrypoint.sh
instance_name: ${CPC_INSTANCE}
bus_type: UART
uart_device_file: ${RADIO_DEVICE}
uart_device_baud: 115200
uart_hardflow: true
EOF

    echo "[otbr-radio] Starting cpcd (instance: ${CPC_INSTANCE}, device: ${RADIO_DEVICE})..."
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
fi

###############################################################################
# 5. Start bt_host_cpc_hci_bridge (CPC → virtual HCI serial device)
#
# Connects to cpcd, opens a CPC Bluetooth endpoint, and creates a numbered
# virtual serial device (e.g. /dev/pts/6) plus a convenience symlink
# 'pts_hci' in the working directory.  BlueZ attaches to this device via
# btattach in the next step.
###############################################################################
echo "[otbr-radio] Starting bt_host_cpc_hci_bridge..."
BT_BRIDGE_DIR="/var/run/bt-hci-bridge"
mkdir -p "${BT_BRIDGE_DIR}"
# Remove any stale symlink from a previous run so the wait loop below
# blocks until bt_host_cpc_hci_bridge creates a fresh one.
rm -f "${BT_BRIDGE_DIR}/pts_hci"
cd "${BT_BRIDGE_DIR}"
bt_host_cpc_hci_bridge &
BT_BRIDGE_PID=$!

# Wait for the pts_hci symlink that bt_host_cpc_hci_bridge creates once ready.
BT_BRIDGE_WAIT_MAX=15
btBridgeWaitCount=0
while [ ! -L "${BT_BRIDGE_DIR}/pts_hci" ]; do
    if ! kill -0 ${BT_BRIDGE_PID} 2>/dev/null; then
        echo "[otbr-radio] ERROR: bt_host_cpc_hci_bridge exited unexpectedly." >&2
        exit 1
    fi

    if [ ${btBridgeWaitCount} -ge ${BT_BRIDGE_WAIT_MAX} ]; then
        echo "[otbr-radio] ERROR: bt_host_cpc_hci_bridge did not create pts_hci after ${BT_BRIDGE_WAIT_MAX}s." >&2
        exit 1
    fi
    sleep 1
    btBridgeWaitCount=$((btBridgeWaitCount + 1))
done
PTS_DEVICE=$(readlink -f "${BT_BRIDGE_DIR}/pts_hci")
echo "[otbr-radio] bt_host_cpc_hci_bridge ready (PID ${BT_BRIDGE_PID}, pts: ${PTS_DEVICE}, waited ${btBridgeWaitCount}s)."

###############################################################################
# 5b. Start HCI PTY proxy (strip LE Extended Advertising feature bit)
#
# The Silicon Labs CPC BLE firmware reports Extended Advertising support but
# responds to Extended Scan Disable (0x2042) with the legacy opcode (0x200c).
# The kernel treats this as unexpected, times out, and cannot stop scanning —
# which prevents LE connections from being established.
#
# This proxy sits between bt_host_cpc_hci_bridge and btattach, modifying the
# LE Read Local Supported Features response to clear the Extended Advertising
# bit.  The kernel then uses legacy BLE commands that the firmware handles
# correctly.
###############################################################################
HCI_PROXY_SCRIPT="/opt/cpc-proxy/hci_pty_proxy.py"

if [ -f "${HCI_PROXY_SCRIPT}" ]; then
    echo "[otbr-radio] Starting HCI PTY proxy (strip Extended Advertising)..."
    python3 "${HCI_PROXY_SCRIPT}" "${PTS_DEVICE}" "${BT_BRIDGE_DIR}/pts_hci" &
    HCI_PROXY_PID=$!
    sleep 1

    if ! kill -0 ${HCI_PROXY_PID} 2>/dev/null; then
        echo "[otbr-radio] WARNING: HCI PTY proxy exited early — continuing without it." >&2
    else
        # btattach will now open the proxy's PTY instead of the original.
        PTS_DEVICE=$(readlink -f "${BT_BRIDGE_DIR}/pts_hci")
        echo "[otbr-radio] HCI PTY proxy ready (PID ${HCI_PROXY_PID}, new pts: ${PTS_DEVICE})."
    fi
else
    echo "[otbr-radio] HCI PTY proxy not found — skipping (Extended BLE commands may fail)."
fi

###############################################################################
# 6. Attach the virtual HCI device and start bluetoothd
#
# AF_BLUETOOTH sockets are only available in the initial (host) network
# namespace.  btattach and bluetoothd must run in the host netns via nsenter.
#
# bluetoothd inherits the container's DBUS_SYSTEM_BUS_ADDRESS so it registers
# on our private D-Bus, not the host's.  This avoids conflicts with any
# host-side bluetoothd that may be running on the host system bus.
#
# The host machine may have its own built-in Bluetooth adapter (e.g. hci0).
# btattach creates a new HCI device for the radio (e.g. hci1).  We detect
# its index and write it to the shared D-Bus volume so Barton can read it
# at runtime to configure the Matter SDK's BLE adapter selection.
#
# A background monitor watches btattach and restarts the BLE stack if it
# exits (e.g. after a USB-IP disconnect/reconnect cycle).
###############################################################################
HOST_NETNS="/run/host-netns"

# ble_attach — run btattach in the host network namespace against the
# pts_hci device and identify the new HCI adapter.  On success, starts
# bluetoothd and writes ble_adapter_id so Barton picks the right adapter.
# Sets BTATTACH_PID in the caller's scope.
ble_attach() {
    # Snapshot existing HCI devices before btattach creates a new one.
    local hciBefore
    hciBefore=$(ls /sys/class/bluetooth/ 2>/dev/null | sort)

    local ptsDevice
    ptsDevice=$(readlink -f "${BT_BRIDGE_DIR}/pts_hci")

    echo "[otbr-radio] Attaching HCI device ${ptsDevice} via btattach (host netns)..."
    nsenter --net="${HOST_NETNS}" btattach -B "${ptsDevice}" -S 115200 &
    BTATTACH_PID=$!

    # Wait up to 5 seconds for a new HCI device to appear.
    local waited=0

    while [ ${waited} -lt 5 ]; do
        sleep 1
        waited=$((waited + 1))

        local hciAfter
        hciAfter=$(ls /sys/class/bluetooth/ 2>/dev/null | sort)
        local radioHci=""

        for hci in ${hciAfter}; do

            if ! echo "${hciBefore}" | grep -qw "${hci}"; then
                radioHci="${hci}"
                break
            fi
        done

        if [ -n "${radioHci}" ]; then
            local radioHciIndex="${radioHci#hci}"
            echo "[otbr-radio] Radio HCI device: ${radioHci} (index ${radioHciIndex})"
            echo "${radioHciIndex}" > "${DBUS_DIR}/ble_adapter_id"
            echo "[otbr-radio] Wrote BLE adapter index ${radioHciIndex} to ${DBUS_DIR}/ble_adapter_id"

            # Kill any stale bluetoothd before starting a fresh instance.
            pkill -x bluetoothd 2>/dev/null || true
            sleep 0.5

            # Fix the Silicon Labs CPC BLE controller initialization issue.
            #
            # Problem: the kernel's initial HCI probe (triggered by btattach)
            # sends commands that the CPC firmware responds to with malformed
            # responses, leaving the controller in a state where LE scanning
            # returns "Command Disallowed" (0x0C).
            #
            # Solution: cycle the adapter down→up to clear the kernel's stale
            # HCI state, then send a raw HCI Reset to fix the controller.
            # After that, the adapter is in a clean state and bluetoothd can
            # start without conflicts.  Starting bluetoothd AFTER the reset
            # is critical — if bluetoothd runs when the HCI Reset is sent,
            # bluetoothd's internal state becomes inconsistent (InProgress
            # errors on StartDiscovery).
            echo "[otbr-radio] Resetting ${radioHci} (down/up/HCI Reset)..."
            nsenter --net="${HOST_NETNS}" hciconfig "${radioHci}" down 2>/dev/null || true
            sleep 0.5
            nsenter --net="${HOST_NETNS}" hciconfig "${radioHci}" up 2>/dev/null || true
            sleep 0.5
            nsenter --net="${HOST_NETNS}" hcitool -i "${radioHci}" cmd 0x03 0x0003 >/dev/null 2>&1 || true
            sleep 0.5

            echo "[otbr-radio] Starting bluetoothd (host netns, private D-Bus)..."
            nsenter --net="${HOST_NETNS}" bluetoothd &
            sleep 3
            echo "[otbr-radio] BLE stack ready (${radioHci})."

            return 0
        fi
    done

    echo "[otbr-radio] WARNING: btattach did not create a new HCI device within 5s." >&2

    if ! kill -0 ${BTATTACH_PID} 2>/dev/null; then
        echo "[otbr-radio] WARNING: btattach (PID ${BTATTACH_PID}) already exited." >&2
    fi

    return 1
}

# ble_monitor — background loop that restarts btattach (and bluetoothd)
# whenever btattach exits.  This handles USB-IP disconnect/reconnect
# cycles where cpcd and bt_host_cpc_hci_bridge survive but btattach
# loses the HCI device and exits.
ble_monitor() {
    local backoff=2

    while true; do

        if ! kill -0 ${BTATTACH_PID} 2>/dev/null; then
            echo "[otbr-radio] btattach (PID ${BTATTACH_PID}) exited — restarting BLE stack..." >&2

            # If bt_host_cpc_hci_bridge also died, wait for it to come back.
            if ! kill -0 ${BT_BRIDGE_PID} 2>/dev/null; then
                echo "[otbr-radio] bt_host_cpc_hci_bridge also exited; waiting for it to restart..." >&2
                sleep "${backoff}"
                continue
            fi

            # pts_hci must still be valid.
            if [ ! -L "${BT_BRIDGE_DIR}/pts_hci" ]; then
                echo "[otbr-radio] pts_hci symlink missing; waiting..." >&2
                sleep "${backoff}"
                continue
            fi

            if ble_attach; then
                echo "[otbr-radio] BLE stack restarted successfully."
                backoff=2
            else
                echo "[otbr-radio] BLE stack restart failed; retrying in ${backoff}s..." >&2

                if [ ${backoff} -lt 30 ]; then
                    backoff=$((backoff * 2))
                fi
            fi
        fi

        sleep "${backoff}"
    done
}

if [ ! -e "${HOST_NETNS}" ]; then
    echo "[otbr-radio] WARNING: Host network namespace not mounted at ${HOST_NETNS}." >&2
    echo "[otbr-radio]          Bluetooth support will not be available." >&2
    echo "[otbr-radio]          Add '- /proc/1/ns/net:/run/host-netns:ro' to volumes." >&2
else
    BTATTACH_PID=0

    if ble_attach; then
        echo "[otbr-radio] BLE stack initialized."
    else
        echo "[otbr-radio] WARNING: Initial BLE attach failed; monitor will retry." >&2
    fi

    # Start background monitor to handle USB-IP reconnects.
    ble_monitor &
    BLE_MONITOR_PID=$!
    echo "[otbr-radio] BLE monitor started (PID ${BLE_MONITOR_PID})."
fi

###############################################################################
# 7. Start otbr-agent
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
    "spinel+cpc://${CPC_INSTANCE}?iid=1&iid-list=0"
