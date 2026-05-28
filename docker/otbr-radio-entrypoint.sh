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
#   4. otbr-agent  (Thread Border Router — CPC socket ↔ D-Bus API)
#
# Environment variables:
#   RADIO_DEVICE              - host path of the USB serial device. The compose
#                               overlay requires this to be set explicitly.
#                               Not required when RADIO_HOST/RADIO_PORT are set.
#   RADIO_HOST                - when set (with RADIO_PORT), connect to a remote
#                               serial tunnel instead of using a local device.
#                               The workstation runs remote-serial.py to forward
#                               its serial port via SSH.
#   RADIO_PORT                - TCP port for the remote serial tunnel.
#   CPC_INSTANCE              - CPC daemon instance name (default: cpcd_0)
#   BACKBONE_IF               - network interface to use as the Thread backbone,
#                               e.g. eth0
#   CPCD_CONF                 - path to cpcd config file
#   DBUS_DIR                  - private shared D-Bus socket directory
#   DBUS_SOCKET_PATH          - socket path for the private system bus
#   DBUS_SYSTEM_BUS_ADDRESS   - D-Bus address used by otbr-agent and clients
#
# See docs/REMOTE_RADIO_FOR_DEVELOPMENT.md for setup and hardware info.
#

set -e

# RADIO_DEVICE is intentionally not defaulted here.
# If it is unset or empty the validation section below will exit with a
# clear error message (unless RADIO_HOST/RADIO_PORT are set for remote serial).
RADIO_DEVICE="${RADIO_DEVICE:-}"
RADIO_HOST="${RADIO_HOST:-host.docker.internal}"
RADIO_PORT="${RADIO_PORT:-}"
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

# Accept Router Advertisements even with forwarding=1.
# The kernel drops RAs when forwarding is enabled unless accept_ra=2.
# This is an interface-specific sysctl that cannot be set via Docker Compose
# sysctls (Docker requires the driver option syntax for those), so we set it
# here since the container is privileged.
sysctl -qw "net.ipv6.conf.${BACKBONE_IF}.accept_ra=2" 2>/dev/null || true

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
#   b) Remote serial tunnel: RADIO_HOST and RADIO_PORT are set.  A socat
#      process bridges the remote TCP serial tunnel to a local PTY that cpcd
#      opens as if it were a directly-attached USB serial device.
#      The workstation runs scripts/remote-radio/remote-serial.py to forward
#      its local serial port over an SSH tunnel to this container.
###############################################################################

if [ -n "${RADIO_PORT}" ]; then
    #--------------------------------------------------------------------------
    # Remote serial tunnel mode — create a virtual serial device via socat
    # that connects to the SSH-tunnelled serial port on the workstation.
    # RADIO_HOST defaults to host.docker.internal (Docker's built-in DNS
    # for the host machine).
    #--------------------------------------------------------------------------
    echo "[otbr-radio] Remote serial mode: connecting to ${RADIO_HOST}:${RADIO_PORT}"

    VIRTUAL_TTY="/dev/ttyRadio"

    # Resolve RADIO_HOST to an IPv4 address.  The SSH reverse tunnel binds on
    # 0.0.0.0 (IPv4 only), so we must connect via IPv4 even when the Docker
    # host-gateway resolves to an IPv6 address.
    RADIO_HOST_V4=$(getent ahostsv4 "${RADIO_HOST}" 2>/dev/null | awk '{print $1; exit}')
    if [ -z "${RADIO_HOST_V4}" ]; then
        echo "[otbr-radio] WARNING: Could not resolve ${RADIO_HOST} to IPv4; using as-is"
        RADIO_HOST_V4="${RADIO_HOST}"
    else
        echo "[otbr-radio] Resolved ${RADIO_HOST} → ${RADIO_HOST_V4} (IPv4)"
    fi

    # socat creates a PTY linked to the remote TCP stream.  Retry with
    # backoff until the SSH tunnel is reachable.  The 'forever' and
    # 'intervall' options make socat reconnect automatically if the TCP
    # connection drops (self-healing).
    SOCAT_WAIT_MAX=60
    socatWaitCount=0
    echo "[otbr-radio] Waiting for remote serial tunnel at ${RADIO_HOST_V4}:${RADIO_PORT}..."

    while ! timeout 2 bash -c ": >/dev/tcp/${RADIO_HOST_V4}/${RADIO_PORT}" 2>/dev/null; do

        if [ ${socatWaitCount} -ge ${SOCAT_WAIT_MAX} ]; then
            echo "[otbr-radio] ERROR: Remote serial tunnel at ${RADIO_HOST_V4}:${RADIO_PORT} not reachable after ${SOCAT_WAIT_MAX}s." >&2
            echo "[otbr-radio]        Ensure remote-serial.py is running on your workstation." >&2
            echo "[otbr-radio]        See docs/REMOTE_RADIO_FOR_DEVELOPMENT.md for setup instructions." >&2
            exit 1
        fi

        sleep 2
        socatWaitCount=$((socatWaitCount + 2))
    done

    echo "[otbr-radio] Remote serial tunnel is reachable.  Starting socat bridge..."
    rm -f "${VIRTUAL_TTY}"

    # Start socat:  TCP connection → PTY device
    #   - TCP4: force IPv4 (the SSH reverse tunnel binds on 0.0.0.0, not [::])
    #   - b115200: match the radio baud rate
    #   - raw,echo=0: pass bytes through unmodified
    #   - link=: create a named symlink to the PTY
    #   - forever,intervall=3: reconnect on TCP drops every 3s (self-healing)
    #   NOTE: do NOT use wait-slave — it ties socat's TCP lifecycle to the
    #   PTY slave state.  If cpcd briefly closes/reopens the device during
    #   CPC init retries, wait-slave causes socat to disconnect TCP and
    #   destroy the relay connection.
    socat \
        "TCP4:${RADIO_HOST_V4}:${RADIO_PORT},forever,intervall=3,keepalive,keepidle=10,keepintvl=5,keepcnt=3" \
        "PTY,link=${VIRTUAL_TTY},raw,echo=0,b115200" &
    SOCAT_PID=$!

    # Wait for the PTY symlink to appear.
    socatPtyWait=0

    while [ ! -L "${VIRTUAL_TTY}" ]; do

        if ! kill -0 ${SOCAT_PID} 2>/dev/null; then
            echo "[otbr-radio] ERROR: socat exited before creating ${VIRTUAL_TTY}." >&2
            exit 1
        fi

        if [ ${socatPtyWait} -ge 10 ]; then
            echo "[otbr-radio] ERROR: socat did not create ${VIRTUAL_TTY} after 10s." >&2
            exit 1
        fi

        sleep 1
        socatPtyWait=$((socatPtyWait + 1))
    done

    RADIO_DEVICE="${VIRTUAL_TTY}"
    echo "[otbr-radio] Virtual serial device ready: ${RADIO_DEVICE} (via ${RADIO_HOST_V4}:${RADIO_PORT})"

elif [ -n "${RADIO_DEVICE}" ]; then
    #--------------------------------------------------------------------------
    # Local radio mode — validate RADIO_DEVICE directly.
    #--------------------------------------------------------------------------
    if [ ! -e "${RADIO_DEVICE}" ]; then
        echo "[otbr-radio] ERROR: USB radio device '${RADIO_DEVICE}' not found." >&2
        echo "[otbr-radio]        Check that the radio is connected." >&2
        echo "[otbr-radio]        See docs/REMOTE_RADIO_FOR_DEVELOPMENT.md for setup instructions." >&2
        exit 1
    fi

    echo "[otbr-radio] Using USB radio device: ${RADIO_DEVICE}"

else
    echo "[otbr-radio] ERROR: No radio configured." >&2
    echo "[otbr-radio]        Set RADIO_DEVICE for a locally attached radio:" >&2
    echo "[otbr-radio]            export RADIO_DEVICE=/dev/ttyACM0" >&2
    echo "[otbr-radio]        Or set RADIO_PORT for a remote serial tunnel:" >&2
    echo "[otbr-radio]            export RADIO_PORT=21000" >&2
    exit 1
fi

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
#
# In remote serial tunnel mode, RADIO_DEVICE has been set to the socat-created
# PTY (/dev/ttyRadio) by step 2 above.  cpcd treats it identically to a
# directly-attached USB serial device.
#
# Note: even for remote serial tunnels, uart_hardflow must be 'true' because
# the radio's firmware has hardware flow control enabled.  cpcd checks the
# radio's capability flags during init and exits with FATAL if there is a
# mismatch.  On the PTY, CRTSCTS is accepted by the kernel but is a no-op
# (no physical RTS/CTS lines).  Actual flow control happens on the physical
# serial port at the workstation relay side.
###############################################################################

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
# stdbuf -oL forces line-buffered stdout so 'grep' can detect the ready
# message promptly even though cpcd's output is going through a pipe.
stdbuf -oL cpcd --conf "${CPCD_CONF}" > >(tee "${CPCD_LOG}") 2>&1 &
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
# Remote CPC (over TCP proxy) may take longer to negotiate the endpoint.
BT_BRIDGE_WAIT_MAX=30
btBridgeWaitCount=0
while [ ! -L "${BT_BRIDGE_DIR}/pts_hci" ]; do
    if ! kill -0 ${BT_BRIDGE_PID} 2>/dev/null; then
        echo "[otbr-radio] WARNING: bt_host_cpc_hci_bridge exited during startup — BLE monitor will retry." >&2
        break
    fi

    if [ ${btBridgeWaitCount} -ge ${BT_BRIDGE_WAIT_MAX} ]; then
        echo "[otbr-radio] WARNING: bt_host_cpc_hci_bridge did not create pts_hci after ${BT_BRIDGE_WAIT_MAX}s — BLE monitor will retry." >&2
        kill ${BT_BRIDGE_PID} 2>/dev/null || true
        break
    fi
    sleep 1
    btBridgeWaitCount=$((btBridgeWaitCount + 1))
done

# Only proceed with the proxy if the bridge actually created pts_hci.
# If the bridge failed during startup, the BLE monitor will retry later.
if [ -L "${BT_BRIDGE_DIR}/pts_hci" ]; then

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
    python3 -u "${HCI_PROXY_SCRIPT}" "${PTS_DEVICE}" "${BT_BRIDGE_DIR}/pts_hci" &
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

fi  # end: bridge created pts_hci successfully

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
# btattach creates a new HCI device for the CPC radio (e.g. hci1).  We detect
# its index and write it to the shared D-Bus volume so Barton can read it
# at runtime to configure the Matter SDK's BLE adapter selection.
#
# To prevent Matter/bluetoothd from accidentally using the host's built-in
# adapter, we explicitly power down every non-CPC HCI device in the host
# network namespace after btattach creates the CPC device.
#
# A background monitor watches the full BLE chain (bridge, proxy, btattach)
# and restarts it if any component dies.
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

            # Log any non-CPC adapters present.  We do NOT disable them
            # (that would be disruptive to the host).  Instead we rely on
            # ble_adapter_id to steer Matter/bluetoothd to the right one.
            for hostHci in $(ls /sys/class/bluetooth/ 2>/dev/null); do
                if [ "${hostHci}" != "${radioHci}" ]; then
                    echo "[otbr-radio] NOTE: host adapter ${hostHci} also present (not CPC bridge)."
                fi
            done

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

# ble_restart_bridge — restart the entire BLE chain from the CPC bridge
# onwards (bridge → HCI proxy → btattach → bluetoothd).  Called by the
# monitor when bt_host_cpc_hci_bridge dies.
ble_restart_bridge() {
    echo "[otbr-radio] Restarting full BLE chain (bridge → proxy → btattach)..."

    # Tear down everything downstream and reap zombies.
    [ -n "${BTATTACH_PID:-}" ] && kill ${BTATTACH_PID} 2>/dev/null || true
    [ -n "${HCI_PROXY_PID:-}" ] && kill ${HCI_PROXY_PID} 2>/dev/null || true
    pkill -x bluetoothd 2>/dev/null || true
    wait ${BT_BRIDGE_PID} 2>/dev/null || true
    [ -n "${BTATTACH_PID:-}" ] && wait ${BTATTACH_PID} 2>/dev/null || true
    [ -n "${HCI_PROXY_PID:-}" ] && wait ${HCI_PROXY_PID} 2>/dev/null || true
    sleep 1

    # Bring down any stale HCI devices that were created by previous
    # btattach instances.  This prevents hci index accumulation
    # (hci1, hci2, hci3...) across restarts.
    local adapterFile="${DBUS_DIR}/ble_adapter_id"
    if [ -f "$adapterFile" ]; then
        local oldIdx
        oldIdx=$(cat "$adapterFile" 2>/dev/null)
        if [ -n "$oldIdx" ]; then
            nsenter --net="${HOST_NETNS}" hciconfig "hci${oldIdx}" down 2>/dev/null || true
        fi
    fi

    # Start a fresh bridge.  The CPC endpoint (ep14) may take time to be
    # released by cpcd after the previous session ended.  Retry up to
    # BT_BRIDGE_RETRY_MAX times with a short delay to ride out transient
    # EAGAIN / "Resource temporarily unavailable" failures.
    local BT_BRIDGE_RETRY_MAX=5
    local bridgeAttempt=0

    while [ ${bridgeAttempt} -lt ${BT_BRIDGE_RETRY_MAX} ]; do
        rm -f "${BT_BRIDGE_DIR}/pts_hci"
        cd "${BT_BRIDGE_DIR}"
        bt_host_cpc_hci_bridge &
        BT_BRIDGE_PID=$!

        local waited=0

        while [ ! -L "${BT_BRIDGE_DIR}/pts_hci" ]; do

            if ! kill -0 ${BT_BRIDGE_PID} 2>/dev/null; then
                break
            fi

            if [ ${waited} -ge ${BT_BRIDGE_WAIT_MAX} ]; then
                kill ${BT_BRIDGE_PID} 2>/dev/null || true
                break
            fi

            sleep 1
            waited=$((waited + 1))
        done

        if [ -L "${BT_BRIDGE_DIR}/pts_hci" ]; then
            break  # Bridge started successfully.
        fi

        # Bridge failed — wait for zombie and retry.
        wait ${BT_BRIDGE_PID} 2>/dev/null || true
        bridgeAttempt=$((bridgeAttempt + 1))

        if [ ${bridgeAttempt} -lt ${BT_BRIDGE_RETRY_MAX} ]; then
            local retryDelay=$((bridgeAttempt * 3))
            echo "[otbr-radio] Bridge attempt ${bridgeAttempt}/${BT_BRIDGE_RETRY_MAX} failed (EAGAIN?); retrying in ${retryDelay}s..." >&2
            sleep "${retryDelay}"
        fi
    done

    if [ ! -L "${BT_BRIDGE_DIR}/pts_hci" ]; then
        echo "[otbr-radio] bt_host_cpc_hci_bridge failed after ${BT_BRIDGE_RETRY_MAX} attempts." >&2
        return 1
    fi

    PTS_DEVICE=$(readlink -f "${BT_BRIDGE_DIR}/pts_hci")
    echo "[otbr-radio] Bridge restarted (PID ${BT_BRIDGE_PID}, pts: ${PTS_DEVICE})."

    # Restart the HCI PTY proxy if the script is available.
    if [ -f "${HCI_PROXY_SCRIPT}" ]; then
        python3 -u "${HCI_PROXY_SCRIPT}" "${PTS_DEVICE}" "${BT_BRIDGE_DIR}/pts_hci" &
        HCI_PROXY_PID=$!
        sleep 1

        if kill -0 ${HCI_PROXY_PID} 2>/dev/null; then
            PTS_DEVICE=$(readlink -f "${BT_BRIDGE_DIR}/pts_hci")
            echo "[otbr-radio] HCI proxy restarted (PID ${HCI_PROXY_PID}, pts: ${PTS_DEVICE})."
        else
            echo "[otbr-radio] WARNING: HCI proxy exited during restart." >&2
        fi
    fi

    return 0
}

# is_process_alive — check if a process is alive and not a zombie.
# kill -0 returns true for zombie processes, which misleads the monitor
# into thinking a dead bridge is still running.
is_process_alive() {
    local pid=$1
    kill -0 "$pid" 2>/dev/null || return 1
    local state
    state=$(awk '/^State:/ {print $2}' /proc/"$pid"/status 2>/dev/null)
    [[ "$state" != "Z" ]]
}

# hci_transport_healthy — verify the HCI transport is responsive by
# sending a Read BD ADDR command (0x04|0x0009) and checking for a
# valid response.  Returns non-zero if the adapter doesn't respond
# within 5 seconds (dead transport or hung CPC endpoint).
#
# NOTE: The Silicon Labs CPC BLE firmware does NOT support some common
# HCI commands like Read Local Name (0x03|0x0014).  Using those would
# cause false "unhealthy" results.  Read BD ADDR is always supported.
hci_transport_healthy() {
    local adapterFile="${DBUS_DIR}/ble_adapter_id"
    [ -f "$adapterFile" ] || return 1
    local idx
    idx=$(cat "$adapterFile" 2>/dev/null)
    [ -n "$idx" ] || return 1
    # Timeout protects against hung transports.
    timeout 5 nsenter --net="${HOST_NETNS}" \
        hcitool -i "hci${idx}" cmd 0x04 0x0009 >/dev/null 2>&1
}

# ble_monitor — background loop that watches the full BLE chain
# (bridge, HCI proxy, btattach) and restarts from the point of failure.
# If the bridge dies, the entire chain is restarted.  If only btattach
# dies (e.g. USB-IP reconnect), only the attach step is retried.
# Additionally performs periodic HCI transport health checks to catch
# cases where the bridge process is alive but the CPC endpoint died.
ble_monitor() {
    local backoff=2
    local healthCheckInterval=0
    local healthFailCount=0
    # Grace period — skip health checks for the first 30 seconds after
    # startup or a successful restart to let the adapter stabilise.
    local graceUntil=$((SECONDS + 30))

    while true; do
        local needRestart=false
        local bridgeDied=false

        if ! is_process_alive ${BT_BRIDGE_PID}; then
            needRestart=true
            bridgeDied=true
        elif [ -n "${HCI_PROXY_PID:-}" ] && ! is_process_alive ${HCI_PROXY_PID}; then
            # The proxy owns the PTY that btattach is connected to.
            # If the proxy dies, the PTY becomes invalid — full restart.
            echo "[otbr-radio] HCI PTY proxy (PID ${HCI_PROXY_PID}) died." >&2
            needRestart=true
            bridgeDied=true
            kill ${BT_BRIDGE_PID} 2>/dev/null || true
        elif [ -n "${BTATTACH_PID:-}" ] && ! is_process_alive ${BTATTACH_PID}; then
            needRestart=true
        elif [ ${SECONDS} -ge ${graceUntil} ] && [ ${healthCheckInterval} -ge 5 ]; then
            # Every ~10s (5 iterations × 2s sleep), verify the HCI
            # transport is actually responsive.  This catches dead CPC
            # endpoints where the bridge PID is still alive.
            # Require 3 consecutive failures to avoid false positives
            # from transient slowness.
            healthCheckInterval=0

            if ! hci_transport_healthy; then
                healthFailCount=$((healthFailCount + 1))

                if [ ${healthFailCount} -ge 3 ]; then
                    echo "[otbr-radio] HCI transport unresponsive (${healthFailCount} consecutive failures) — restarting BLE chain." >&2
                    needRestart=true
                    bridgeDied=true
                    healthFailCount=0
                    # Kill the stale bridge so ble_restart_bridge can start fresh.
                    kill ${BT_BRIDGE_PID} 2>/dev/null || true
                else
                    echo "[otbr-radio] HCI health check failed (${healthFailCount}/3)." >&2
                fi
            else
                healthFailCount=0
            fi
        else
            healthCheckInterval=$((healthCheckInterval + 1))
        fi

        if ${needRestart}; then
            healthCheckInterval=0
            healthFailCount=0

            if ${bridgeDied}; then
                echo "[otbr-radio] bt_host_cpc_hci_bridge (PID ${BT_BRIDGE_PID}) died." >&2

                if ! ble_restart_bridge; then
                    backoff=$((backoff < 30 ? backoff * 2 : 30))
                    echo "[otbr-radio] Bridge restart failed; retrying in ${backoff}s..." >&2
                    sleep "${backoff}"
                    continue
                fi
            else
                echo "[otbr-radio] btattach (PID ${BTATTACH_PID}) died — restarting BLE attach..." >&2
            fi

            if ble_attach; then
                echo "[otbr-radio] BLE stack restarted successfully."
                backoff=2
                graceUntil=$((SECONDS + 30))
            else
                backoff=$((backoff < 30 ? backoff * 2 : 30))
                echo "[otbr-radio] BLE restart failed; retrying in ${backoff}s..." >&2
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
    BTATTACH_PID=
    HCI_PROXY_PID=${HCI_PROXY_PID:-}

    if [ -L "${BT_BRIDGE_DIR}/pts_hci" ]; then

        if ble_attach; then
            echo "[otbr-radio] BLE stack initialized."
        else
            echo "[otbr-radio] WARNING: Initial BLE attach failed; monitor will retry." >&2
        fi
    else
        echo "[otbr-radio] WARNING: Bridge did not create pts_hci during startup; monitor will retry." >&2
    fi

    # Start background monitor to handle bridge/btattach failures.
    ble_monitor &
    BLE_MONITOR_PID=$!
    echo "[otbr-radio] BLE monitor started (PID ${BLE_MONITOR_PID})."
fi

###############################################################################
# 7. Start otbr-agent with auto-restart
#
# Uses spinel+cpc:// instead of the simulated spinel+hdlc+forkpty:// URI
# that is used by scripts/start-simulated-otbr.sh.
#
# otbr-agent runs in the foreground inside a restart loop so the container
# survives transient CPC failures (e.g. cpcd restart, firmware EAGAIN).
# The loop backs off exponentially up to 30 seconds between retries.
###############################################################################
echo "[otbr-radio] Starting otbr-agent (backbone: ${BACKBONE_IF})..."

otbr_backoff=2

while true; do
    otbr_start=$(date +%s)
    otbr-agent \
        -I wpan0 \
        -B "${BACKBONE_IF}" \
        -d 7 \
        -v \
        "spinel+cpc://${CPC_INSTANCE}?iid=1&iid-list=0" &
    OTBR_PID=$!
    set +e
    wait ${OTBR_PID} 2>/dev/null
    otbr_exit=$?
    set -e

    # Reset backoff if otbr-agent ran for at least 60 seconds (not a
    # crash loop).
    otbr_elapsed=$(( $(date +%s) - otbr_start ))

    if [ "${otbr_elapsed}" -ge 60 ]; then
        otbr_backoff=2
    fi

    echo "[otbr-radio] otbr-agent exited (code ${otbr_exit}) after ${otbr_elapsed}s; restarting in ${otbr_backoff}s..." >&2
    sleep "${otbr_backoff}"
    otbr_backoff=$((otbr_backoff < 30 ? otbr_backoff * 2 : 30))
done
