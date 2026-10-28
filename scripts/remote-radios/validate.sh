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
#     the kernel btusb driver registers and bluetoothd manages.  This radio is
#     optional — Thread /
#     Zigbee works without it.
#
# Run this inside the Barton or remote-radios container to diagnose the
# Silabs serial link, the Bluetooth dongle chain, and runtime issues.
#
# Usage:
#   ./validate.sh              # Run all checks
#   ./validate.sh --json       # Output results as JSON (for automation)
#   ./validate.sh --quick      # Skip the slow checks (BLE scan, MeshCoP lookup)
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
# This script must run inside the remote-radios container.  Run from the dev
# server host it finds that container and re-execs there automatically.
#
# It cannot do so from the Barton devcontainer: that container has neither the
# Docker CLI nor the Docker socket, so it has no way to reach the radio
# container.  Run the validator from the host instead.
###############################################################################
if [ ! -f /entrypoint.sh ] || ! grep -q "remote-radios" /entrypoint.sh 2>/dev/null; then
    # Not inside the remote-radios container.  Find this user's own container
    # and re-exec there.
    #
    # Multiple users share the same Docker host.  Each user's radios run in a
    # Compose project named "<user>-remote-radios" (see dockerw).  Only that
    # project is ever searched: there is deliberately no broader fallback, so
    # this script can never exec into another developer's radio container.
    QUIET=false
    for arg in "$@"; do
        [ "$arg" = "--json" ] && QUIET=true
    done

    RADIOS_USER="${BUILDER_USER:-${USER:-$(id -un 2>/dev/null || true)}}"
    if [ -z "$RADIOS_USER" ]; then
        echo "ERROR: Could not determine your username (set BUILDER_USER or USER)." >&2
        exit 2
    fi
    RADIOS_PROJECT="${RADIOS_USER}-remote-radios"
    OTBR_CONTAINER=""

    # Determine docker command (with or without sudo).  Fall back to the
    # Docker Engine API over the socket when the CLI is unavailable.
    DOCKER_CMD=""
    USE_CURL_API=false
    if command -v docker >/dev/null 2>&1 && docker ps >/dev/null 2>&1; then
        DOCKER_CMD="docker"
    elif command -v docker >/dev/null 2>&1 && command -v sudo >/dev/null 2>&1 && sudo -n docker ps >/dev/null 2>&1; then
        DOCKER_CMD="sudo -n docker"
    elif [ -S /var/run/docker.sock ]; then
        USE_CURL_API=true
    fi

    if [ "$USE_CURL_API" = true ]; then
        _DOCKER_API="http://localhost/v1.45"
        _CURL="curl -s --unix-socket /var/run/docker.sock"
        if ! $_CURL "$_DOCKER_API/_ping" >/dev/null 2>&1; then
            _CURL="sudo -n curl -s --unix-socket /var/run/docker.sock"
        fi

        _filter=$(python3 -c "
import json, sys, urllib.parse
print(urllib.parse.quote(json.dumps(
    {'label': ['com.docker.compose.project=' + sys.argv[1]],
     'name': ['remote-radios']})))
" "$RADIOS_PROJECT")
        OTBR_CONTAINER=$($_CURL "$_DOCKER_API/containers/json?filters=${_filter}" 2>/dev/null \
            | python3 -c "import json,sys; cs=json.load(sys.stdin); print(cs[0]['Names'][0].lstrip('/') if cs else '')" 2>/dev/null) || true

        if [ -n "$OTBR_CONTAINER" ]; then
            if ! $QUIET; then
                echo "Not in remote-radios container — re-executing inside ${OTBR_CONTAINER}..."
                # The container cannot see the lease; say which workstation is forwarding,
                # since the radios checked below are that workstation's.
                _owner="$(sed -n 's/^host=//p' "${HOME}/.remote-radios/owner" 2>/dev/null)"
                [ -n "$_owner" ] && echo "Radios are forwarded from: ${_owner}"
                echo ""
            fi
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
            _EXIT=$($_CURL "$_DOCKER_API/exec/${_EXEC_ID}/json" \
                | python3 -c "import json,sys; print(json.load(sys.stdin).get('ExitCode',1))")
            exit "${_EXIT}"
        fi
    elif [ -n "$DOCKER_CMD" ]; then
        OTBR_CONTAINER=$($DOCKER_CMD ps \
            --filter "label=com.docker.compose.project=${RADIOS_PROJECT}" \
            --filter "name=remote-radios" \
            --format '{{.Names}}' 2>/dev/null | head -1) || true
    fi

    if [ -n "$OTBR_CONTAINER" ]; then
        if ! $QUIET; then
            echo "Not in remote-radios container — re-executing inside ${OTBR_CONTAINER}..."
            # The container cannot see the lease; say which workstation is forwarding,
            # since the radios checked below are that workstation's.
            _owner="$(sed -n 's/^host=//p' "${HOME}/.remote-radios/owner" 2>/dev/null)"
            [ -n "$_owner" ] && echo "Radios are forwarded from: ${_owner}"
            echo ""
        fi
        # Pass the script via stdin and forward arguments.
        exec $DOCKER_CMD exec -i "$OTBR_CONTAINER" bash -s -- "$@" < "$0"
    else
        echo "ERROR: Not inside the remote-radios container and could not find" >&2
        echo "       your radio container (Compose project '${RADIOS_PROJECT}')." >&2
        echo "       Start it with ./dockerw -T, then run this from the Docker host," >&2
        echo "       or inside the container itself:" >&2
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
# Same precedence as the entrypoint: an explicit local device wins.
[ -n "${SILABS_DEVICE}" ] && SILABS_SOCKET=""
BT_USBIP_SOCKET="${BT_USBIP_SOCKET:-}"
BT_USBIP_TCP_PORT="${BT_USBIP_TCP_PORT:-3240}"
BT_USBIP_BUSID="${BT_USBIP_BUSID:-}"
CPC_SOCKET_DIR="${CPC_SOCKET_DIR:-/dev/shm}"
CPC_SOCKET_BASE="${CPC_SOCKET_DIR}/cpcd/${CPC_INSTANCE}"
DBUS_DIR="${DBUS_DIR:-/var/run/remote-radios-dbus}"
# org.bluez is published by the dev server's own bluetoothd, not by a daemon in
# this container, so Bluetooth checks use that bus rather than the private one.
HOST_DBUS_SOCKET="${HOST_DBUS_SOCKET:-/run/host-dbus/system_bus_socket}"
BT_DBUS_ADDRESS="unix:path=${HOST_DBUS_SOCKET}"
HOST_NETNS="/run/host-netns"
VIRTUAL_TTY="/dev/ttyRadio"

JSON_MODE=false
# --quick skips the slow checks (the BLE scan and the MeshCoP lookup).  It is
# what ./dockerw -T runs before starting Barton.
QUICK_MODE=false
for arg in "$@"; do
    case "$arg" in
        --json) JSON_MODE=true ;;
        --quick) QUICK_MODE=true ;;
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

    # Privileged mode (needed for usb-ip attach, iptables, wpan0 creation)
    if ip link add dummy_priv_test type dummy 2>/dev/null; then
        ip link del dummy_priv_test 2>/dev/null
        pass "Privileged mode" "Container is privileged"
    else
        fail "Privileged mode" "Container is NOT privileged (required for usb-ip attach/iptables)"
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
    for cmd in bluetoothctl hcitool hciconfig nsenter usbip python3 socat cpcd; do
        if command -v "$cmd" >/dev/null 2>&1; then
            pass "Command: $cmd" "$(command -v "$cmd")"
        else
            fail "Command: $cmd" "Not found in PATH"
        fi
    done
}

###############################################################################
# Section 2: Shared state and D-Bus
###############################################################################
check_dbus() {
    section "Shared state and D-Bus"

    # Directory shared with the barton containers; carries ble_adapter_id.
    if [ -d "${DBUS_DIR}" ]; then
        pass "Shared state directory" "${DBUS_DIR} exists"
    else
        fail "Shared state directory" "${DBUS_DIR} not found"
    fi

    # Thread and Bluetooth both use the dev server's own system bus.
    if [ ! -S "${HOST_DBUS_SOCKET}" ]; then
        fail "Dev server D-Bus" "${HOST_DBUS_SOCKET} not mounted"
    elif ! command -v dbus-send >/dev/null 2>&1; then
        skip "Dev server D-Bus" "dbus-send not available"
    elif DBUS_SYSTEM_BUS_ADDRESS="${BT_DBUS_ADDRESS}" \
        dbus-send --system --dest=org.freedesktop.DBus --print-reply \
            /org/freedesktop/DBus org.freedesktop.DBus.ListNames >/dev/null 2>&1; then
        pass "Dev server D-Bus" "Can list bus names on ${HOST_DBUS_SOCKET}"
    else
        fail "Dev server D-Bus" "Cannot communicate with ${HOST_DBUS_SOCKET}"
    fi
}

###############################################################################
# Section 3: Silabs Radio (Zigbee/Thread) Connection
###############################################################################
check_radio() {
    section "Silabs Radio (Zigbee/Thread) Connection"

    local relay="${RADIO_RELAY_SOCKET:-/run/remote-radios/cpc/relay.sock}"
    if [ -n "${SILABS_SOCKET}" ] && [ -S "${relay}" ]; then
        #----------------------------------------------------------------------
        # The usual case: cpcd and zigbeed run on the workstation, and its
        # cpcd's client sockets are relayed here for otbr-agent.
        #----------------------------------------------------------------------
        pass "Silabs mode" "Radio runs on the workstation (relay ${relay})"
        local proxy_pid
        proxy_pid=$(pgrep -f "radio-relay.py proxy" 2>/dev/null | head -1) || true
        if [ -n "$proxy_pid" ]; then
            pass "Radio relay" "PID $proxy_pid"
        else
            fail "Radio relay" "radio-relay.py proxy is not running — otbr-agent cannot reach the workstation's cpcd"
        fi

    elif [ -n "${SILABS_SOCKET}" ]; then
        #----------------------------------------------------------------------
        # Remote serial-over-SSH tunnel mode (bind-mounted UNIX socket); used
        # while an hh4 guest holds the radio, when this container is released.
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

    # cpcd process (here unless it runs on the workstation)
    local cpcd_pid
    cpcd_pid=$(pgrep -x cpcd 2>/dev/null | head -1) || true
    if [ -n "${SILABS_SOCKET}" ] && [ -S "${relay}" ]; then
        :
    elif [ -n "$cpcd_pid" ]; then
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

    # bluetoothd.  It belongs to the dev server and is shared by everyone using
    # the machine, so there is no process of ours to look for — what matters is
    # that org.bluez answers on the bus we point Barton at.
    if [ ! -S "${HOST_DBUS_SOCKET}" ]; then
        fail "bluetoothd bus" "${HOST_DBUS_SOCKET} not mounted — cannot reach the dev server's bluetoothd"
    elif DBUS_SYSTEM_BUS_ADDRESS="${BT_DBUS_ADDRESS}" \
        timeout 5 dbus-send --system --dest=org.freedesktop.DBus --print-reply \
            /org/freedesktop/DBus org.freedesktop.DBus.GetNameOwner \
            string:org.bluez >/dev/null 2>&1; then
        pass "bluetoothd" "org.bluez present on the dev server's bus"
    else
        fail "bluetoothd" "org.bluez absent — on the dev server run: sudo systemctl enable --now bluetooth.service"
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
            sleep 8
            echo "scan off"
            sleep 1
            echo "quit"
        } | DBUS_SYSTEM_BUS_ADDRESS="${BT_DBUS_ADDRESS}" \
            timeout 20 nsenter --net="${HOST_NETNS}" \
            bluetoothctl --agent=NoInputNoOutput 2>&1
    ) || true

    # Order matters.  bluetoothctl echoes every command back on its prompt line,
    # so the literal text "scan on" is ALWAYS present in the output — matching
    # on it first would swallow every genuine error reported further down.
    # Check for concrete failure signatures before deciding the scan ran.
    if echo "$btctl_output" | grep -qi "not available\|not found\|No default controller"; then
        fail "BLE LE scan" \
            "Controller ${target_hci} (${bd_addr}) not available to the dev server's bluetoothd — check that the adapter is powered and that bluetooth.service is running there"
    elif echo "$btctl_output" | grep -qi "InProgress"; then
        fail "BLE LE scan" "bluetoothd reports scan InProgress — a stale scan may be stuck"
    else
        # Count distinct advertiser addresses rather than "NEW" lines.
        # bluetoothd reports a device as NEW only the first time it sees it, so
        # rescanning an already-known device yields CHG lines only.  Counting
        # NEW alone therefore reported "Found 0 device(s)" — and passed.
        local btctl_count
        btctl_count=$(echo "$btctl_output" \
            | grep -oiE "Device ([0-9A-F]{2}:){5}[0-9A-F]{2}" \
            | grep -oiE "([0-9A-F]{2}:){5}[0-9A-F]{2}" \
            | sort -u | wc -l) || btctl_count=0

        if [ "${btctl_count:-0}" -gt 0 ]; then
            pass "BLE LE scan" "Found ${btctl_count} device(s) via D-Bus on ${target_hci}"
        else
            # Zero devices is a failure, not a warning: a radio that is present
            # but deaf looks exactly like this, and that is the failure mode
            # most worth catching.  Ambient BLE traffic is plentiful in any
            # normal environment.
            fail "BLE LE scan" \
                "No BLE devices seen on ${target_hci} during the scan window — the adapter is not receiving advertisements"
        fi
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

    # This developer's Thread interface (wpan<uid> under remote-radios).
    local tif="${THREAD_INTERFACE:-wpan0}"
    if ip link show "$tif" >/dev/null 2>&1; then
        local wpan_state
        wpan_state=$(ip -br link show "$tif" 2>/dev/null | awk '{print $2}') || wpan_state="unknown"
        pass "${tif} interface" "State: ${wpan_state}"

        local v6_addrs
        v6_addrs=$(ip -6 addr show dev "$tif" scope global 2>/dev/null | grep -c "inet6") || v6_addrs=0
        if [ "$v6_addrs" -gt 0 ]; then
            pass "${tif} IPv6 addresses" "${v6_addrs} global address(es)"
        else
            warn "${tif} IPv6 addresses" "No global IPv6 — Thread network may not be formed"
        fi
    else
        warn "${tif} interface" "Not found yet — otbr-agent may still be starting (created a few seconds after cpcd is ready)"
    fi

    # The border router must be registered on the dev server's bus, where
    # Barton and Matter look for it alongside org.bluez.
    local otbr_name="io.openthread.BorderRouter.${tif}"
    if DBUS_SYSTEM_BUS_ADDRESS="${BT_DBUS_ADDRESS}" \
        timeout 5 dbus-send --system --dest=org.freedesktop.DBus --print-reply \
            /org/freedesktop/DBus org.freedesktop.DBus.GetNameOwner \
            "string:${otbr_name}" >/dev/null 2>&1; then
        pass "Border router on shared bus" "${otbr_name}"
    else
        fail "Border router on shared bus" "${otbr_name} not registered — is docker/remote-radios-host-dbus.conf installed on the dev server?"
    fi

    # mdnsd (mDNSResponder) publishes the border router's services.
    local mdnsd_pid
    mdnsd_pid=$(pgrep -x mdnsd 2>/dev/null | head -1) || true
    if [ -z "$mdnsd_pid" ]; then
        fail "mdnsd" "Not running — otbr-agent cannot publish its mDNS services"
    elif [ ! -S /var/run/mdnsd ]; then
        fail "mdnsd" "PID $mdnsd_pid but socket /var/run/mdnsd is missing"
    else
        pass "mdnsd" "PID $mdnsd_pid"
    fi

    # The border router advertises itself as _meshcop._udp once it is up;
    # commissioners (including Matter) find it this way.
    if [ "$QUICK_MODE" = true ]; then
        skip "MeshCoP advertisement" "Skipped (--quick)"
    elif command -v dns-sd >/dev/null 2>&1; then
        local meshcop
        meshcop=$(timeout 4 dns-sd -B _meshcop._udp local. 2>/dev/null \
            | awk '/ Add /{ $1=$2=$3=$4=$5=$6=""; sub(/^ +/, ""); print; exit }') || true
        if [ -n "$meshcop" ]; then
            pass "MeshCoP advertisement" "$meshcop"
        else
            warn "MeshCoP advertisement" "No _meshcop._udp service seen — the border router is not advertising yet"
        fi
    fi
}

###############################################################################
# Section: Zigbee (ZigbeeCore's remote-zigbee container)
###############################################################################
check_zigbee() {
    section "Zigbee"

    # zigbeed runs on the workstation, next to cpcd, whenever the workstation
    # runs the radio itself (relay mode).  It does not in serial mode: until
    # ZigbeeCore is set up on the dev server, or while an hh4 guest holds the
    # radio.  ZigbeeCore's own container is checked by dockerw -T, from outside
    # this container, which is not on its network.
    if [ ! -S "${RADIO_RELAY_SOCKET:-/run/remote-radios/cpc/relay.sock}" ]; then
        skip "zigbeed" "Not running — the workstation forwards the radio's serial port (ZigbeeCore not set up, or an hh4 guest holds the radio)"
        return
    fi
    pass "zigbeed" "Runs on the workstation (see remote-radios.service there)"

    # Thread and Zigbee share the multi-PAN radio, each on its own spinel
    # interface: otbr-agent on 2, zigbeed on 1.
    if pgrep -a otbr-agent 2>/dev/null | grep -q "iid=2"; then
        pass "otbr-agent spinel interface" "iid=2 (zigbeed uses 1)"
    else
        fail "otbr-agent spinel interface" "otbr-agent is not on iid=2 and will collide with zigbeed"
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
    if [ "$hci_count" -gt 1 ] && [ -z "${BT_USBIP_SOCKET}" ]; then
        warn "Multiple HCI adapters" "${hci_count} adapters present and no dongle forwarded — set BARTON_BLE_ADAPTER_ID to choose one"
    elif [ "$hci_count" -gt 1 ]; then
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
if [ "$QUICK_MODE" = true ]; then
    section "BLE Scanning"
    skip "BLE LE scan" "Skipped (--quick)"
else
    check_ble_scan
fi
check_otbr
check_zigbee
check_known_pitfalls
print_summary

if [ "$FAIL_COUNT" -gt 0 ]; then
    exit 1
fi
exit 0
