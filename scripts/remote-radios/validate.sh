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
# Remote Radios Validation Script
#
# Performs a detailed check of all requirements for the two independent
# physical radios forwarded from a developer's workstation into the
# remote-radios container:
#
#   * Silicon Labs Zigbee/Thread radio — reached over a serial-over-SSH
#     tunnel to a per-user UNIX socket that is bind-mounted into the
#     container (SILABS_SOCKET).  socat bridges that socket to a local PTY
#     (/dev/ttyRadio) that cpcd opens as if it were a directly-attached USB
#     serial device.  Alternatively, a locally attached Silabs USB serial
#     device (SILABS_DEVICE) is opened by cpcd directly.
#
#   * Dedicated Bluetooth USB dongle — reached over usb-ip (BT_USBIP_SOCKET,
#     a bind-mounted socket).  The container attaches it with `usbip attach`,
#     producing a REAL HCI device in the host network namespace, which
#     btattach/bluetoothd then manage.  This radio is optional — Thread /
#     Zigbee works without it.
#
# Run this inside the Barton or remote-radios container to diagnose the
# Silabs serial link, the Bluetooth dongle chain, and runtime issues.
#
# Usage:
#   ./validate.sh              # Run all checks
#   ./validate.sh --json       # Output results as JSON (for automation)
#
# Exit codes:
#   0  All checks passed
#   1  One or more checks failed
#   2  Script error
#
# ------------------------------ tabstop = 4 ----------------------------------

set -euo pipefail

###############################################################################
# Auto-detect container and re-exec if needed
#
# This script must run inside the remote-radios container.  If we detect we're
# in the Barton devcontainer (or elsewhere), find the remote-radios container
# and re-exec there automatically.
###############################################################################
if [ ! -f /entrypoint.sh ] || ! grep -q "remote-radios" /entrypoint.sh 2>/dev/null; then
    # Not inside the remote-radios container.  Try to find it and re-exec.
    #
    # Multiple users may share the same Docker host.  Each user's containers
    # belong to a distinct Compose project whose name includes the username.
    # We scope the search to our own project to avoid matching another user's
    # remote-radios container.
    OTBR_CONTAINER=""

    # Determine docker command (with or without sudo).
    # The barton devcontainer does not have the Docker CLI installed but does
    # bind-mount the Docker socket.  Fall back to the curl+API approach when
    # the docker command is unavailable.
    DOCKER_CMD=""
    USE_CURL_API=false
    if command -v docker >/dev/null 2>&1 && docker ps >/dev/null 2>&1; then
        DOCKER_CMD="docker"
    elif command -v docker >/dev/null 2>&1 && command -v sudo >/dev/null 2>&1 && sudo docker ps >/dev/null 2>&1; then
        DOCKER_CMD="sudo docker"
    elif [ -S /var/run/docker.sock ]; then
        USE_CURL_API=true
    fi

    if [ "$USE_CURL_API" = true ]; then
        # Use the Docker Engine API via curl to find the remote-radios container.
        _DOCKER_API="http://localhost/v1.45"
        _CURL="curl -s --unix-socket /var/run/docker.sock"
        _SUDO=""
        # Socket may require sudo for access.
        if ! $_CURL "$_DOCKER_API/_ping" >/dev/null 2>&1; then
            _CURL="sudo curl -s --unix-socket /var/run/docker.sock"
        fi

        # Get our own compose project from our hostname (container ID).
        COMPOSE_PROJECT=$($_CURL "$_DOCKER_API/containers/$(hostname)/json" 2>/dev/null \
            | python3 -c "import json,sys; print(json.load(sys.stdin)['Config']['Labels'].get('com.docker.compose.project',''))" 2>/dev/null) || true

        if [ -n "$COMPOSE_PROJECT" ]; then
            OTBR_CONTAINER=$($_CURL "$_DOCKER_API/containers/json?filters=%7B%22label%22%3A%5B%22com.docker.compose.project%3D${COMPOSE_PROJECT}%22%5D%2C%22name%22%3A%5B%22remote-radios%22%5D%7D" 2>/dev/null \
                | python3 -c "import json,sys; cs=json.load(sys.stdin); print(cs[0]['Names'][0].lstrip('/') if cs else '')" 2>/dev/null) || true
        fi

        if [ -z "$OTBR_CONTAINER" ]; then
            OTBR_CONTAINER=$($_CURL "$_DOCKER_API/containers/json?filters=%7B%22name%22%3A%5B%22remote-radios%22%5D%7D" 2>/dev/null \
                | python3 -c "import json,sys; cs=json.load(sys.stdin); print(cs[0]['Names'][0].lstrip('/') if cs else '')" 2>/dev/null) || true
        fi

        if [ -n "$OTBR_CONTAINER" ]; then
            echo "Not in remote-radios container — re-executing inside ${OTBR_CONTAINER}..."
            echo ""
            # Use the Docker Engine API to exec into the container.
            # The script is base64-encoded and passed as a positional
            # parameter ($1) to avoid interpolating into the bash -c string.
            # Arguments are passed as additional positional params (${@:2})
            # so spaces and special characters are preserved without injection.
            _SCRIPT_B64=$(base64 -w0 "$0")
            _EXEC_PAYLOAD=$(python3 -c "
import json, sys
script_b64 = sys.argv[1]
args = sys.argv[2:]
cmd = ['bash', '-c', 'echo \"\$1\" | base64 -d | bash -s -- \"\${@:2}\"', '--', script_b64] + args
print(json.dumps({'Cmd': cmd, 'AttachStdout': True, 'AttachStderr': True}))
" "$_SCRIPT_B64" "$@")
            _EXEC_ID=$($_CURL -X POST "$_DOCKER_API/containers/${OTBR_CONTAINER}/exec" \
                -H "Content-Type: application/json" \
                -d "$_EXEC_PAYLOAD" \
                | python3 -c "import json,sys; print(json.load(sys.stdin)['Id'])")
            # Start the exec and demux the Docker multiplexed stream.
            $_CURL -X POST "$_DOCKER_API/exec/${_EXEC_ID}/start" \
                -H "Content-Type: application/json" \
                -d '{"Detach":false}' \
                | python3 -c "
import sys
buf = sys.stdin.buffer.read()
i = 0
while i + 8 <= len(buf):
    size = int.from_bytes(buf[i+4:i+8], 'big')
    if i + 8 + size <= len(buf):
        sys.stdout.buffer.write(buf[i+8:i+8+size])
    i += 8 + size
sys.stdout.buffer.flush()
"
            # Get the exit code from the exec instance.
            _EXIT=$($_CURL "$_DOCKER_API/exec/${_EXEC_ID}/json" \
                | python3 -c "import json,sys; print(json.load(sys.stdin).get('ExitCode',1))")
            exit "${_EXIT}"
        fi
    elif [ -n "$DOCKER_CMD" ]; then
        # If we're inside a Compose-managed container, read the project label
        # from our own container to scope the search.
        COMPOSE_PROJECT=$($DOCKER_CMD inspect "$(hostname)" \
            --format '{{index .Config.Labels "com.docker.compose.project"}}' 2>/dev/null) || true

        if [ -n "$COMPOSE_PROJECT" ]; then
            OTBR_CONTAINER=$($DOCKER_CMD ps \
                --filter "label=com.docker.compose.project=${COMPOSE_PROJECT}" \
                --filter "name=remote-radios" \
                --format '{{.Names}}' 2>/dev/null | head -1) || true
        fi

        # Fallback: broad search if project detection failed (e.g. running
        # from the host outside any container).
        if [ -z "$OTBR_CONTAINER" ]; then
            OTBR_CONTAINER=$($DOCKER_CMD ps \
                --filter "name=remote-radios" \
                --format '{{.Names}}' 2>/dev/null | head -1) || true
        fi
    fi

    if [ -n "$OTBR_CONTAINER" ]; then
        echo "Not in remote-radios container — re-executing inside ${OTBR_CONTAINER}..."
        echo ""
        # Pass the script via stdin and forward arguments.
        exec $DOCKER_CMD exec -i "$OTBR_CONTAINER" bash -s -- "$@" < "$0"
    else
        echo "ERROR: Not inside the remote-radios container and could not find one running." >&2
        echo "       Start the remote-radios container first, or run this script inside it:" >&2
        echo "       docker exec -i <remote-radios-container> bash < $0" >&2
        exit 2
    fi
fi

###############################################################################
# Configuration
###############################################################################
CPC_INSTANCE="${CPC_INSTANCE:-cpcd_0}"
SILABS_SOCKET="${SILABS_SOCKET:-}"
SILABS_DEVICE="${SILABS_DEVICE:-}"
BT_USBIP_SOCKET="${BT_USBIP_SOCKET:-}"
BT_USBIP_TCP_PORT="${BT_USBIP_TCP_PORT:-3240}"
BT_USBIP_BUSID="${BT_USBIP_BUSID:-}"
CPC_SOCKET_DIR="${CPC_SOCKET_DIR:-/dev/shm}"
CPC_SOCKET_BASE="${CPC_SOCKET_DIR}/cpcd/${CPC_INSTANCE}"
DBUS_DIR="${DBUS_DIR:-/var/run/remote-radios-dbus}"
DBUS_SOCKET_PATH="${DBUS_SOCKET_PATH:-${DBUS_DIR}/system_bus_socket}"
HOST_NETNS="/run/host-netns"
VIRTUAL_TTY="/dev/ttyRadio"

JSON_MODE=false
for arg in "$@"; do
    case "$arg" in
        --json) JSON_MODE=true ;;
    esac
done

###############################################################################
# Output helpers
###############################################################################
PASS_COUNT=0
FAIL_COUNT=0
WARN_COUNT=0
SKIP_COUNT=0
RESULTS=()

pass() {
    PASS_COUNT=$((PASS_COUNT + 1))
    RESULTS+=("PASS|$1|$2")
    if ! $JSON_MODE; then
        printf "  \033[32m✓ PASS\033[0m  %-40s %s\n" "$1" "$2"
    fi
}

fail() {
    FAIL_COUNT=$((FAIL_COUNT + 1))
    RESULTS+=("FAIL|$1|$2")
    if ! $JSON_MODE; then
        printf "  \033[31m✗ FAIL\033[0m  %-40s %s\n" "$1" "$2"
    fi
}

warn() {
    WARN_COUNT=$((WARN_COUNT + 1))
    RESULTS+=("WARN|$1|$2")
    if ! $JSON_MODE; then
        printf "  \033[33m⚠ WARN\033[0m  %-40s %s\n" "$1" "$2"
    fi
}

skip() {
    SKIP_COUNT=$((SKIP_COUNT + 1))
    RESULTS+=("SKIP|$1|$2")
    if ! $JSON_MODE; then
        printf "  \033[36m- SKIP\033[0m  %-40s %s\n" "$1" "$2"
    fi
}

section() {
    if ! $JSON_MODE; then
        echo ""
        printf "\033[1m[%s]\033[0m\n" "$1"
    fi
}

# is_process_alive — true if PID exists and is not a zombie.
is_process_alive() {
    local pid=$1
    kill -0 "$pid" 2>/dev/null || return 1
    local state
    state=$(awk '/^State:/ {print $2}' /proc/"$pid"/status 2>/dev/null) || return 1
    [[ "$state" != "Z" ]]
}

###############################################################################
# Section 1: Container Environment
###############################################################################
check_container_env() {
    section "Container Environment"

    # Privileged mode (needed for btattach, iptables, wpan0 creation)
    if ip link add dummy_priv_test type dummy 2>/dev/null; then
        ip link del dummy_priv_test 2>/dev/null
        pass "Privileged mode" "Container is privileged"
    else
        fail "Privileged mode" "Container is NOT privileged (required for btattach/iptables)"
    fi

    # Host network namespace mount
    if [ -e "${HOST_NETNS}" ]; then
        pass "Host netns mount" "${HOST_NETNS} exists"
        # Verify it's actually a network namespace
        if nsenter --net="${HOST_NETNS}" ip link show lo >/dev/null 2>&1; then
            pass "Host netns accessible" "Can enter host network namespace"
        else
            fail "Host netns accessible" "Cannot nsenter into ${HOST_NETNS}"
        fi
    else
        fail "Host netns mount" "${HOST_NETNS} not found — Bluetooth will not work"
    fi

    # Required commands
    for cmd in btattach bluetoothd hcitool hciconfig nsenter usbip python3 socat cpcd; do
        if command -v "$cmd" >/dev/null 2>&1; then
            pass "Command: $cmd" "$(command -v "$cmd")"
        else
            fail "Command: $cmd" "Not found in PATH"
        fi
    done
}

###############################################################################
# Section 2: D-Bus
###############################################################################
check_dbus() {
    section "D-Bus"

    # D-Bus socket directory
    if [ -d "${DBUS_DIR}" ]; then
        pass "D-Bus directory" "${DBUS_DIR} exists"
    else
        fail "D-Bus directory" "${DBUS_DIR} not found"
        return
    fi

    # D-Bus socket
    if [ -S "${DBUS_SOCKET_PATH}" ]; then
        pass "D-Bus socket" "${DBUS_SOCKET_PATH} exists"
    else
        fail "D-Bus socket" "${DBUS_SOCKET_PATH} not found"
    fi

    # D-Bus daemon process
    local dbus_pid
    dbus_pid=$(pgrep -x dbus-daemon 2>/dev/null | head -1) || true
    if [ -n "$dbus_pid" ]; then
        pass "D-Bus daemon" "PID $dbus_pid"
    else
        fail "D-Bus daemon" "dbus-daemon not running"
    fi

    # Verify we can talk to D-Bus
    if command -v dbus-send >/dev/null 2>&1; then
        if DBUS_SYSTEM_BUS_ADDRESS="unix:path=${DBUS_SOCKET_PATH}" \
           dbus-send --system --dest=org.freedesktop.DBus --print-reply \
           /org/freedesktop/DBus org.freedesktop.DBus.ListNames >/dev/null 2>&1; then
            pass "D-Bus connectivity" "Can list bus names"
        else
            fail "D-Bus connectivity" "Cannot communicate with D-Bus"
        fi
    else
        skip "D-Bus connectivity" "dbus-send not available"
    fi
}

###############################################################################
# Section 3: Silabs Radio (Zigbee/Thread) Connection
###############################################################################
check_radio() {
    section "Silabs Radio (Zigbee/Thread) Connection"

    if [ -n "${SILABS_SOCKET}" ]; then
        #----------------------------------------------------------------------
        # Remote serial-over-SSH tunnel mode (bind-mounted UNIX socket)
        #----------------------------------------------------------------------
        pass "Silabs mode" "Remote tunnel (socket ${SILABS_SOCKET})"

        # Bind-mounted UNIX socket must exist.
        if [ -S "${SILABS_SOCKET}" ]; then
            pass "Silabs tunnel socket" "${SILABS_SOCKET} present"
        else
            fail "Silabs tunnel socket" "${SILABS_SOCKET} not found — is remote-serial.py running on your workstation?"
        fi

        # socat process bridging the socket to the PTY.
        local socat_pid
        socat_pid=$(pgrep -x socat 2>/dev/null | head -1) || true
        if [ -n "$socat_pid" ]; then
            if is_process_alive "$socat_pid"; then
                pass "socat bridge" "PID $socat_pid (alive)"
            else
                fail "socat bridge" "PID $socat_pid (ZOMBIE)"
            fi
        else
            fail "socat bridge" "socat not running — ${VIRTUAL_TTY} will not exist"
        fi

        # Virtual PTY device that cpcd opens.
        if [ -e "${VIRTUAL_TTY}" ]; then
            local pty_target
            pty_target=$(readlink -f "${VIRTUAL_TTY}" 2>/dev/null) || pty_target="${VIRTUAL_TTY}"
            if [ -c "$pty_target" ]; then
                pass "Virtual serial device" "${VIRTUAL_TTY} → ${pty_target} (char device)"
            else
                fail "Virtual serial device" "${VIRTUAL_TTY} exists but ${pty_target} is not a character device"
            fi
        else
            fail "Virtual serial device" "${VIRTUAL_TTY} not found — socat may not have started"
        fi

    elif [ -n "${SILABS_DEVICE}" ]; then
        #----------------------------------------------------------------------
        # Local USB radio mode
        #----------------------------------------------------------------------
        pass "Silabs mode" "Local USB (${SILABS_DEVICE})"

        if [ -e "${SILABS_DEVICE}" ]; then
            pass "Silabs device" "${SILABS_DEVICE} exists"
            if [ -c "${SILABS_DEVICE}" ]; then
                pass "Silabs device type" "Character device"
            else
                fail "Silabs device type" "Not a character device"
            fi
        else
            fail "Silabs device" "${SILABS_DEVICE} not found — is the radio connected?"
        fi

    else
        fail "Silabs mode" "Neither SILABS_SOCKET nor SILABS_DEVICE is set"
        return
    fi

    # cpcd process (runs in both modes)
    local cpcd_pid
    cpcd_pid=$(pgrep -x cpcd 2>/dev/null | head -1) || true
    if [ -n "$cpcd_pid" ]; then
        if is_process_alive "$cpcd_pid"; then
            pass "cpcd process" "PID $cpcd_pid (alive)"
        else
            fail "cpcd process" "PID $cpcd_pid (ZOMBIE)"
        fi
    else
        fail "cpcd process" "cpcd not running"
    fi

    # CPC socket directory
    if [ -d "${CPC_SOCKET_BASE}" ]; then
        pass "CPC socket directory" "${CPC_SOCKET_BASE} exists"
    else
        fail "CPC socket directory" "${CPC_SOCKET_BASE} not found"
        return
    fi

    # Individual sockets
    for sock in ctrl.cpcd.sock reset.cpcd.sock; do
        local sockpath="${CPC_SOCKET_BASE}/${sock}"
        if [ -S "$sockpath" ]; then
            pass "CPC socket: $sock" "Present"
        else
            fail "CPC socket: $sock" "Not found at ${sockpath}"
        fi
    done

    # Thread/Spinel endpoint socket (ep12).  It is created once otbr-agent
    # connects to cpcd, so treat its absence as a warning (may still be starting)
    # rather than a hard failure.  BLE no longer uses a CPC endpoint (ep14) — it
    # is a dedicated usb-ip dongle now — so ep14 is intentionally not checked.
    local ep12sock="${CPC_SOCKET_BASE}/ep12.cpcd.sock"
    if [ -S "$ep12sock" ]; then
        pass "CPC socket: ep12 (Thread/Spinel)" "Present"
    else
        warn "CPC socket: ep12 (Thread/Spinel)" "Not found yet — otbr-agent may still be connecting to cpcd"
    fi
}

###############################################################################
# Section 4: Bluetooth Dongle (usb-ip → HCI → bluetoothd)
###############################################################################
check_ble_chain() {
    section "Bluetooth Dongle (usb-ip → HCI → bluetoothd)"

    if [ -z "${BT_USBIP_SOCKET}" ]; then
        skip "Bluetooth dongle" "No Bluetooth dongle configured (BT_USBIP_SOCKET unset)"
        return
    fi

    # usb-ip socket bridge present (workstation usbipd reverse-tunnelled here).
    if [ -S "${BT_USBIP_SOCKET}" ]; then
        pass "usb-ip socket" "${BT_USBIP_SOCKET} present"
    else
        fail "usb-ip socket" "${BT_USBIP_SOCKET} missing — is remote-radios-setup.sh running on your workstation?"
    fi

    # Imported usb-ip device.
    if usbip port 2>/dev/null | grep -q "^Port [0-9]"; then
        pass "usb-ip imported device" "usbip port shows an attached device"
    else
        fail "usb-ip imported device" "usbip port shows no imported device — dongle not attached"
    fi

    # Bluetooth HCI device present in the host netns.
    local hci_list
    hci_list=$(nsenter --net="${HOST_NETNS}" ls /sys/class/bluetooth/ 2>/dev/null) || true
    if [ -n "$hci_list" ]; then
        pass "Bluetooth HCI device" "Present in host netns: $(echo "$hci_list" | tr '\n' ' ')"
    else
        fail "Bluetooth HCI device" "No HCI device in host netns — usb-ip attach may have failed"
    fi

    # bluetoothd
    local bluetoothd_pid
    bluetoothd_pid=$(pgrep -x bluetoothd 2>/dev/null | head -1) || true
    if [ -n "$bluetoothd_pid" ]; then
        if is_process_alive "$bluetoothd_pid"; then
            pass "bluetoothd" "PID $bluetoothd_pid (alive)"
        else
            fail "bluetoothd" "PID $bluetoothd_pid (ZOMBIE)"
        fi
    else
        fail "bluetoothd" "Not running — Bluetooth operations will fail"
    fi

    # ble_adapter_id must exist and match an existing hci device.
    local adapter_file="${DBUS_DIR}/ble_adapter_id"
    if [ -f "$adapter_file" ]; then
        local idx
        idx=$(cat "$adapter_file" 2>/dev/null) || idx=""
        if [ -z "$idx" ]; then
            fail "BLE adapter ID file" "${adapter_file} exists but is empty"
        elif nsenter --net="${HOST_NETNS}" test -e "/sys/class/bluetooth/hci${idx}" 2>/dev/null; then
            pass "BLE adapter ID file" "Index ${idx} matches hci${idx}"

            # Reuse the HCI health idea against the dongle's adapter index.
            if timeout 5 nsenter --net="${HOST_NETNS}" \
               hciconfig "hci${idx}" version >/dev/null 2>&1; then
                pass "Dongle HCI health" "hci${idx} responsive (hciconfig version)"
            else
                fail "Dongle HCI health" "hci${idx} unresponsive — usb-ip session may be dead"
            fi
        else
            fail "BLE adapter ID file" "Index ${idx} but hci${idx} does not exist in host netns"
        fi
    else
        fail "BLE adapter ID file" "${adapter_file} not found — Matter won't know which adapter to use"
    fi
}

###############################################################################
# Section 5: HCI Adapter & Transport
###############################################################################
check_hci() {
    section "HCI Adapter & Transport"

    if [ -z "${BT_USBIP_SOCKET}" ]; then
        skip "HCI checks" "No Bluetooth dongle configured (BT_USBIP_SOCKET unset)"
        return
    fi

    if [ ! -e "${HOST_NETNS}" ]; then
        skip "HCI checks" "Host netns not available"
        return
    fi

    # List all HCI adapters in host netns
    local hci_list
    hci_list=$(nsenter --net="${HOST_NETNS}" ls /sys/class/bluetooth/ 2>/dev/null) || true
    if [ -z "$hci_list" ]; then
        fail "HCI adapters" "No HCI adapters found in host netns"
        return
    fi

    # BLE adapter ID file (the dongle's adapter index)
    local adapter_file="${DBUS_DIR}/ble_adapter_id"
    local expected_idx=""
    if [ -f "$adapter_file" ]; then
        expected_idx=$(cat "$adapter_file" 2>/dev/null)
        if [ -n "$expected_idx" ]; then
            pass "BLE adapter ID file" "Index ${expected_idx} (hci${expected_idx})"
        else
            fail "BLE adapter ID file" "File exists but is empty"
        fi
    else
        fail "BLE adapter ID file" "${adapter_file} not found — Matter won't know which adapter to use"
    fi

    for hci in $hci_list; do
        local idx="${hci#hci}"
        local usb_product
        usb_product=$(nsenter --net="${HOST_NETNS}" cat "/sys/class/bluetooth/${hci}/device/../product" 2>/dev/null) || usb_product=""
        local info="USB${usb_product:+ ($usb_product)}"

        local up_state
        if nsenter --net="${HOST_NETNS}" hciconfig "$hci" 2>/dev/null | grep -q "UP RUNNING"; then
            up_state="UP"
        else
            up_state="DOWN"
        fi

        if [ "$idx" = "$expected_idx" ]; then
            pass "HCI adapter: ${hci}" "${info} — state=${up_state} [SELECTED — Bluetooth dongle]"
        else
            if [ "$up_state" = "UP" ]; then
                warn "HCI adapter: ${hci}" "${info} — state=${up_state} (other adapter — may confuse bluetoothd)"
            else
                pass "HCI adapter: ${hci}" "${info} — state=${up_state} (other adapter, powered down)"
            fi
        fi
    done

    # HCI transport health against the dongle's adapter index.
    if [ -n "$expected_idx" ]; then
        local target_hci="hci${expected_idx}"
        if nsenter --net="${HOST_NETNS}" hciconfig "${target_hci}" 2>/dev/null | grep -q "UP RUNNING"; then
            if timeout 5 nsenter --net="${HOST_NETNS}" \
               hciconfig "${target_hci}" version >/dev/null 2>&1; then
                pass "HCI transport (version)" "Responsive on ${target_hci}"
            else
                fail "HCI transport (version)" "No response from ${target_hci} within 5s — transport dead"
            fi

            local bd_addr
            bd_addr=$(nsenter --net="${HOST_NETNS}" hciconfig "${target_hci}" 2>/dev/null \
                | awk '/BD Address:/{print $3}') || bd_addr=""
            if [ -n "$bd_addr" ]; then
                pass "BLE BD Address" "$bd_addr"
            fi
        else
            fail "HCI transport" "${target_hci} is not UP — usb-ip session or bluetoothd may have died"
        fi
    fi
}

###############################################################################
# Section 6: BLE Scanning Capability
###############################################################################

check_ble_scan() {
    section "BLE Scanning"

    if [ -z "${BT_USBIP_SOCKET}" ]; then
        skip "BLE scan test" "No Bluetooth dongle configured (BT_USBIP_SOCKET unset)"
        return
    fi

    local adapter_file="${DBUS_DIR}/ble_adapter_id"
    if [ ! -f "$adapter_file" ]; then
        skip "BLE scan test" "No adapter ID file — cannot determine adapter"
        return
    fi

    local idx
    idx=$(cat "$adapter_file" 2>/dev/null)
    if [ -z "$idx" ]; then
        skip "BLE scan test" "Empty adapter ID"
        return
    fi

    local target_hci="hci${idx}"

    # Use bluetoothctl for scanning (via D-Bus → bluetoothd).
    # IMPORTANT: Do NOT use raw hcitool lescan — it bypasses bluetoothd
    # and corrupts its internal state machine.
    local bd_addr
    bd_addr=$(nsenter --net="${HOST_NETNS}" hciconfig "${target_hci}" 2>/dev/null \
        | awk '/BD Address:/{print $3}') || bd_addr=""

    if [ -z "$bd_addr" ]; then
        fail "BLE LE scan" "Cannot determine BD address for ${target_hci}"
        return
    fi

    local btctl_output
    btctl_output=$(
        {
            echo "select ${bd_addr}"
            echo "scan on"
            sleep 4
            echo "scan off"
            sleep 1
            echo "quit"
        } | DBUS_SYSTEM_BUS_ADDRESS="unix:path=${DBUS_SOCKET_PATH}" \
            timeout 10 nsenter --net="${HOST_NETNS}" \
            bluetoothctl --agent=NoInputNoOutput 2>&1
    ) || true

    if echo "$btctl_output" | grep -qi "NEW.*Device\|CHG.*RSSI"; then
        local btctl_count
        btctl_count=$(echo "$btctl_output" | grep -ciE "NEW.*Device") || btctl_count=0
        pass "BLE LE scan" "Found ${btctl_count} device(s) via D-Bus on ${target_hci}"
    elif echo "$btctl_output" | grep -qi "SetDiscoveryFilter\|Discovery started\|scan on"; then
        warn "BLE LE scan" "Scan started on ${target_hci} but no devices found in 6s window"
    elif echo "$btctl_output" | grep -qi "InProgress"; then
        fail "BLE LE scan" "bluetoothd reports scan InProgress — a stale scan may be stuck"
    elif echo "$btctl_output" | grep -qi "not available\|not found"; then
        fail "BLE LE scan" "Controller ${target_hci} (${bd_addr}) not available to bluetoothd"
    else
        fail "BLE LE scan" "Unexpected: $(echo "$btctl_output" | tail -3 | tr '\n' ' ')"
    fi
}

###############################################################################
# Section 7: otbr-agent
###############################################################################
check_otbr() {
    section "otbr-agent (Thread Border Router)"

    local otbr_pid
    otbr_pid=$(pgrep -x otbr-agent 2>/dev/null | head -1) || true
    if [ -n "$otbr_pid" ]; then
        if is_process_alive "$otbr_pid"; then
            pass "otbr-agent process" "PID $otbr_pid (alive)"

            local etime
            etime=$(ps -o etime= -p "$otbr_pid" 2>/dev/null | tr -d ' ') || etime=""
            if [ -n "$etime" ]; then
                pass "otbr-agent uptime" "$etime"
            fi
        else
            fail "otbr-agent process" "PID $otbr_pid (ZOMBIE)"
        fi
    else
        fail "otbr-agent process" "Not running"
    fi

    # Check wpan0 interface
    if ip link show wpan0 >/dev/null 2>&1; then
        local wpan_state
        wpan_state=$(ip -br link show wpan0 2>/dev/null | awk '{print $2}') || wpan_state="unknown"
        pass "wpan0 interface" "State: ${wpan_state}"

        local v6_addrs
        v6_addrs=$(ip -6 addr show dev wpan0 scope global 2>/dev/null | grep -c "inet6") || v6_addrs=0
        if [ "$v6_addrs" -gt 0 ]; then
            pass "wpan0 IPv6 addresses" "${v6_addrs} global address(es)"
        else
            warn "wpan0 IPv6 addresses" "No global IPv6 — Thread network may not be formed"
        fi
    else
        warn "wpan0 interface" "Not found yet — otbr-agent may still be starting (created a few seconds after cpcd is ready)"
    fi

    # Check avahi-daemon
    local avahi_pid
    avahi_pid=$(pgrep -x avahi-daemon 2>/dev/null | head -1) || true
    if [ -n "$avahi_pid" ]; then
        pass "avahi-daemon" "PID $avahi_pid"
    else
        warn "avahi-daemon" "Not running (otbr-agent may fail with OTBR_MDNS=avahi)"
    fi
}

###############################################################################
# Section 8: Known Pitfalls
###############################################################################
check_known_pitfalls() {
    section "Known Pitfalls & Lessons Learned"

    # Check for stale CPC sockets
    if [ -S "${CPC_SOCKET_BASE}/ctrl.cpcd.sock" ]; then
        local ctrl_established
        ctrl_established=$(ss -x 2>/dev/null | grep -c "ctrl.cpcd.sock" 2>/dev/null) || ctrl_established=0
        if [ "$ctrl_established" -gt 0 ]; then
            pass "ctrl.cpcd.sock liveness" "${ctrl_established} established connection(s)"
        else
            warn "ctrl.cpcd.sock liveness" "Socket file exists but no established connections — may be stale"
        fi
    fi

    # ep12 (Thread/Spinel) endpoint check
    if [ -S "${CPC_SOCKET_BASE}/ep12.cpcd.sock" ]; then
        pass "ep12 Spinel endpoint" "Thread/Spinel CPC endpoint present"
    else
        warn "ep12 Spinel endpoint" "ep12 socket missing — if cpcd keeps failing, restart it"
    fi

    # Multiple HCI adapters
    local hci_count
    hci_count=$(nsenter --net="${HOST_NETNS}" ls /sys/class/bluetooth/ 2>/dev/null | wc -w) || hci_count=0
    if [ "$hci_count" -gt 1 ]; then
        local adapter_file="${DBUS_DIR}/ble_adapter_id"
        if [ -f "$adapter_file" ] && [ -n "$(cat "$adapter_file" 2>/dev/null)" ]; then
            warn "Multiple HCI adapters" "${hci_count} adapters present — ble_adapter_id is set, but bluetoothd may default to wrong one"
        else
            fail "Multiple HCI adapters" "${hci_count} adapters but no ble_adapter_id — Matter will likely use wrong adapter"
        fi
    elif [ "$hci_count" -eq 1 ]; then
        pass "Single HCI adapter" "Only one adapter — no confusion risk"
    fi

    # Dangerous HCI Reset
    if grep -q "hcitool.*cmd.*0x03.*0x0003" /entrypoint.sh 2>/dev/null; then
        warn "HCI Reset in entrypoint" "Found HCI Reset command — this can disrupt the dongle transport"
    else
        pass "No HCI Reset in entrypoint" "Dangerous reset sequence not present"
    fi

    # Raw HCI scan
    if grep -rq "hcitool.*lescan" /entrypoint.sh 2>/dev/null; then
        fail "Raw HCI scan in entrypoint" "hcitool lescan corrupts bluetoothd state — use bluetoothctl instead"
    else
        pass "No raw HCI scan in entrypoint" "Not using hcitool lescan (safe for bluetoothd)"
    fi
}

###############################################################################
# Summary
###############################################################################
print_summary() {
    if $JSON_MODE; then
        echo "{"
        echo "  \"pass\": $PASS_COUNT,"
        echo "  \"fail\": $FAIL_COUNT,"
        echo "  \"warn\": $WARN_COUNT,"
        echo "  \"skip\": $SKIP_COUNT,"
        echo "  \"results\": ["
        local first=true
        for r in "${RESULTS[@]}"; do
            IFS='|' read -r status name detail <<< "$r"
            if $first; then first=false; else echo ","; fi
            printf '    {"status":"%s","check":"%s","detail":"%s"}' \
                "$status" "$name" "$(echo "$detail" | sed 's/"/\\"/g')"
        done
        echo ""
        echo "  ]"
        echo "}"
    else
        echo ""
        echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
        printf "  \033[32m%d passed\033[0m  " "$PASS_COUNT"
        if [ "$FAIL_COUNT" -gt 0 ]; then
            printf "\033[31m%d failed\033[0m  " "$FAIL_COUNT"
        else
            printf "0 failed  "
        fi
        if [ "$WARN_COUNT" -gt 0 ]; then
            printf "\033[33m%d warnings\033[0m  " "$WARN_COUNT"
        else
            printf "0 warnings  "
        fi
        printf "%d skipped\n" "$SKIP_COUNT"
        echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

        if [ "$FAIL_COUNT" -gt 0 ]; then
            echo ""
            printf "\033[31mFailed checks:\033[0m\n"
            for r in "${RESULTS[@]}"; do
                IFS='|' read -r status name detail <<< "$r"
                if [ "$status" = "FAIL" ]; then
                    printf "  ✗ %-40s %s\n" "$name" "$detail"
                fi
            done
        fi
    fi
}

###############################################################################
# Main
###############################################################################

# Detect Silabs radio mode for header
SILABS_MODE_DISPLAY="Not configured"
if [ -n "${SILABS_SOCKET}" ]; then
    SILABS_MODE_DISPLAY="Remote tunnel (${SILABS_SOCKET})"
elif [ -n "${SILABS_DEVICE}" ]; then
    SILABS_MODE_DISPLAY="Local USB (${SILABS_DEVICE})"
fi

# Detect Bluetooth dongle mode for header
if [ -n "${BT_USBIP_SOCKET}" ]; then
    BT_MODE_DISPLAY="usb-ip (${BT_USBIP_SOCKET})"
else
    BT_MODE_DISPLAY="Not configured (BT_USBIP_SOCKET unset)"
fi

if ! $JSON_MODE; then
    echo "╔══════════════════════════════════════════════════════════════╗"
    echo "║         Remote Radios Validation                           ║"
    echo "╠══════════════════════════════════════════════════════════════╣"
    printf "║  Instance:   %-47s║\n" "${CPC_INSTANCE}"
    printf "║  Silabs:     %-47s║\n" "${SILABS_MODE_DISPLAY}"
    printf "║  Bluetooth:  %-47s║\n" "${BT_MODE_DISPLAY}"
    printf "║  Date:       %-47s║\n" "$(date -Iseconds)"
    echo "╚══════════════════════════════════════════════════════════════╝"
fi

check_container_env
check_dbus
check_radio
check_ble_chain
check_hci
check_ble_scan
check_otbr
check_known_pitfalls
print_summary

if [ "$FAIL_COUNT" -gt 0 ]; then
    exit 1
fi
exit 0
