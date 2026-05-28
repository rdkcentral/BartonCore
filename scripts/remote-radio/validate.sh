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
# Radio Validation Script
#
# Performs a detailed check of all requirements for the real-radio chain
# to function correctly.  Run this inside the Barton or otbr-radio
# container to diagnose connectivity, BLE chain, and runtime issues.
#
# Usage:
#   ./validate.sh              # Run all checks
#   ./validate.sh --fix        # Attempt to fix common issues
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
# This script must run inside the otbr-radio container.  If we detect we're
# in the Barton devcontainer (or elsewhere), find the otbr-radio container
# and re-exec there automatically.
###############################################################################
if [ ! -f /entrypoint.sh ] || ! grep -q "otbr-radio" /entrypoint.sh 2>/dev/null; then
    # Not inside the otbr-radio container.  Try to find it and re-exec.
    #
    # Multiple users may share the same Docker host.  Each user's containers
    # belong to a distinct Compose project whose name includes the username.
    # We scope the search to our own project to avoid matching another user's
    # otbr-radio container.
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
        # Use the Docker Engine API via curl to find the otbr-radio container.
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
            OTBR_CONTAINER=$($_CURL "$_DOCKER_API/containers/json?filters=%7B%22label%22%3A%5B%22com.docker.compose.project%3D${COMPOSE_PROJECT}%22%5D%2C%22name%22%3A%5B%22otbr-radio%22%5D%7D" 2>/dev/null \
                | python3 -c "import json,sys; cs=json.load(sys.stdin); print(cs[0]['Names'][0].lstrip('/') if cs else '')" 2>/dev/null) || true
        fi

        if [ -z "$OTBR_CONTAINER" ]; then
            OTBR_CONTAINER=$($_CURL "$_DOCKER_API/containers/json?filters=%7B%22name%22%3A%5B%22otbr-radio%22%5D%7D" 2>/dev/null \
                | python3 -c "import json,sys; cs=json.load(sys.stdin); print(cs[0]['Names'][0].lstrip('/') if cs else '')" 2>/dev/null) || true
        fi

        if [ -n "$OTBR_CONTAINER" ]; then
            echo "Not in otbr-radio container — re-executing inside ${OTBR_CONTAINER}..."
            echo ""
            # Use the Docker Engine API to exec into the container.
            # Read the script content, base64-encode it, and pass as a
            # command argument to avoid Docker exec stdin piping issues.
            _ESCAPED_ARGS=""
            for arg in "$@"; do
                _ESCAPED_ARGS="${_ESCAPED_ARGS}, \"${arg}\""
            done
            _SCRIPT_B64=$(base64 -w0 "$0")
            _EXEC_ID=$($_CURL -X POST "$_DOCKER_API/containers/${OTBR_CONTAINER}/exec" \
                -H "Content-Type: application/json" \
                -d "{\"Cmd\":[\"bash\", \"-c\", \"echo ${_SCRIPT_B64} | base64 -d | bash -s -- ${*}\"],\"AttachStdout\":true,\"AttachStderr\":true}" \
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
                --filter "name=otbr-radio" \
                --format '{{.Names}}' 2>/dev/null | head -1) || true
        fi

        # Fallback: broad search if project detection failed (e.g. running
        # from the host outside any container).
        if [ -z "$OTBR_CONTAINER" ]; then
            OTBR_CONTAINER=$($DOCKER_CMD ps \
                --filter "name=otbr-radio" \
                --format '{{.Names}}' 2>/dev/null | head -1) || true
        fi
    fi

    if [ -n "$OTBR_CONTAINER" ]; then
        echo "Not in otbr-radio container — re-executing inside ${OTBR_CONTAINER}..."
        echo ""
        # Pass the script via stdin and forward arguments.
        exec $DOCKER_CMD exec -i "$OTBR_CONTAINER" bash -s -- "$@" < "$0"
    else
        echo "ERROR: Not inside the otbr-radio container and could not find one running." >&2
        echo "       Start the otbr-radio container first, or run this script inside it:" >&2
        echo "       docker exec -i <otbr-radio-container> bash < $0" >&2
        exit 2
    fi
fi

###############################################################################
# Configuration
###############################################################################
CPC_INSTANCE="${CPC_INSTANCE:-cpcd_0}"
RADIO_PORT="${RADIO_PORT:-}"
RADIO_HOST="${RADIO_HOST:-host.docker.internal}"
RADIO_DEVICE="${RADIO_DEVICE:-}"
CPC_SOCKET_DIR="${CPC_SOCKET_DIR:-/dev/shm}"
CPC_SOCKET_BASE="${CPC_SOCKET_DIR}/cpcd/${CPC_INSTANCE}"
DBUS_DIR="${DBUS_DIR:-/var/run/otbr-dbus}"
DBUS_SOCKET_PATH="${DBUS_SOCKET_PATH:-${DBUS_DIR}/system_bus_socket}"
HOST_NETNS="/run/host-netns"
BT_BRIDGE_DIR="/var/run/bt-hci-bridge"
HCI_PROXY_SCRIPT="/opt/cpc-proxy/hci_pty_proxy.py"

FIX_MODE=false
JSON_MODE=false
for arg in "$@"; do
    case "$arg" in
        --fix)  FIX_MODE=true ;;
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
        fail "Host netns mount" "${HOST_NETNS} not found — BLE will not work"
    fi

    # HCI PTY proxy script
    if [ -f "${HCI_PROXY_SCRIPT}" ]; then
        pass "HCI PTY proxy script" "${HCI_PROXY_SCRIPT} present"
    else
        warn "HCI PTY proxy script" "${HCI_PROXY_SCRIPT} not found — Extended Advertising workaround unavailable"
    fi

    # Required commands
    for cmd in btattach bluetoothd hcitool hciconfig nsenter bt_host_cpc_hci_bridge python3 socat cpcd; do
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
# Section 3: Radio Connection
###############################################################################
check_radio() {
    section "Radio Connection"

    if [ -n "${RADIO_PORT}" ]; then
        #----------------------------------------------------------------------
        # Remote serial tunnel mode
        #----------------------------------------------------------------------
        pass "Radio mode" "Remote serial tunnel (port ${RADIO_PORT})"

        # Resolve to IPv4 (the SSH tunnel binds on 0.0.0.0)
        local radio_host_v4
        radio_host_v4=$(getent ahostsv4 "${RADIO_HOST}" 2>/dev/null | awk '{print $1; exit}') || true
        if [ -z "${radio_host_v4}" ]; then
            radio_host_v4="${RADIO_HOST}"
        fi

        # TCP connectivity to the tunnel endpoint
        if timeout 3 bash -c "echo >/dev/tcp/${radio_host_v4}/${RADIO_PORT}" 2>/dev/null; then
            pass "Tunnel TCP connectivity" "${radio_host_v4}:${RADIO_PORT} reachable"
        else
            fail "Tunnel TCP connectivity" "Cannot connect to ${radio_host_v4}:${RADIO_PORT} — is remote-serial.py running on your workstation?"
        fi

        # socat process
        local socat_pid
        socat_pid=$(pgrep -x socat 2>/dev/null | head -1) || true
        if [ -n "$socat_pid" ]; then
            if is_process_alive "$socat_pid"; then
                pass "socat bridge" "PID $socat_pid (alive)"
            else
                fail "socat bridge" "PID $socat_pid (ZOMBIE)"
            fi
        else
            fail "socat bridge" "socat not running — virtual serial device will not exist"
        fi

        # Virtual PTY device
        if [ -L "/dev/ttyRadio" ]; then
            local pty_target
            pty_target=$(readlink -f "/dev/ttyRadio" 2>/dev/null) || pty_target=""
            if [ -c "$pty_target" ]; then
                pass "Virtual serial device" "/dev/ttyRadio → ${pty_target}"
            else
                fail "Virtual serial device" "/dev/ttyRadio exists but target ${pty_target} is not a character device"
            fi
        else
            fail "Virtual serial device" "/dev/ttyRadio not found — socat may not have started"
        fi

    elif [ -n "${RADIO_DEVICE}" ]; then
        #----------------------------------------------------------------------
        # Local USB radio mode
        #----------------------------------------------------------------------
        pass "Radio mode" "Local USB (${RADIO_DEVICE})"

        if [ -e "${RADIO_DEVICE}" ]; then
            pass "Radio device" "${RADIO_DEVICE} exists"
            if [ -c "${RADIO_DEVICE}" ]; then
                pass "Radio device type" "Character device"
            else
                warn "Radio device type" "Not a character device"
            fi
        else
            fail "Radio device" "${RADIO_DEVICE} not found — is the radio connected?"
        fi

    else
        fail "Radio mode" "Neither RADIO_PORT nor RADIO_DEVICE is set"
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

    # Endpoint sockets (ep14 = BLE, ep12 = Thread/Spinel)
    for ep in 14 12; do
        local sockpath="${CPC_SOCKET_BASE}/ep${ep}.cpcd.sock"
        if [ -S "$sockpath" ]; then
            pass "CPC socket: ep${ep} data" "Present"
        else
            fail "CPC socket: ep${ep} data" "Not found — endpoint may be in EAGAIN state"
        fi

        local eventsock="${CPC_SOCKET_BASE}/ep${ep}.event.cpcd.sock"
        if [ -S "$eventsock" ]; then
            pass "CPC socket: ep${ep} event" "Present"
        else
            warn "CPC socket: ep${ep} event" "Not found (events may not be available)"
        fi
    done
}

###############################################################################
# Section 4: BLE Chain
###############################################################################
check_ble_chain() {
    section "BLE Chain (bridge → pty_proxy → btattach → bluetoothd)"

    # bt_host_cpc_hci_bridge
    local bridge_pid
    bridge_pid=$(pgrep -f "bt_host_cpc_hci_bridge" 2>/dev/null | head -1) || true
    if [ -n "$bridge_pid" ]; then
        if is_process_alive "$bridge_pid"; then
            pass "bt_host_cpc_hci_bridge" "PID $bridge_pid (alive)"
        else
            fail "bt_host_cpc_hci_bridge" "PID $bridge_pid (ZOMBIE)"
        fi
    else
        fail "bt_host_cpc_hci_bridge" "Not running"
    fi

    # PTY from bridge
    if [ -L "${BT_BRIDGE_DIR}/pts_hci" ]; then
        local pts_target
        pts_target=$(readlink -f "${BT_BRIDGE_DIR}/pts_hci")
        if [ -c "$pts_target" ]; then
            pass "Bridge PTY (pts_hci)" "${pts_target}"
        else
            fail "Bridge PTY (pts_hci)" "Symlink exists but ${pts_target} is not a character device"
        fi
    else
        fail "Bridge PTY (pts_hci)" "Symlink not found at ${BT_BRIDGE_DIR}/pts_hci"
    fi

    # HCI PTY proxy
    local proxy_pid
    proxy_pid=$(pgrep -f "hci_pty_proxy.py" 2>/dev/null | head -1) || true
    if [ -n "$proxy_pid" ]; then
        if is_process_alive "$proxy_pid"; then
            pass "HCI PTY proxy" "PID $proxy_pid (alive)"
        else
            fail "HCI PTY proxy" "PID $proxy_pid (ZOMBIE)"
        fi
    else
        if [ -f "${HCI_PROXY_SCRIPT}" ]; then
            fail "HCI PTY proxy" "Not running (script present but process missing)"
        else
            skip "HCI PTY proxy" "Script not installed — Extended Advertising workaround unavailable"
        fi
    fi

    # btattach
    local btattach_pid
    btattach_pid=$(pgrep -x btattach 2>/dev/null | head -1) || true
    if [ -n "$btattach_pid" ]; then
        if is_process_alive "$btattach_pid"; then
            pass "btattach" "PID $btattach_pid (alive)"
        else
            fail "btattach" "PID $btattach_pid (ZOMBIE)"
        fi
    else
        fail "btattach" "Not running — no HCI device will be created"
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
        fail "bluetoothd" "Not running — BLE operations will fail"
    fi

    # BLE monitor
    local monitor_found=false
    for child_pid in $(pgrep -P 1 2>/dev/null); do
        local child_cmd
        child_cmd=$(cat /proc/"$child_pid"/cmdline 2>/dev/null | tr '\0' ' ') || continue
        if echo "$child_cmd" | grep -q "entrypoint.sh"; then
            pass "BLE monitor" "PID $child_pid (entrypoint subshell)"
            monitor_found=true
            break
        fi
    done
    if ! $monitor_found; then
        warn "BLE monitor" "No monitor subshell found — BLE chain may not auto-recover"
    fi
}

###############################################################################
# Section 5: HCI Adapter & Transport
###############################################################################
check_hci() {
    section "HCI Adapter & Transport"

    if [ ! -e "${HOST_NETNS}" ]; then
        skip "HCI checks" "Host netns not available"
        return
    fi

    # List all HCI adapters in host netns
    local hci_list
    hci_list=$(ls /sys/class/bluetooth/ 2>/dev/null) || true
    if [ -z "$hci_list" ]; then
        fail "HCI adapters" "No HCI adapters found in host netns"
        return
    fi

    # BLE adapter ID file
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
        local info=""
        local is_cpc=false

        # The CPC bridge adapter uses UART bus type
        if nsenter --net="${HOST_NETNS}" hciconfig "$hci" 2>/dev/null | grep -q "Bus: UART"; then
            is_cpc=true
            info="UART (CPC bridge)"
        else
            local usb_product
            usb_product=$(cat "/sys/class/bluetooth/${hci}/device/../product" 2>/dev/null) || usb_product=""
            info="USB${usb_product:+ ($usb_product)}"
        fi

        local up_state
        if nsenter --net="${HOST_NETNS}" hciconfig "$hci" 2>/dev/null | grep -q "UP RUNNING"; then
            up_state="UP"
        else
            up_state="DOWN"
        fi

        if [ "$idx" = "$expected_idx" ]; then
            if $is_cpc; then
                pass "HCI adapter: ${hci}" "${info} — state=${up_state} [SELECTED]"
            else
                warn "HCI adapter: ${hci}" "${info} — state=${up_state} [SELECTED but not CPC!]"
            fi
        else
            if $is_cpc; then
                warn "HCI adapter: ${hci}" "${info} — state=${up_state} (CPC adapter not selected?)"
            else
                if [ "$up_state" = "UP" ]; then
                    warn "HCI adapter: ${hci}" "${info} — state=${up_state} (host adapter — may confuse bluetoothd)"
                else
                    pass "HCI adapter: ${hci}" "${info} — state=${up_state} (host adapter, powered down)"
                fi
            fi
        fi
    done

    # HCI transport health — use Read BD ADDR (0x04|0x0009).
    # IMPORTANT: Do NOT use hciconfig name / Read Local Name (0x03|0x0014).
    # The Silicon Labs CPC BLE firmware does NOT support it and returns
    # "Unknown HCI Command", which hciconfig misreports as "I/O error".
    if [ -n "$expected_idx" ]; then
        local target_hci="hci${expected_idx}"
        if nsenter --net="${HOST_NETNS}" hciconfig "${target_hci}" 2>/dev/null | grep -q "UP RUNNING"; then
            if timeout 5 nsenter --net="${HOST_NETNS}" \
               hcitool -i "${target_hci}" cmd 0x04 0x0009 >/dev/null 2>&1; then
                pass "HCI transport (Read BD ADDR)" "Responsive on ${target_hci}"

                local bd_addr
                bd_addr=$(timeout 5 nsenter --net="${HOST_NETNS}" \
                    hcitool -i "${target_hci}" cmd 0x04 0x0009 2>/dev/null \
                    | grep "HCI Event" -A1 | tail -1 | awk '{
                        printf "%s:%s:%s:%s:%s:%s", $10, $9, $8, $7, $6, $5
                    }') || bd_addr=""
                if [ -n "$bd_addr" ]; then
                    pass "BLE BD Address" "$bd_addr"
                fi
            else
                fail "HCI transport (Read BD ADDR)" "No response from ${target_hci} within 5s — transport dead"
            fi

            if timeout 5 nsenter --net="${HOST_NETNS}" \
               hcitool -i "${target_hci}" cmd 0x04 0x0001 >/dev/null 2>&1; then
                pass "HCI transport (Read Version)" "Responsive"
            else
                warn "HCI transport (Read Version)" "No response (unusual)"
            fi

            local name_result
            name_result=$(timeout 3 nsenter --net="${HOST_NETNS}" \
                hciconfig "${target_hci}" name 2>&1) || true
            if echo "$name_result" | grep -qi "error\|can't read"; then
                pass "Read Local Name (known unsupported)" "Correctly rejected by firmware — do NOT use for health checks"
            else
                pass "Read Local Name" "Supported (unexpected for CPC firmware)"
            fi
        else
            fail "HCI transport" "${target_hci} is not UP — btattach may have died"
        fi
    fi
}

###############################################################################
# Section 6: BLE Scanning Capability
###############################################################################
check_ble_scan() {
    section "BLE Scanning"

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
        fail "wpan0 interface" "Not found — otbr-agent may not be running or CPC link is down"
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

    # ep14 EAGAIN state check
    local bridge_pid
    bridge_pid=$(pgrep -f "bt_host_cpc_hci_bridge" 2>/dev/null | head -1) || true
    if [ -z "$bridge_pid" ]; then
        warn "ep14 EAGAIN check" "Bridge not running — if it keeps failing, restart cpcd"
    else
        pass "ep14 EAGAIN check" "Bridge alive (PID $bridge_pid)"
    fi

    # Multiple HCI adapters
    local hci_count
    hci_count=$(ls /sys/class/bluetooth/ 2>/dev/null | wc -w) || hci_count=0
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

    # Dangerous health check command
    if grep -rq "hciconfig.*name" /entrypoint.sh 2>/dev/null; then
        fail "Health check command" "entrypoint.sh uses 'hciconfig name' — this ALWAYS fails on CPC firmware (use hcitool cmd 0x04 0x0009)"
    else
        pass "Health check command" "Not using unsupported 'hciconfig name'"
    fi

    # Dangerous HCI Reset
    if grep -q "hcitool.*cmd.*0x03.*0x0003" /entrypoint.sh 2>/dev/null; then
        warn "HCI Reset in entrypoint" "Found HCI Reset command — this can kill the CPC transport"
    else
        pass "No HCI Reset in entrypoint" "Dangerous reset sequence not present"
    fi

    # kill-0 bug
    if grep -q 'BTATTACH_PID=0' /entrypoint.sh 2>/dev/null; then
        fail "BTATTACH_PID init" "Initialized to 0 — 'kill 0' will SIGTERM the entire process group!"
    else
        pass "BTATTACH_PID init" "Not initialized to 0 (safe)"
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

# Detect radio mode for header
RADIO_MODE_DISPLAY="Unknown"
if [ -n "${RADIO_PORT}" ]; then
    RADIO_MODE_DISPLAY="Remote serial tunnel (${RADIO_HOST}:${RADIO_PORT})"
elif [ -n "${RADIO_DEVICE}" ]; then
    RADIO_MODE_DISPLAY="Local USB (${RADIO_DEVICE})"
else
    RADIO_MODE_DISPLAY="Not configured"
fi

if ! $JSON_MODE; then
    echo "╔══════════════════════════════════════════════════════════════╗"
    echo "║         Radio Stack Validation                             ║"
    echo "╠══════════════════════════════════════════════════════════════╣"
    printf "║  Instance: %-49s║\n" "${CPC_INSTANCE}"
    printf "║  Radio:    %-49s║\n" "${RADIO_MODE_DISPLAY}"
    printf "║  Date:     %-49s║\n" "$(date -Iseconds)"
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
