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
# clear error message.
RADIO_DEVICE="${RADIO_DEVICE}"
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
# 2. Validate the USB radio device
###############################################################################
if [ -z "${RADIO_DEVICE}" ]; then
    echo "[otbr-radio] ERROR: RADIO_DEVICE is not set." >&2
    echo "[otbr-radio]        Set it in docker/.env or export before starting:" >&2
    echo "[otbr-radio]            export RADIO_DEVICE=/dev/ttyACM0" >&2
    echo "[otbr-radio]        See docs/REMOTE_RADIO_FOR_DEVELOPMENT.md for setup instructions." >&2
    exit 1
fi
if [ ! -e "${RADIO_DEVICE}" ]; then
    echo "[otbr-radio] ERROR: USB radio device '${RADIO_DEVICE}' not found." >&2
    echo "[otbr-radio]        Check that the thread radio is connected and USB-IP is attached." >&2
    echo "[otbr-radio]        See docs/REMOTE_RADIO_FOR_DEVELOPMENT.md for setup instructions." >&2
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

            # Allow the kernel's initial HCI probe to complete before we send
            # any commands.  The kernel sends HCI_Reset, Read_Local_Features,
            # etc. immediately after btattach registers the device.  Sending
            # our own commands too early races with those and can leave the
            # Silicon Labs CPC-BLE bridge in a broken state where LE scanning
            # returns "Command Disallowed" or I/O errors.
            echo "[otbr-radio] Waiting for kernel HCI initialization to settle..."
            sleep 3

            # Bring the device up cleanly.  Do NOT use hcitool here — it uses
            # the legacy HCI socket which permanently poisons the MGMT layer.
            echo "[otbr-radio] Bringing ${radioHci} up..."
            nsenter --net="${HOST_NETNS}" hciconfig "${radioHci}" up 2>/dev/null || true
            sleep 1

            # Verify the adapter is accessible.
            if ! ble_verify_scan "${radioHci}"; then
                echo "[otbr-radio] WARNING: HCI verification failed after initial up." >&2
                echo "[otbr-radio] Attempting down/up recovery cycle..." >&2
                nsenter --net="${HOST_NETNS}" hciconfig "${radioHci}" down 2>/dev/null || true
                sleep 2
                nsenter --net="${HOST_NETNS}" hciconfig "${radioHci}" up 2>/dev/null || true
                sleep 2

                if ! ble_verify_scan "${radioHci}"; then
                    echo "[otbr-radio] ERROR: HCI still not accessible after recovery." >&2
                    return 1
                fi
            fi

            echo "[otbr-radio] LE scan verification passed for ${radioHci}."

            # Kill any stale bluetoothd before starting a fresh instance.
            pkill -x bluetoothd 2>/dev/null || true
            sleep 0.5
            echo "[otbr-radio] Starting bluetoothd (host netns, private D-Bus)..."
            nsenter --net="${HOST_NETNS}" bluetoothd &
            BLUETOOTHD_PID=$!
            sleep 1

            if ! kill -0 ${BLUETOOTHD_PID} 2>/dev/null; then
                echo "[otbr-radio] ERROR: bluetoothd (PID ${BLUETOOTHD_PID}) exited immediately." >&2
                return 1
            fi

            echo "[otbr-radio] bluetoothd started (PID ${BLUETOOTHD_PID})."

            return 0
        fi
    done

    echo "[otbr-radio] WARNING: btattach did not create a new HCI device within 5s." >&2

    if ! kill -0 ${BTATTACH_PID} 2>/dev/null; then
        echo "[otbr-radio] WARNING: btattach (PID ${BTATTACH_PID}) already exited." >&2
    fi

    return 1
}

# ble_verify_scan — test that the HCI adapter exists and is responsive.
# Returns 0 if the adapter is detected by hciconfig, 1 if it fails.
# Uses a hard 4-second timeout to avoid blocking if the CPC-BLE bridge is
# completely unresponsive.
# IMPORTANT: Do NOT use hcitool lescan here.  hcitool uses the legacy HCI socket
# interface which permanently conflicts with bluetoothd's MGMT socket — starting
# an LE scan via hcitool and killing it leaves the MGMT layer stuck, causing all
# subsequent StartDiscovery calls to fail with "InProgress".
ble_verify_scan() {
    local hciDev="$1"
    local output
    output=$(timeout 4 nsenter --net="${HOST_NETNS}" hciconfig "${hciDev}" 2>&1) || true

    if echo "${output}" | grep -qi "no such device"; then
        echo "[otbr-radio] ble_verify_scan: device not found — ${output}" >&2
        return 1
    fi

    # As long as hciconfig recognizes the device, the btattach→HCI chain
    # is functional.  bluetoothd will power it on via MGMT when it starts.
    if ! echo "${output}" | grep -qi "Bus:"; then
        echo "[otbr-radio] ble_verify_scan: unexpected output — ${output}" >&2
        return 1
    fi

    return 0
}

# ble_health_check — Passive D-Bus-based health check suitable for use while
# bluetoothd is running.  Verifies the adapter is reachable and powered via
# D-Bus property reads.  Does NOT start/stop discovery to avoid conflicting
# with Matter SDK BLE scans.  Returns 0 if the adapter is powered, 1 on failure.
ble_health_check() {
    local adapterPath="/org/bluez/${1}"

    # Verify bluetoothd is reachable and adapter is powered.
    local powered
    powered=$(timeout 5 dbus-send --system --dest=org.bluez \
        --print-reply "${adapterPath}" \
        org.freedesktop.DBus.Properties.Get \
        string:org.bluez.Adapter1 string:Powered 2>&1) || true

    if echo "${powered}" | grep -qi "error\|no reply\|not found"; then
        echo "[otbr-radio] ble_health_check: adapter not reachable on D-Bus" >&2
        return 1
    fi

    if ! echo "${powered}" | grep -q "boolean true"; then
        echo "[otbr-radio] ble_health_check: adapter not powered" >&2
        return 1
    fi

    # Verify adapter address is resolvable (validates HCI transport).
    local address
    address=$(timeout 5 dbus-send --system --dest=org.bluez \
        --print-reply "${adapterPath}" \
        org.freedesktop.DBus.Properties.Get \
        string:org.bluez.Adapter1 string:Address 2>&1) || true

    if echo "${address}" | grep -qi "error\|no reply\|not found"; then
        echo "[otbr-radio] ble_health_check: cannot read adapter address" >&2
        return 1
    fi

    return 0
}

# ble_monitor — background loop that monitors BLE health and restarts the
# stack when needed.  Handles two failure modes:
#   1. btattach exits (e.g. USB-IP disconnect/reconnect cycle)
#   2. btattach alive but CPC-BLE bridge in broken state (scan fails)
#
# The health check runs every BLE_HEALTH_INTERVAL seconds and verifies that
# LE scanning works.  If scanning fails, it kills the BLE chain and restarts
# it from the CPC bridge level.
#
# A startup grace period (BLE_HEALTH_GRACE) allows Thread traffic to settle
# after otbr-agent forms/joins the network before the first health check.
# During initial Thread establishment, CPC bandwidth is saturated and BLE
# commands may time out — this is transient, not a real failure.
BLE_HEALTH_INTERVAL=60
BLE_HEALTH_GRACE=90

ble_monitor() {
    local backoff=2
    local healthCounter=0
    local radioHci=""
    local graceRemaining=${BLE_HEALTH_GRACE}

    # Resolve the radio HCI device name from the adapter index file.
    if [ -f "${DBUS_DIR}/ble_adapter_id" ]; then
        radioHci="hci$(cat "${DBUS_DIR}/ble_adapter_id" | tr -d '[:space:]')"
    fi

    while true; do

        if ! kill -0 ${BTATTACH_PID} 2>/dev/null; then
            echo "[otbr-radio] btattach (PID ${BTATTACH_PID}) exited — restarting BLE stack..." >&2

            # If bt_host_cpc_hci_bridge also died, wait for it to come back.
            if ! kill -0 ${BT_BRIDGE_PID} 2>/dev/null; then
                echo "[otbr-radio] bt_host_cpc_hci_bridge also exited; restarting bridge..." >&2
                ble_restart_bridge

                if ! kill -0 ${BT_BRIDGE_PID} 2>/dev/null; then
                    echo "[otbr-radio] Bridge restart failed; retrying in ${backoff}s..." >&2
                    sleep "${backoff}"

                    if [ ${backoff} -lt 30 ]; then
                        backoff=$((backoff * 2))
                    fi

                    continue
                fi
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
                healthCounter=0

                # Update the HCI device name after restart (index may change).
                if [ -f "${DBUS_DIR}/ble_adapter_id" ]; then
                    radioHci="hci$(cat "${DBUS_DIR}/ble_adapter_id" | tr -d '[:space:]')"
                fi
            else
                echo "[otbr-radio] BLE stack restart failed; retrying in ${backoff}s..." >&2

                if [ ${backoff} -lt 30 ]; then
                    backoff=$((backoff * 2))
                fi
            fi
        else
            # btattach is alive — periodically verify scanning still works.
            # Skip health checks during the startup grace period.
            if [ ${graceRemaining} -gt 0 ]; then
                graceRemaining=$((graceRemaining - 1))
            else
                healthCounter=$((healthCounter + 1))

                if [ ${healthCounter} -ge ${BLE_HEALTH_INTERVAL} ] && [ -n "${radioHci}" ]; then
                    healthCounter=0

                    if ! ble_health_check "${radioHci}"; then
                        echo "[otbr-radio] BLE health check FAILED — scan broken. Restarting full BLE chain..." >&2
                        # Kill the entire BLE chain.
                        kill ${BTATTACH_PID} 2>/dev/null || true
                        pkill -x bluetoothd 2>/dev/null || true
                        kill ${BT_BRIDGE_PID} 2>/dev/null || true
                        sleep 5

                        # Restart the bridge with retries.
                        ble_restart_bridge
                        sleep 1
                        continue
                    fi
                fi
            fi
        fi

        sleep 1
    done
}

# ble_restart_bridge — restart bt_host_cpc_hci_bridge with retries.
# cpcd may take 5-20s to release the BLE endpoint after the old bridge exits.
# Retry up to 4 times with increasing pre-delays (5s, 10s, 15s, 20s).
ble_restart_bridge() {
    local attempt=0
    local maxAttempts=4

    while [ ${attempt} -lt ${maxAttempts} ]; do
        attempt=$((attempt + 1))
        local delay=$((attempt * 5))
        echo "[otbr-radio] Restarting bt_host_cpc_hci_bridge (attempt ${attempt}/${maxAttempts}, pre-delay ${delay}s)..."

        # Wait for cpcd to release the BLE endpoint.  First attempt gets a
        # short delay; subsequent attempts wait longer.
        sleep ${delay}
        rm -f "${BT_BRIDGE_DIR}/pts_hci"
        cd "${BT_BRIDGE_DIR}"
        bt_host_cpc_hci_bridge &
        BT_BRIDGE_PID=$!

        # Wait for pts_hci symlink.
        local bridgeWait=0

        while [ ! -L "${BT_BRIDGE_DIR}/pts_hci" ] && [ ${bridgeWait} -lt 10 ]; do

            if ! kill -0 ${BT_BRIDGE_PID} 2>/dev/null; then
                break
            fi

            sleep 1
            bridgeWait=$((bridgeWait + 1))
        done

        if [ -L "${BT_BRIDGE_DIR}/pts_hci" ]; then
            echo "[otbr-radio] bt_host_cpc_hci_bridge ready (PID ${BT_BRIDGE_PID})."
            return 0
        fi

        echo "[otbr-radio] bt_host_cpc_hci_bridge attempt ${attempt} failed." >&2
        kill ${BT_BRIDGE_PID} 2>/dev/null || true
    done

    echo "[otbr-radio] ERROR: bt_host_cpc_hci_bridge failed after ${maxAttempts} attempts." >&2
    return 1
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
    "spinel+cpc://cpcd_0?iid=1&iid-list=0"
