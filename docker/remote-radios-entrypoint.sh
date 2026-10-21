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
# Entrypoint for the remote-radios container.  It manages two independent
# physical radios that a developer has forwarded from their workstation:
#
#   * Silicon Labs Zigbee/Thread radio — reached over a serial-over-SSH tunnel.
#     The workstation runs remote-serial.py, which relays the radio's serial
#     port to a per-user UNIX socket on the dev server (via `ssh -R`).  That
#     socket is bind-mounted into this container; socat bridges it to a local
#     PTY that cpcd opens as if it were a directly-attached USB serial device.
#
#   * Dedicated Bluetooth USB dongle — reached over usb-ip.  The workstation
#     binds the dongle with usbipd/usbip and reverse-tunnels the usb-ip port
#     to the dev server.  This container attaches it with `usbip attach`,
#     producing a real HCI device in the host network namespace, which the
#     kernel btusb driver registers and bluetoothd then manages.
#
# Services started (in order):
#   1. shared state directory (ble_adapter_id)
#   2. mdnsd         (mDNSResponder — otbr-agent's mDNS publisher)
#   3. socat         (UNIX socket -> PTY) for the Silabs radio  [remote mode]
#   4. cpcd          (CPC daemon — serial <-> CPC socket)
#   5. usbip attach of the Bluetooth dongle (managed by the dev server's bluetoothd)
#   6. otbr-agent    (Thread Border Router — CPC socket <-> D-Bus API)
#
# Environment variables:
#   SILABS_SOCKET             - path (inside the container) of the bind-mounted
#                               UNIX socket for the Silabs serial tunnel.  When
#                               set, socat bridges it to a local PTY for cpcd.
#   SILABS_DEVICE             - host path of a locally-attached Silabs USB
#                               serial device (alternative to SILABS_SOCKET,
#                               for a radio physically on the dev server).
#   BT_USBIP_SOCKET           - path (inside the container) of the bind-mounted
#                               UNIX socket that the workstation's usbipd is
#                               reverse-tunnelled to.  When set, the Bluetooth
#                               dongle is attached via usb-ip (socat bridges the
#                               socket to a local TCP port).  Optional — Thread
#                               works without it.
#   BT_USBIP_TCP_PORT         - local TCP port socat listens on for usbip
#                               (default: 3240).
#   BT_USBIP_BUSID            - remote busid to attach (default: auto-detect the
#                               first exported device).
#   CPC_INSTANCE              - CPC daemon instance name (default: cpcd_0)
#   BACKBONE_IF               - Thread backbone interface (e.g. eth0)
#   CPCD_CONF                 - path to the cpcd config file
#   DBUS_DIR                  - directory shared with barton (ble_adapter_id)
#   HOST_DBUS_SOCKET          - the dev server's system bus socket, mounted in
#   THREAD_INTERFACE          - this developer's Thread interface (wpan<uid>)
#
# See docs/REMOTE_RADIO_FOR_DEVELOPMENT.md for setup and hardware info.
#

set -e

# Silabs radio: either a bind-mounted UNIX socket (remote tunnel) or a local
# USB serial device.  Neither is defaulted; the validation section below exits
# with a clear message if both are unset.  SILABS_DEVICE is only ever set on
# purpose, so it wins over the socket, which the compose overlay defaults.
SILABS_SOCKET="${SILABS_SOCKET:-}"
SILABS_DEVICE="${SILABS_DEVICE:-}"
if [ -n "${SILABS_DEVICE}" ] && [ -n "${SILABS_SOCKET}" ]; then
    echo "[remote-radios] SILABS_DEVICE is set — using the local radio and ignoring SILABS_SOCKET."
    SILABS_SOCKET=""
fi

# Bluetooth dongle over usb-ip.
BT_USBIP_SOCKET="${BT_USBIP_SOCKET:-}"
BT_USBIP_TCP_PORT="${BT_USBIP_TCP_PORT:-3240}"
BT_USBIP_BUSID="${BT_USBIP_BUSID:-}"

# Claim files let another consumer borrow a radio that cannot be shared.  The
# workstation relay serves exactly one client at a time, and a consumer running
# its own cpcd (an hh4 QEMU guest, for example) needs the raw CPC byte stream
# rather than a cpcd endpoint.  Both sides reconnect automatically, so without
# an explicit claim they displace each other in a loop and corrupt CPC framing
# for whichever one currently holds the link.  While the claim exists this
# container releases the radio; when it disappears the radio is reclaimed.
RADIO_CLAIM_DIR="${RADIO_CLAIM_DIR:-/run/remote-radios/claims}"
SILABS_CLAIM="${SILABS_CLAIM:-${RADIO_CLAIM_DIR}/silabs.claim}"

# Set while this container is running without the Silabs radio, and why:
#   claim       another consumer holds the claim file
#   absent      the workstation is not forwarding it (no tunnel socket yet)
#   unavailable the socket exists but cpcd could not start on it
# The container keeps running regardless; the service monitor connects the
# radio as soon as the reason goes away.  Exiting instead would leave a
# developer without Thread whenever their workstation was offline at the
# moment the container started.
SILABS_YIELDED=""
SILABS_YIELD_REASON=""

CPC_INSTANCE="${CPC_INSTANCE:-cpcd_0}"
CPCD_CONF="${CPCD_CONF:-/usr/local/etc/cpcd.conf}"
DBUS_DIR="${DBUS_DIR:-/var/run/remote-radios-dbus}"

# The dev server's own bluetoothd owns every Bluetooth adapter; this container
# attaches its developer's dongle over usb-ip and then stays out of the way.
#
# This is not a preference, it is forced.  The kernel refuses AF_BLUETOOTH
# outside the initial network namespace, so every bluetoothd on the machine
# necessarily shares one namespace, and bluetoothd has no option restricting it
# to a single adapter — two daemons therefore fight over every dongle.  One
# daemon managing many adapters is exactly what BlueZ is designed for, and each
# developer's Barton selects its own adapter through ble_adapter_id.
HOST_DBUS_SOCKET="${HOST_DBUS_SOCKET:-/run/host-dbus/system_bus_socket}"
BT_DBUS_ADDRESS="unix:path=${HOST_DBUS_SOCKET}"

# This developer's Thread interface.  otbr-agent registers on the shared bus as
# io.openthread.BorderRouter.<THREAD_INTERFACE>, so it must be unique on the dev
# server (dockerw derives it from the developer's uid).
THREAD_INTERFACE="${THREAD_INTERFACE:-wpan0}"
export THREAD_INTERFACE

# Everything in this container that uses D-Bus uses the dev server's bus.
DBUS_SYSTEM_BUS_ADDRESS="${BT_DBUS_ADDRESS}"
export DBUS_SYSTEM_BUS_ADDRESS

HOST_NETNS="/run/host-netns"

# RADIO_DEVICE is the serial device cpcd ultimately opens.  In remote mode it
# is the socat-created PTY; in local mode it is SILABS_DEVICE.
RADIO_DEVICE=""
VIRTUAL_TTY="/dev/ttyRadio"

# Resolve the backbone interface.
# 1. Use BACKBONE_IF from the environment if explicitly set and the interface exists.
# 2. Otherwise auto-detect from the default route.
# Never fall back silently to a wrong interface — fail fast with a clear message.
if [ -n "${BACKBONE_IF}" ] && ip link show "${BACKBONE_IF}" >/dev/null 2>&1; then
    echo "[remote-radios] Using backbone interface from environment: ${BACKBONE_IF}"
else
    if [ -n "${BACKBONE_IF}" ]; then
        echo "[remote-radios] WARNING: BACKBONE_IF '${BACKBONE_IF}' not found; auto-detecting..."
    fi
    BACKBONE_IF=$(ip route show default 2>/dev/null | awk '/default/ {print $5; exit}')
    if [ -z "${BACKBONE_IF}" ]; then
        echo "[remote-radios] ERROR: Could not detect a backbone interface. Set BACKBONE_IF explicitly." >&2
        exit 1
    fi
    echo "[remote-radios] Auto-detected backbone interface: ${BACKBONE_IF}"
fi

# Accept Router Advertisements even with forwarding=1.
# The kernel drops RAs when forwarding is enabled unless accept_ra=2.
# This is an interface-specific sysctl that cannot be set via Docker Compose
# sysctls (Docker requires the driver option syntax for those), so we set it
# here since the container is privileged.
sysctl -qw "net.ipv6.conf.${BACKBONE_IF}.accept_ra=2" 2>/dev/null || true

###############################################################################
# Helper functions
#
# Defined up front so both the initial startup sequence and the background
# service_monitor can reuse them for restart/recovery.
###############################################################################

# is_process_alive — check if a process is alive and not a zombie.
# kill -0 returns true for zombie processes, which would mislead the monitor.
is_process_alive() {
    local pid=$1
    [ -n "${pid}" ] || return 1
    kill -0 "$pid" 2>/dev/null || return 1
    local state
    state=$(awk '/^State:/ {print $2}' /proc/"$pid"/status 2>/dev/null)
    [[ "$state" != "Z" ]]
}

# ── Silabs serial (socat UNIX-socket -> PTY) ────────────────────────────────

# silabs_claimed — true while another consumer has claimed the Silabs radio.
silabs_claimed() {
    [ -e "${SILABS_CLAIM}" ]
}

# yield_silabs — release the radio so the claim holder can open the relay.
# Stops cpcd first, then socat, so cpcd never reads from a half-closed PTY.
yield_silabs() {
    echo "[remote-radios] Silabs radio claimed via ${SILABS_CLAIM} — releasing it."
    SILABS_YIELDED=1
    SILABS_YIELD_REASON=claim
    stop_silabs_link
    echo "[remote-radios] Silabs radio released; waiting for the claim to clear."
}

# stop_silabs_link — stop cpcd, then socat, so cpcd never reads from a
# half-closed PTY.
stop_silabs_link() {
    if [ -n "${CPCD_PID:-}" ]; then
        kill ${CPCD_PID} 2>/dev/null || true
        wait ${CPCD_PID} 2>/dev/null || true
        CPCD_PID=""
    fi
    if [ -n "${SOCAT_PID:-}" ]; then
        kill ${SOCAT_PID} 2>/dev/null || true
        wait ${SOCAT_PID} 2>/dev/null || true
        SOCAT_PID=""
    fi
    rm -f "${VIRTUAL_TTY}"
}

# resume_silabs — connect the radio once nothing prevents it.  Non-zero on
# failure so the monitor can back off and retry.
resume_silabs() {
    case "${SILABS_YIELD_REASON}" in
        claim)  echo "[remote-radios] Silabs claim cleared — reconnecting." ;;
        absent) echo "[remote-radios] Silabs tunnel socket appeared — connecting." ;;
        *)      echo "[remote-radios] Retrying the Silabs radio..." ;;
    esac
    start_silabs_socat || return 1
    restart_cpcd || return 1
    SILABS_YIELDED=""
    SILABS_YIELD_REASON=""
    echo "[remote-radios] Silabs radio connected."
    return 0
}

# start_silabs_socat — bridge the bind-mounted UNIX socket to a local PTY that
# cpcd opens as /dev/ttyRadio.  Sets SOCAT_PID.  Returns non-zero on failure.
#
# NOTE: do NOT use wait-slave — it ties socat's UNIX lifecycle to the PTY slave
# state.  If cpcd briefly closes/reopens the device during CPC init retries,
# wait-slave causes socat to drop the relay connection.
start_silabs_socat() {
    [ -n "${SOCAT_PID:-}" ] && kill ${SOCAT_PID} 2>/dev/null || true
    [ -n "${SOCAT_PID:-}" ] && wait ${SOCAT_PID} 2>/dev/null || true
    rm -f "${VIRTUAL_TTY}"

    # Wait for the socket to exist (it may vanish briefly on tunnel restarts).
    local waitCount=0
    while [ ! -S "${SILABS_SOCKET}" ]; do
        if [ ${waitCount} -ge 30 ]; then
            echo "[remote-radios] Silabs tunnel socket ${SILABS_SOCKET} not present after 30s." >&2
            return 1
        fi
        sleep 2
        waitCount=$((waitCount + 2))
    done

    #   - UNIX-CONNECT: connect to the bind-mounted stream socket
    #   - forever,intervall=3: reconnect on drops (self-healing)
    #   - PTY,raw,echo=0,b115200: expose a clean serial device for cpcd
    socat \
        "UNIX-CONNECT:${SILABS_SOCKET},forever,intervall=3" \
        "PTY,link=${VIRTUAL_TTY},raw,echo=0,b115200" &
    SOCAT_PID=$!

    local ptyWait=0
    while [ ! -L "${VIRTUAL_TTY}" ]; do
        if ! kill -0 ${SOCAT_PID} 2>/dev/null; then
            echo "[remote-radios] socat exited before creating ${VIRTUAL_TTY}." >&2
            return 1
        fi
        if [ ${ptyWait} -ge 10 ]; then
            echo "[remote-radios] socat did not create ${VIRTUAL_TTY} after 10s." >&2
            return 1
        fi
        sleep 1
        ptyWait=$((ptyWait + 1))
    done

    echo "[remote-radios] socat bridge ready (PID ${SOCAT_PID}, ${VIRTUAL_TTY})."
    return 0
}

# ── cpcd ────────────────────────────────────────────────────────────────────

CPCD_LOG="/tmp/cpcd.log"
CPCD_WAIT_MAX=30

write_cpcd_conf() {
    echo "[remote-radios] Writing cpcd config to ${CPCD_CONF}..."
    mkdir -p "$(dirname "${CPCD_CONF}")"
    cat > "${CPCD_CONF}" <<EOF
# cpcd configuration — written at container startup by remote-radios-entrypoint.sh
instance_name: ${CPC_INSTANCE}
bus_type: UART
uart_device_file: ${RADIO_DEVICE}
uart_device_baud: 115200
uart_hardflow: true
EOF
}

# start_cpcd — start cpcd and wait for its ready message.  Sets CPCD_PID.
# Exits the container on first-start failure (fatal); returns non-zero when
# called for a restart so the monitor can retry.
start_cpcd() {
    local fatal="${1:-fatal}"
    : > "${CPCD_LOG}"
    # stdbuf -oL forces line-buffered stdout so grep can detect the ready
    # message promptly through the pipe.
    stdbuf -oL cpcd --conf "${CPCD_CONF}" > >(tee "${CPCD_LOG}") 2>&1 &
    CPCD_PID=$!

    local waitCount=0
    echo "[remote-radios] Waiting for cpcd to be ready..."
    while ! grep -q "Daemon startup was successful" "${CPCD_LOG}" 2>/dev/null; do
        if ! kill -0 ${CPCD_PID} 2>/dev/null; then
            echo "[remote-radios] ERROR: cpcd exited before becoming ready. Check device and firmware." >&2
            cat "${CPCD_LOG}" >&2
            [ "${fatal}" = "fatal" ] && exit 1
            return 1
        fi
        if [ ${waitCount} -ge ${CPCD_WAIT_MAX} ]; then
            echo "[remote-radios] ERROR: cpcd did not become ready after ${CPCD_WAIT_MAX}s." >&2
            cat "${CPCD_LOG}" >&2
            [ "${fatal}" = "fatal" ] && exit 1
            return 1
        fi
        sleep 1
        waitCount=$((waitCount + 1))
    done

    echo "[remote-radios] cpcd ready (PID ${CPCD_PID}, waited ${waitCount}s)."
    return 0
}

# restart_cpcd — kill the old cpcd, rewrite config, start a fresh instance.
restart_cpcd() {
    echo "[remote-radios] Restarting cpcd..."
    [ -n "${CPCD_PID:-}" ] && kill ${CPCD_PID} 2>/dev/null || true
    [ -n "${CPCD_PID:-}" ] && wait ${CPCD_PID} 2>/dev/null || true
    sleep 1
    write_cpcd_conf
    start_cpcd nonfatal
}

# ── Bluetooth dongle (usb-ip -> btusb -> bluetoothd) ─────────────────────
#
# The dedicated Bluetooth USB dongle is forwarded from the developer's
# workstation via usb-ip.  Attaching it here creates a REAL HCI device in the
# host network namespace (AF_BLUETOOTH sockets only work in the initial netns),
# which the kernel btusb driver registers and the dev server's shared
# bluetoothd then manages.  The workstation's own Bluetooth adapter is never
# touched.

# bt_usbip_attach — attach the dongle over usb-ip in the host netns.
# Auto-detects the busid if BT_USBIP_BUSID is unset.  Returns non-zero on
# failure.
bt_usbip_attach() {
    # vhci-hcd provides the virtual USB host controller for usb-ip clients.
    # btusb drives the imported Bluetooth dongle so the kernel creates an hci
    # device.  Both require the host's /lib/modules to be mounted (compose does
    # this) and are loaded into the host kernel from this privileged container.
    if ! lsmod 2>/dev/null | grep -qw vhci_hcd; then
        modprobe vhci-hcd 2>/dev/null || true
    fi
    if ! lsmod 2>/dev/null | grep -qw vhci_hcd; then
        echo "[remote-radios] ERROR: vhci-hcd kernel module is not loaded and could not be" >&2
        echo "[remote-radios]        loaded.  Ensure /lib/modules is mounted into this" >&2
        echo "[remote-radios]        container (compose overlay does this) and that the host" >&2
        echo "[remote-radios]        provides the usb-ip vhci-hcd module." >&2
        return 1
    fi
    # btusb is needed for the kernel to expose the imported dongle as an HCI
    # device.  Best-effort: some kernels build it in.
    if ! lsmod 2>/dev/null | grep -qw btusb; then
        modprobe btusb 2>/dev/null || true
    fi

    # A record left by an earlier, unclean run cannot prove the port is still
    # ours: the vhci controller is shared, and another developer may since have
    # attached a device with the same id on that port.  Drop it without
    # detaching; our old usb-ip connection died with that run, which frees its
    # port.
    rm -f "${BT_USBIP_PORT_FILE}"

    # The workstation's usbipd is reverse-tunnelled to a bind-mounted UNIX
    # socket (BT_USBIP_SOCKET).  usbip speaks TCP, so bridge the socket to a
    # local TCP port with socat, then attach against 127.0.0.1:<port>.  Using a
    # UNIX socket avoids any sshd GatewayPorts change on the dev server.
    if [ ! -S "${BT_USBIP_SOCKET}" ]; then
        # Wait for the workstation tunnel to create the socket.
        local sockWait=0
        while [ ! -S "${BT_USBIP_SOCKET}" ]; do
            if [ ${sockWait} -ge 30 ]; then
                echo "[remote-radios] BT usb-ip socket ${BT_USBIP_SOCKET} did not appear after 30s." >&2
                echo "[remote-radios]        Ensure remote-radios-setup.sh is running on your workstation." >&2
                return 1
            fi
            sleep 3
            sockWait=$((sockWait + 3))
        done
    fi

    # (Re)start the socat UNIX->TCP bridge for usbip.
    [ -n "${BT_SOCAT_PID:-}" ] && kill "${BT_SOCAT_PID}" 2>/dev/null || true
    socat "TCP4-LISTEN:${BT_USBIP_TCP_PORT},bind=127.0.0.1,reuseaddr,fork" \
          "UNIX-CONNECT:${BT_USBIP_SOCKET}" &
    BT_SOCAT_PID=$!

    # Wait for the local usbip TCP endpoint to accept connections.
    local waitCount=0
    while ! timeout 2 bash -c ": >/dev/tcp/127.0.0.1/${BT_USBIP_TCP_PORT}" 2>/dev/null; do
        if ! kill -0 "${BT_SOCAT_PID}" 2>/dev/null; then
            echo "[remote-radios] usb-ip socat bridge exited unexpectedly." >&2
            return 1
        fi
        if [ ${waitCount} -ge 20 ]; then
            echo "[remote-radios] local usb-ip bridge 127.0.0.1:${BT_USBIP_TCP_PORT} not ready after 20s." >&2
            return 1
        fi
        sleep 2
        waitCount=$((waitCount + 2))
    done

    local busid="${BT_USBIP_BUSID}"
    if [ -z "${busid}" ]; then
        # Auto-detect: take the first exported device.  The TCP port is a global
        # usbip option (--tcp-port), not a subcommand flag.
        busid=$(usbip --tcp-port "${BT_USBIP_TCP_PORT}" list -r 127.0.0.1 2>/dev/null \
            | awk -F: '/^[[:space:]]*[0-9]+-[0-9.]+:/ {gsub(/^[[:space:]]+/,"",$1); print $1; exit}')
    fi

    if [ -z "${busid}" ]; then
        echo "[remote-radios] Could not determine usb-ip busid for the Bluetooth dongle." >&2
        echo "[remote-radios] usbip list output was:" >&2
        usbip --tcp-port "${BT_USBIP_TCP_PORT}" list -r 127.0.0.1 2>&1 | sed 's/^/[remote-radios]   /' >&2
        return 1
    fi

    echo "[remote-radios] Attaching Bluetooth dongle via usb-ip (busid ${busid})..."
    local attach_out attach_rc
    # usbip records each port it attaches in this container's own
    # /var/run/vhci_hcd, which identifies our port without looking at other
    # developers' attaches on the shared controller.
    local attachMarker
    attachMarker=$(mktemp)
    attach_out=$(usbip --tcp-port "${BT_USBIP_TCP_PORT}" attach -r 127.0.0.1 -b "${busid}" 2>&1)
    attach_rc=$?
    if [ ${attach_rc} -ne 0 ]; then
        echo "[remote-radios] usbip attach failed for busid ${busid}: ${attach_out}" >&2
        rm -f "${attachMarker}"
        return 1
    fi
    bt_usbip_record_port "${attachMarker}" "${busid}"
    rm -f "${attachMarker}"

    # Record which busid we attached (informational; teardown uses BT_USBIP_PORT_FILE).
    BT_USBIP_ATTACHED_BUSID="${busid}"
    export BT_USBIP_ATTACHED_BUSID
    return 0
}

# The vhci-hcd controller is shared by every container on the host, so
# `usbip port` lists other developers' dongles too.  The port this container
# attached, and the remote device id it carried, are recorded in this user's
# own state directory; only a port that still matches that record is detached.
BT_USBIP_PORT_FILE="${DBUS_DIR}/usbip_port"

# vhci_ports_in_state <sta> — print "<port> <devid>" for vhci ports in a state
# (006 == VDEV_ST_USED, 007 == VDEV_ST_ERROR; enum usbip_device_status in
# drivers/usb/usbip/usbip_common.h, printed by vhci_sysfs.c).
vhci_ports_in_state() {
    local f
    for f in /sys/devices/platform/vhci_hcd.*/status*; do
        [ -r "${f}" ] || continue
        # Columns: hub port sta spd dev sockfd local_busid
        awk -v sta="$1" '$3 == sta { print ($2 + 0), $5 }' "${f}"
    done | sort -u
}

# A port reads 005 (VDEV_ST_NOTASSIGNED, devid not shown) right after attach and
# 006 (VDEV_ST_USED) once the device has enumerated, so callers wait for 006.
vhci_used_ports() {
    vhci_ports_in_state 006
}

# vhci_local_busid <port> — print the local USB busid (e.g. 7-1) of a vhci port.
vhci_local_busid() {
    local f
    for f in /sys/devices/platform/vhci_hcd.*/status*; do
        [ -r "${f}" ] || continue
        awk -v port="$1" '$3 == "006" && ($2 + 0) == port { print $7 }' "${f}"
    done | head -1
}

# bt_usbip_record_port <marker> <busid> — record the port our attach created.
# usbip writes /var/run/vhci_hcd/port<N> ("<host> <tcp-port> <busid>") in this
# container for each port it attaches; only one written after <marker> for our
# busid counts.
bt_usbip_record_port() {
    local marker="$1" busid="$2" f port="" devid="" tries=0
    for f in $(find /var/run/vhci_hcd -maxdepth 1 -name 'port*' -newer "${marker}" 2>/dev/null); do
        if [ "$(awk '{print $3}' "${f}")" = "${busid}" ]; then
            [ -z "${port}" ] || { port=""; break; }   # ambiguous; refuse to guess
            port="${f##*/port}"
        fi
    done
    rm -f "${BT_USBIP_PORT_FILE}"
    if [ -z "${port}" ]; then
        echo "[remote-radios] WARNING: could not identify the vhci port for this attach;" >&2
        echo "[remote-radios]          it will not be detached automatically." >&2
        return 0
    fi
    # The port reads 005 until the device enumerates, then 006 with its devid.
    while [ ${tries} -lt 20 ]; do
        devid=$(vhci_used_ports | awk -v port="${port}" '$1 == port { print $2 }')
        [ -n "${devid}" ] && break
        tries=$((tries + 1))
        sleep 0.5
    done
    if [ -z "${devid}" ]; then
        echo "[remote-radios] WARNING: vhci port ${port} did not finish enumerating;" >&2
        echo "[remote-radios]          it will not be detached automatically." >&2
        return 0
    fi
    printf '%s %s\n' "${port}" "${devid}" > "${BT_USBIP_PORT_FILE}"
    echo "[remote-radios] Bluetooth dongle attached on vhci port ${port}"
}

# bt_usbip_detach — detach the usb-ip port this container attached during this
# run (teardown only; a record from an earlier run is discarded at startup).
# The port must still carry our device, or be in the error state, where its
# devid is no longer reported.
bt_usbip_detach() {
    [ -r "${BT_USBIP_PORT_FILE}" ] || return 0
    local port devid
    read -r port devid < "${BT_USBIP_PORT_FILE}" || true
    if [ -n "${port}" ] && { vhci_used_ports | grep -qx "${port} ${devid}" \
        || vhci_ports_in_state 007 | grep -q "^${port} "; }; then
        echo "[remote-radios] Detaching Bluetooth dongle from vhci port ${port}"
        usbip detach -p "${port}" 2>/dev/null || true
    fi
    rm -f "${BT_USBIP_PORT_FILE}"
}

# start_bluetooth_chain — attach the dongle, identify its HCI device, and start
# bluetoothd.  Sets BT_HCI_INDEX and starts bluetoothd in the host netns.
# Returns non-zero on failure.
#
# Unlike the old CPC path, the usb-ip dongle registers its own HCI device
# directly (kernel btusb driver), so no bt_host_cpc_hci_bridge / btattach /
# PTY-proxy is needed.  We only need to identify the new adapter index and
# wait for the dev server's bluetoothd to register it.
start_bluetooth_chain() {
    echo "[remote-radios] Starting Bluetooth chain (usb-ip attach -> bluetoothd)..."

    if ! bt_usbip_attach; then
        echo "[remote-radios] Bluetooth usb-ip attach failed." >&2
        return 1
    fi

    # Find the HCI device under the USB device behind our own vhci port, never
    # by diffing the adapter list, which other developers' dongles also join.
    local port="" localBusid="" waited=0 radioHci="" hciPath
    [ -r "${BT_USBIP_PORT_FILE}" ] && read -r port _ < "${BT_USBIP_PORT_FILE}"
    if [ -z "${port}" ]; then
        echo "[remote-radios] WARNING: the attached vhci port is unknown, so its HCI device cannot be identified." >&2
        return 1
    fi
    # btusb probing an imported device over the tunnel can take a few seconds.
    while [ ${waited} -lt 15 ]; do
        localBusid=$(vhci_local_busid "${port}")
        if [ -n "${localBusid}" ]; then
            for hciPath in /sys/bus/usb/devices/"${localBusid}"/"${localBusid}":*/bluetooth/hci*; do
                [ -e "${hciPath}" ] && radioHci="${hciPath##*/}" && break
            done
        fi
        [ -n "${radioHci}" ] && break
        sleep 1
        waited=$((waited + 1))
    done

    if [ -z "${radioHci}" ]; then
        echo "[remote-radios] WARNING: no HCI device appeared on vhci port ${port} within 15s." >&2
        return 1
    fi

    BT_HCI_INDEX="${radioHci#hci}"
    echo "[remote-radios] Bluetooth dongle HCI device: ${radioHci} (index ${BT_HCI_INDEX})"
    echo "${BT_HCI_INDEX}" > "${DBUS_DIR}/ble_adapter_id"
    echo "[remote-radios] Wrote BLE adapter index ${BT_HCI_INDEX} to ${DBUS_DIR}/ble_adapter_id"

    # Note any other adapters (e.g. a host built-in) without disturbing them —
    # ble_adapter_id steers Matter to the dongle.
    for hostHci in $(nsenter --net="${HOST_NETNS}" ls /sys/class/bluetooth/ 2>/dev/null); do
        if [ "${hostHci}" != "${radioHci}" ]; then
            echo "[remote-radios] NOTE: other adapter ${hostHci} also present (left untouched)."
        fi
    done

    # Power the dongle up.  The dev server's bluetoothd picks it up from here.
    nsenter --net="${HOST_NETNS}" hciconfig "${radioHci}" up 2>/dev/null || true
    echo "[remote-radios] Handing ${radioHci} to the dev server's bluetoothd."
    sleep 3

    # The adapter existing in sysfs is NOT proof that BLE works.  bluetoothd
    # must also have claimed it; until then every BLE operation fails even
    # though hciconfig reports the controller UP RUNNING.
    local waited=0
    while [ ${waited} -lt 10 ]; do
        bt_controller_registered && break
        sleep 1
        waited=$((waited + 1))
    done
    if ! bt_controller_registered; then
        echo "[remote-radios] WARNING: the dev server's bluetoothd does not expose ${radioHci}." >&2
        echo "[remote-radios]          BLE will not work.  On the dev server run:" >&2
        echo "[remote-radios]              sudo systemctl unmask bluetooth.service" >&2
        echo "[remote-radios]              sudo systemctl enable --now bluetooth.service" >&2
        echo "[remote-radios]          and check that ${HOST_DBUS_SOCKET} is mounted here." >&2
        return 1
    fi

    echo "[remote-radios] Bluetooth chain ready (${radioHci})."
    return 0
}

# teardown_bluetooth_chain — stop bluetoothd and detach the dongle.
teardown_bluetooth_chain() {
    # bluetoothd belongs to the dev server and is shared with other developers,
    # so it is never stopped from here.
    if [ -n "${BT_HCI_INDEX:-}" ]; then
        nsenter --net="${HOST_NETNS}" hciconfig "hci${BT_HCI_INDEX}" down 2>/dev/null || true
    fi
    bt_usbip_detach
    # Stop the usbip socat bridge so a restart re-establishes it cleanly.
    [ -n "${BT_SOCAT_PID:-}" ] && kill "${BT_SOCAT_PID}" 2>/dev/null || true
    sleep 1
}

# bt_controller_registered — verify bluetoothd actually exposes the adapter.
#
# Every other Bluetooth check in this file is kernel-level, and the kernel is
# perfectly happy to report an adapter as UP RUNNING while no bluetoothd owns
# it.  In that state BLE is completely dead, so this check is what separates
# "the dongle arrived" from "Bluetooth works".  Asking for the Adapter1
# Address property fails unless bluetoothd has registered this exact adapter.
bt_controller_registered() {
    local idx="${BT_HCI_INDEX:-}"
    [ -n "$idx" ] || return 1
    DBUS_SYSTEM_BUS_ADDRESS="${BT_DBUS_ADDRESS}" \
        timeout 5 dbus-send --system --dest=org.bluez --print-reply \
            "/org/bluez/hci${idx}" org.freedesktop.DBus.Properties.Get \
            string:org.bluez.Adapter1 string:Address >/dev/null 2>&1
}

# bt_transport_healthy — verify the dongle's HCI transport is responsive.
bt_transport_healthy() {
    local idx="${BT_HCI_INDEX:-}"
    [ -n "$idx" ] || return 1
    # The dongle must still be present in sysfs (usb-ip session alive).
    nsenter --net="${HOST_NETNS}" test -e "/sys/class/bluetooth/hci${idx}" 2>/dev/null || return 1
    timeout 5 nsenter --net="${HOST_NETNS}" hciconfig "hci${idx}" version >/dev/null 2>&1 || return 1
    # ...and bluetoothd must still own it.  A daemon crash, or another bluetoothd
    # stealing the adapter, leaves the kernel checks above passing while BLE is
    # unusable — the monitor must treat that as unhealthy and rebuild the chain.
    bt_controller_registered || return 1
    return 0
}

# ── Unified service monitor ─────────────────────────────────────────────────
#
# Dependency chain:
#
#   socat (remote only) -> cpcd -> otbr-agent (separate foreground loop)
#   usb-ip dongle       -> bluetoothd            (independent of cpcd)
#
# The Bluetooth dongle is now a fully independent radio, so its failures no
# longer cascade from cpcd.  Each iteration checks the layers and restarts the
# smallest broken scope.
service_monitor() {
    local backoff=5
    local btHealthCounter=0
    local btFailCount=0
    local graceUntil=$((SECONDS + 30))

    while true; do
        sleep "${backoff}"

        local restartSilabs="" restartBt=""

        # Silabs claim arbitration.  Release the radio while another consumer
        # holds it, and take it back as soon as the claim clears.  See the
        # RADIO_CLAIM_DIR comment near the top of this file for why this is not
        # left to a race.
        if [ -n "${SILABS_SOCKET}" ]; then
            if silabs_claimed; then
                if [ -z "${SILABS_YIELDED}" ]; then
                    yield_silabs
                else
                    SILABS_YIELD_REASON=claim
                fi
            elif [ -n "${SILABS_YIELDED}" ]; then
                if [ ! -S "${SILABS_SOCKET}" ]; then
                    # Still not forwarded.  Nothing to try until it appears.
                    SILABS_YIELD_REASON=absent
                elif resume_silabs; then
                    backoff=5
                else
                    SILABS_YIELD_REASON=unavailable
                    backoff=$((backoff * 2 < 60 ? backoff * 2 : 60))
                    echo "[monitor] Could not connect the Silabs radio; retrying in ${backoff}s..." >&2
                fi
            fi
        fi

        # Silabs health checks are meaningless while the radio is released.
        if [ -z "${SILABS_YIELDED}" ]; then
            # Silabs layer 0: socat (remote mode only).
            if [ -n "${SILABS_SOCKET}" ] && [ -n "${SOCAT_PID:-}" ] && ! is_process_alive ${SOCAT_PID}; then
                echo "[monitor] socat (PID ${SOCAT_PID}) died." >&2
                restartSilabs="socat"
            fi

            # Silabs layer 1: cpcd.
            if [ -z "${restartSilabs}" ] && ! is_process_alive ${CPCD_PID}; then
                echo "[monitor] cpcd (PID ${CPCD_PID}) died." >&2
                restartSilabs="cpcd"
            fi
        fi

        # Bluetooth: only if configured.  Deliberately NOT gated on
        # BT_HCI_INDEX — that is set only by a *successful* attach, so gating on
        # it would permanently disable recovery whenever the initial attach
        # failed (exactly the case the startup message promises to retry).
        # bt_transport_healthy already reports unhealthy while the index is
        # unset, so an unattached dongle simply looks broken and gets retried.
        if [ -n "${BT_USBIP_SOCKET}" ] && [ -e "${HOST_NETNS}" ] && [ ${SECONDS} -ge ${graceUntil} ]; then
            btHealthCounter=$((btHealthCounter + 1))
            if [ ${btHealthCounter} -ge 3 ]; then
                btHealthCounter=0
                if ! bt_transport_healthy; then
                    btFailCount=$((btFailCount + 1))
                    if [ ${btFailCount} -ge 2 ]; then
                        echo "[monitor] Bluetooth transport unresponsive (${btFailCount} failures)." >&2
                        restartBt="yes"
                        btFailCount=0
                    fi
                else
                    btFailCount=0
                fi
            fi
        fi

        # ── Execute Silabs restart ──────────────────────────────────────────
        if [ -n "${restartSilabs}" ]; then
            case "${restartSilabs}" in
                socat)
                    kill ${CPCD_PID} 2>/dev/null || true
                    wait ${CPCD_PID} 2>/dev/null || true
                    if ! start_silabs_socat; then
                        backoff=$((backoff * 2 < 60 ? backoff * 2 : 60))
                        echo "[monitor] socat restart failed; retrying in ${backoff}s..." >&2
                        continue
                    fi
                    if ! restart_cpcd; then
                        backoff=$((backoff * 2 < 60 ? backoff * 2 : 60))
                        echo "[monitor] cpcd restart failed; retrying in ${backoff}s..." >&2
                        continue
                    fi
                    backoff=5
                    echo "[monitor] Silabs stack recovered (socat -> cpcd)."
                    ;;
                cpcd)
                    if ! restart_cpcd; then
                        backoff=$((backoff * 2 < 60 ? backoff * 2 : 60))
                        echo "[monitor] cpcd restart failed; retrying in ${backoff}s..." >&2
                        continue
                    fi
                    backoff=5
                    echo "[monitor] Silabs stack recovered (cpcd)."
                    ;;
            esac
        fi

        # ── Execute Bluetooth restart ───────────────────────────────────────
        if [ -n "${restartBt}" ]; then
            teardown_bluetooth_chain
            if ! start_bluetooth_chain; then
                backoff=$((backoff * 2 < 60 ? backoff * 2 : 60))
                echo "[monitor] Bluetooth chain restart failed; retrying in ${backoff}s..." >&2
                continue
            fi
            backoff=5
            graceUntil=$((SECONDS + 30))
            echo "[monitor] Bluetooth chain recovered."
        fi
    done
}

###############################################################################
# 1. Prepare the shared state directory
#
# The ble/ part of the per-user <user>-remote-radios-state volume is mounted at
# ${DBUS_DIR} and shared with each of this user's barton containers.  It
# carries ble_adapter_id.  (Its cpcd/ part holds cpcd's sockets.)
# No D-Bus daemon runs here: otbr-agent and org.bluez both use the dev server's
# own system bus (see HOST_DBUS_SOCKET).
###############################################################################
mkdir -p "${DBUS_DIR}"
rm -f "${DBUS_DIR}/system_bus_socket"

###############################################################################
# 2. Establish the Silabs serial link and start CPC
#
# Two modes are supported:
#   a) Remote tunnel (default for developers): SILABS_SOCKET points at the
#      bind-mounted UNIX socket that remote-serial.py serves via `ssh -R`.
#      socat bridges it to a local PTY (/dev/ttyRadio) that cpcd opens.
#   b) Local radio: SILABS_DEVICE points at a USB serial device physically
#      attached to the dev server.  cpcd opens it directly.
###############################################################################

if [ -n "${SILABS_SOCKET}" ]; then
    #--------------------------------------------------------------------------
    # Remote tunnel mode — bridge the UNIX socket to a PTY via socat.
    #--------------------------------------------------------------------------
    echo "[remote-radios] Silabs remote mode: bridging UNIX socket ${SILABS_SOCKET}"

    # Give a workstation that is just connecting a moment to create the
    # socket, but never wait on it indefinitely here: everything else (the
    # Bluetooth dongle, the service monitor) would be held up too.
    SILABS_WAIT_MAX=20
    silabsWaitCount=0
    echo "[remote-radios] Waiting for Silabs tunnel socket ${SILABS_SOCKET}..."
    while [ ! -S "${SILABS_SOCKET}" ] && [ ${silabsWaitCount} -lt ${SILABS_WAIT_MAX} ]; do
        sleep 2
        silabsWaitCount=$((silabsWaitCount + 2))
    done

    RADIO_DEVICE="${VIRTUAL_TTY}"
    if silabs_claimed; then
        # Start released rather than stealing the link from the claim holder.
        # The service monitor connects as soon as the claim clears.
        SILABS_YIELDED=1
        SILABS_YIELD_REASON=claim
        echo "[remote-radios] Silabs radio is claimed via ${SILABS_CLAIM} — starting without it."
    elif [ ! -S "${SILABS_SOCKET}" ]; then
        SILABS_YIELDED=1
        SILABS_YIELD_REASON=absent
        echo "[remote-radios] Silabs tunnel socket ${SILABS_SOCKET} is not present yet — starting without it." >&2
        echo "[remote-radios]   It connects automatically once your workstation forwards the radio" >&2
        echo "[remote-radios]   (remote-radios.service; see docs/REMOTE_RADIO_FOR_DEVELOPMENT.md)." >&2
    else
        echo "[remote-radios] Silabs tunnel socket is present.  Starting socat bridge..."
        if start_silabs_socat; then
            echo "[remote-radios] Virtual serial device ready: ${RADIO_DEVICE} (via ${SILABS_SOCKET})"
        else
            SILABS_YIELDED=1
            SILABS_YIELD_REASON=unavailable
            echo "[remote-radios] Could not bridge the Silabs socket — the monitor will retry." >&2
        fi
    fi

elif [ -n "${SILABS_DEVICE}" ]; then
    #--------------------------------------------------------------------------
    # Local radio mode — validate SILABS_DEVICE directly.
    #--------------------------------------------------------------------------
    if [ ! -e "${SILABS_DEVICE}" ]; then
        echo "[remote-radios] ERROR: Silabs USB radio device '${SILABS_DEVICE}' not found." >&2
        echo "[remote-radios]        Check that the radio is connected." >&2
        echo "[remote-radios]        See docs/REMOTE_RADIO_FOR_DEVELOPMENT.md for setup instructions." >&2
        exit 1
    fi

    RADIO_DEVICE="${SILABS_DEVICE}"
    echo "[remote-radios] Using local Silabs USB radio device: ${RADIO_DEVICE}"

else
    echo "[remote-radios] ERROR: No Silabs radio configured." >&2
    echo "[remote-radios]        Set SILABS_SOCKET for a remote serial tunnel (the usual case):" >&2
    echo "[remote-radios]            export SILABS_SOCKET=/run/remote-radios/radios/silabs.sock" >&2
    echo "[remote-radios]        Or set SILABS_DEVICE for a locally attached radio:" >&2
    echo "[remote-radios]            export SILABS_DEVICE=/dev/ttyACM0" >&2
    exit 1
fi

###############################################################################
# 3. Start mdnsd (mDNSResponder — otbr-agent's mDNS publisher)
#
# mDNSResponder is used rather than avahi because it is reached over a local
# socket, not D-Bus: otbr-agent's own D-Bus API lives on a bus shared by every
# developer on the dev server, where only one org.freedesktop.Avahi could exist.
# mdnsd runs in this container's network namespace, so each developer's
# instance answers only for their own border router.
###############################################################################
echo "[remote-radios] Starting mdnsd..."
rm -f /var/run/mdnsd
/usr/sbin/mdnsd
for _ in $(seq 1 50); do
    [ -S /var/run/mdnsd ] && break
    sleep 0.1
done
if [ -S /var/run/mdnsd ]; then
    echo "[remote-radios] mdnsd started."
else
    echo "[remote-radios] WARNING: mdnsd socket /var/run/mdnsd did not appear." >&2
fi

###############################################################################
# 4. Configure and start cpcd
#
# uart_hardflow must be 'true' because the radio's firmware has hardware flow
# control enabled; cpcd checks the radio's capability flags during init and
# exits FATAL on a mismatch.  On the socat PTY, CRTSCTS is a no-op (no physical
# RTS/CTS lines) — the real flow control happens at the workstation relay.
###############################################################################
write_cpcd_conf

if [ -n "${SILABS_YIELDED}" ]; then
    echo "[remote-radios] Deferring cpcd until the Silabs radio is available (${SILABS_YIELD_REASON})."
else
    echo "[remote-radios] Starting cpcd (instance: ${CPC_INSTANCE}, device: ${RADIO_DEVICE})..."
    if [ -n "${SILABS_SOCKET}" ]; then
        # A forwarded radio can be slow or briefly unreachable; keep running
        # and let the monitor retry rather than exiting.
        if ! start_cpcd nonfatal; then
            stop_silabs_link
            SILABS_YIELDED=1
            SILABS_YIELD_REASON=unavailable
            echo "[remote-radios] cpcd did not start — the monitor will retry." >&2
        fi
    else
        start_cpcd
    fi
fi

###############################################################################
# 5. Bring up the Bluetooth dongle over usb-ip (optional)
###############################################################################
if [ -n "${BT_USBIP_SOCKET}" ]; then

    if [ ! -e "${HOST_NETNS}" ]; then
        echo "[remote-radios] WARNING: Host network namespace not mounted at ${HOST_NETNS}." >&2
        echo "[remote-radios]          Bluetooth support will not be available." >&2
        echo "[remote-radios]          Add '- /proc/1/ns/net:/run/host-netns:ro' to volumes." >&2
    else
        if start_bluetooth_chain; then
            echo "[remote-radios] Bluetooth stack initialized."
        else
            echo "[remote-radios] WARNING: Initial Bluetooth attach failed; monitor will retry." >&2
        fi
    fi
else
    echo "[remote-radios] No Bluetooth dongle configured (BT_USBIP_SOCKET unset) — Thread/Zigbee only."
fi

# Start the unified service monitor for the whole radio stack.
service_monitor &
SERVICE_MONITOR_PID=$!
echo "[remote-radios] Service monitor started (PID ${SERVICE_MONITOR_PID})."

###############################################################################
# 6. Start otbr-agent with auto-restart
#
# otbr-agent runs in the foreground inside a restart loop so the container
# survives transient CPC failures.  Backs off exponentially up to 30s.
###############################################################################
echo "[remote-radios] Starting otbr-agent (backbone: ${BACKBONE_IF})..."

otbr_backoff=2

while true; do
    # Wait for cpcd to be alive before starting otbr-agent.  This can last as
    # long as the radio is not forwarded, so say so once rather than every
    # few seconds.
    if ! pgrep -x cpcd >/dev/null 2>&1; then
        echo "[remote-radios] Waiting for cpcd before starting otbr-agent..." >&2
        while ! pgrep -x cpcd >/dev/null 2>&1; do
            sleep 5
        done
    fi

    otbr_start=$(date +%s)
    # otbr-agent registers on the dev server's shared bus, next to org.bluez,
    # because Matter needs the border router and BlueZ on one bus.  The unique
    # interface name keeps each developer's registration distinct there.
    #
    # The radio is multi-PAN: Thread and Zigbee share it through this cpcd,
    # each host using its own spinel interface ID.  Silicon Labs' convention
    # is iid=1 for zigbeed and iid=2 for otbr-agent; iid 0 is broadcast.
    DBUS_SYSTEM_BUS_ADDRESS="${BT_DBUS_ADDRESS}" \
    otbr-agent \
        -I "${THREAD_INTERFACE}" \
        -B "${BACKBONE_IF}" \
        -d 7 \
        -v \
        "spinel+cpc://${CPC_INSTANCE}?iid=2&iid-list=0" &
    OTBR_PID=$!
    set +e
    wait ${OTBR_PID} 2>/dev/null
    otbr_exit=$?
    set -e

    otbr_elapsed=$(( $(date +%s) - otbr_start ))

    if [ "${otbr_elapsed}" -ge 60 ]; then
        otbr_backoff=2
    fi

    echo "[remote-radios] otbr-agent exited (code ${otbr_exit}) after ${otbr_elapsed}s; restarting in ${otbr_backoff}s..." >&2
    sleep "${otbr_backoff}"
    otbr_backoff=$((otbr_backoff * 2 < 30 ? otbr_backoff * 2 : 30))
done
