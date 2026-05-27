#!/bin/bash
# ------------------------------ tabstop = 4 ----------------------------------
#
# CPC Remote Radio Validation Script
#
# Performs a detailed check of all requirements for the CPC remote radio
# chain to function correctly.  Run this inside the Barton or otbr-radio
# container to diagnose connectivity, BLE chain, and runtime issues.
#
# Usage:
#   ./cpc_validate.sh              # Run all checks
#   ./cpc_validate.sh --fix        # Attempt to fix common issues
#   ./cpc_validate.sh --json       # Output results as JSON (for automation)
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
    DOCKER_CMD=""
    if command -v docker >/dev/null 2>&1 && docker ps >/dev/null 2>&1; then
        DOCKER_CMD="docker"
    elif command -v sudo >/dev/null 2>&1 && sudo docker ps >/dev/null 2>&1; then
        DOCKER_CMD="sudo docker"
    fi

    if [ -n "$DOCKER_CMD" ]; then
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
CPC_REMOTE_HOST="${CPC_REMOTE_HOST:-}"
CPC_REMOTE_PORT="${CPC_REMOTE_PORT:-15000}"
CPC_SOCKET_DIR="${CPC_SOCKET_DIR:-/dev/shm}"
CPC_SOCKET_BASE="${CPC_SOCKET_DIR}/cpcd/${CPC_INSTANCE}"
DBUS_DIR="${DBUS_DIR:-/var/run/otbr-dbus}"
DBUS_SOCKET_PATH="${DBUS_SOCKET_PATH:-${DBUS_DIR}/system_bus_socket}"
HOST_NETNS="/run/host-netns"
BT_BRIDGE_DIR="/var/run/bt-hci-bridge"
HCI_PROXY_SCRIPT="/opt/cpc-proxy/hci_pty_proxy.py"
CPC_PROXY_SCRIPT="/opt/cpc-proxy/cpc_socket_proxy.py"

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

    # Required bind-mounts
    if [ -f "${CPC_PROXY_SCRIPT}" ]; then
        pass "CPC proxy script" "${CPC_PROXY_SCRIPT} present"
    else
        fail "CPC proxy script" "${CPC_PROXY_SCRIPT} not found"
    fi

    if [ -f "${HCI_PROXY_SCRIPT}" ]; then
        pass "HCI PTY proxy script" "${HCI_PROXY_SCRIPT} present"
    else
        warn "HCI PTY proxy script" "${HCI_PROXY_SCRIPT} not found — Extended Advertising workaround unavailable"
    fi

    # Required commands
    for cmd in btattach bluetoothd hcitool hciconfig nsenter bt_host_cpc_hci_bridge python3; do
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
# Section 3: CPC Connectivity
###############################################################################
check_cpc() {
    section "CPC Connectivity"

    if [ -z "${CPC_REMOTE_HOST}" ]; then
        skip "CPC remote mode" "CPC_REMOTE_HOST not set — local mode"
        # Check for local cpcd
        local cpcd_pid
        cpcd_pid=$(pgrep -x cpcd 2>/dev/null | head -1) || true
        if [ -n "$cpcd_pid" ]; then
            pass "cpcd process" "PID $cpcd_pid"
        else
            fail "cpcd process" "cpcd not running"
        fi
    else
        pass "CPC remote mode" "Remote host: ${CPC_REMOTE_HOST}:${CPC_REMOTE_PORT}"

        # TCP connectivity to remote host
        if timeout 3 bash -c "echo >/dev/tcp/${CPC_REMOTE_HOST}/${CPC_REMOTE_PORT}" 2>/dev/null; then
            pass "Remote ctrl port" "TCP ${CPC_REMOTE_HOST}:${CPC_REMOTE_PORT} reachable"
        else
            fail "Remote ctrl port" "Cannot connect to ${CPC_REMOTE_HOST}:${CPC_REMOTE_PORT}"
        fi

        local reset_port=$((CPC_REMOTE_PORT + 1))
        if timeout 3 bash -c "echo >/dev/tcp/${CPC_REMOTE_HOST}/${reset_port}" 2>/dev/null; then
            pass "Remote reset port" "TCP ${CPC_REMOTE_HOST}:${reset_port} reachable"
        else
            fail "Remote reset port" "Cannot connect to ${CPC_REMOTE_HOST}:${reset_port}"
        fi

        # Check well-known endpoint ports
        local ep14_port=$((CPC_REMOTE_PORT + 100 + 14))
        if timeout 3 bash -c "echo >/dev/tcp/${CPC_REMOTE_HOST}/${ep14_port}" 2>/dev/null; then
            pass "Remote ep14 port (BLE)" "TCP ${CPC_REMOTE_HOST}:${ep14_port} reachable"
        else
            fail "Remote ep14 port (BLE)" "Cannot connect to ${CPC_REMOTE_HOST}:${ep14_port} — BLE bridge will fail"
        fi

        local ep12_port=$((CPC_REMOTE_PORT + 100 + 12))
        if timeout 3 bash -c "echo >/dev/tcp/${CPC_REMOTE_HOST}/${ep12_port}" 2>/dev/null; then
            pass "Remote ep12 port (Thread)" "TCP ${CPC_REMOTE_HOST}:${ep12_port} reachable"
        else
            fail "Remote ep12 port (Thread)" "Cannot connect to ${CPC_REMOTE_HOST}:${ep12_port} — otbr-agent will fail"
        fi
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

    # CPC proxy processes (remote mode)
    if [ -n "${CPC_REMOTE_HOST}" ]; then
        local proxy_count
        proxy_count=$(pgrep -cf "cpc_socket_proxy.py client" 2>/dev/null) || proxy_count=0
        if [ "$proxy_count" -ge 6 ]; then
            pass "CPC proxy processes" "${proxy_count} running (expected 6: ctrl, reset, ep12/ep14 data+event)"
        elif [ "$proxy_count" -gt 0 ]; then
            warn "CPC proxy processes" "Only ${proxy_count} running (expected 6)"
        else
            fail "CPC proxy processes" "No CPC proxy client processes running"
        fi

        # Check for zombie proxies
        local zombie_count=0
        for pid in $(pgrep -f "cpc_socket_proxy.py client" 2>/dev/null); do
            local state
            state=$(awk '/^State:/ {print $2}' /proc/"$pid"/status 2>/dev/null) || continue
            if [[ "$state" == "Z" ]]; then
                zombie_count=$((zombie_count + 1))
            fi
        done
        if [ "$zombie_count" -gt 0 ]; then
            fail "CPC proxy zombies" "${zombie_count} zombie proxy process(es)"
        else
            pass "CPC proxy zombies" "None"
        fi
    fi
}

###############################################################################
# Section 4: BLE Chain
###############################################################################
check_ble_chain() {
    section "BLE Chain (bridge → pty_proxy → btattach → bluetoothd)"

    # bt_host_cpc_hci_bridge
    # NOTE: pgrep -x can't match this — Linux /proc/PID/comm truncates to
    # 15 chars ("bt_host_cpc_hci").  Use pgrep -f for the full command line.
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

    # BLE monitor — runs as a backgrounded bash function inside the
    # entrypoint script, so it appears as a child /bin/bash process of PID 1.
    # We detect it by looking for child bash processes whose /proc/PID/cmdline
    # contains "/entrypoint.sh" (i.e. subshells forked from the entrypoint).
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
        local bus
        bus=$(cat "/sys/class/bluetooth/${hci}/device/subsystem" 2>/dev/null | xargs basename 2>/dev/null) || bus="unknown"
        # Try to get the vendor from hciconfig or sysfs
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
                # Non-CPC adapter that's not selected — just informational
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
    # This was the root cause of false health-check failures that cascaded
    # into bridge restarts and ep14 EAGAIN lockups.
    if [ -n "$expected_idx" ]; then
        local target_hci="hci${expected_idx}"
        if nsenter --net="${HOST_NETNS}" hciconfig "${target_hci}" 2>/dev/null | grep -q "UP RUNNING"; then
            if timeout 5 nsenter --net="${HOST_NETNS}" \
               hcitool -i "${target_hci}" cmd 0x04 0x0009 >/dev/null 2>&1; then
                pass "HCI transport (Read BD ADDR)" "Responsive on ${target_hci}"

                # Extract the BD address from the response for display
                local bd_addr
                bd_addr=$(timeout 5 nsenter --net="${HOST_NETNS}" \
                    hcitool -i "${target_hci}" cmd 0x04 0x0009 2>/dev/null \
                    | grep "HCI Event" -A1 | tail -1 | awk '{
                        # Response: status(1) + addr(6 bytes LE)
                        # Skip first 4 bytes (evt fields), addr starts at byte 5
                        printf "%s:%s:%s:%s:%s:%s", $10, $9, $8, $7, $6, $5
                    }') || bd_addr=""
                if [ -n "$bd_addr" ]; then
                    pass "BLE BD Address" "$bd_addr"
                fi
            else
                fail "HCI transport (Read BD ADDR)" "No response from ${target_hci} within 5s — transport dead"
            fi

            # Verify Read Local Version works (basic command, widely supported)
            if timeout 5 nsenter --net="${HOST_NETNS}" \
               hcitool -i "${target_hci}" cmd 0x04 0x0001 >/dev/null 2>&1; then
                pass "HCI transport (Read Version)" "Responsive"
            else
                warn "HCI transport (Read Version)" "No response (unusual)"
            fi

            # Explicitly check Read Local Name is unsupported (known CPC firmware limitation)
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
    #
    # IMPORTANT: Do NOT use raw hcitool lescan here.  Raw HCI scanning
    # bypasses bluetoothd and corrupts its internal state machine.  When the
    # Matter SDK later tries to start BLE discovery via D-Bus, bluetoothd
    # rejects it with "org.bluez.Error.InProgress" because it doesn't know
    # the raw scan stopped.  This blocks commissioning entirely.
    #
    # bluetoothctl goes through D-Bus → bluetoothd, keeping state consistent.
    # We must explicitly select the CPC adapter because bluetoothd may
    # default to a host USB adapter (e.g. hci0).
    local bd_addr
    bd_addr=$(nsenter --net="${HOST_NETNS}" hciconfig "${target_hci}" 2>/dev/null \
        | awk '/BD Address:/{print $3}') || bd_addr=""

    if [ -z "$bd_addr" ]; then
        fail "BLE LE scan" "Cannot determine BD address for ${target_hci}"
        return
    fi

    # Start scan, collect output for a few seconds, then stop cleanly.
    #
    # CRITICAL: Use a SINGLE bluetoothctl session for both scan on and
    # scan off.  Using two separate instances creates a D-Bus session
    # mismatch — the second instance's "scan off" is ignored because
    # bluetoothd tracks discovery per-client and the second client never
    # started one.  The first client's scan persists until bluetoothd's
    # auto-cleanup fires (which may be delayed or incomplete), leaving
    # the adapter in Discovering=yes and causing "InProgress" errors
    # for the Matter SDK.
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

            # Check how long it's been running (stability indicator)
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

        # Check for IPv6 addresses on wpan0 (Thread mesh connectivity)
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

    # Check avahi-daemon (required by otbr-agent with OTBR_MDNS=avahi)
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

    # Check for stale PID files or sockets from previous runs
    if [ -S "${CPC_SOCKET_BASE}/ctrl.cpcd.sock" ]; then
        # Verify the socket is actually connected (not stale)
        local ctrl_established
        ctrl_established=$(ss -x 2>/dev/null | grep -c "ctrl.cpcd.sock" 2>/dev/null) || ctrl_established=0
        if [ "$ctrl_established" -gt 0 ]; then
            pass "ctrl.cpcd.sock liveness" "${ctrl_established} established connection(s)"
        else
            warn "ctrl.cpcd.sock liveness" "Socket file exists but no established connections — may be stale"
        fi
    fi

    # Check for ep14 EAGAIN state: if the bridge is repeatedly failing to open
    # the endpoint, cpcd needs to be restarted on the server side.
    local bridge_pid
    bridge_pid=$(pgrep -f "bt_host_cpc_hci_bridge" 2>/dev/null | head -1) || true
    if [ -z "$bridge_pid" ]; then
        warn "ep14 EAGAIN check" "Bridge not running — if it keeps failing, restart cpcd on the server"
    else
        pass "ep14 EAGAIN check" "Bridge alive (PID $bridge_pid)"
    fi

    # Check for multiple HCI adapters (can confuse bluetoothd default adapter selection)
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

    # Check if there's a health check using the wrong HCI command
    # (hciconfig name / Read Local Name is unsupported by CPC firmware)
    if grep -rq "hciconfig.*name" /entrypoint.sh 2>/dev/null; then
        fail "Health check command" "entrypoint.sh uses 'hciconfig name' — this ALWAYS fails on CPC firmware (use hcitool cmd 0x04 0x0009)"
    else
        pass "Health check command" "Not using unsupported 'hciconfig name'"
    fi

    # Check if entrypoint has the dangerous HCI Reset sequence
    if grep -q "hcitool.*cmd.*0x03.*0x0003" /entrypoint.sh 2>/dev/null; then
        warn "HCI Reset in entrypoint" "Found HCI Reset command — this can kill the CPC transport"
    else
        pass "No HCI Reset in entrypoint" "Dangerous reset sequence not present"
    fi

    # Check for the kill-0 bug (BTATTACH_PID=0 initialization)
    if grep -q 'BTATTACH_PID=0' /entrypoint.sh 2>/dev/null; then
        fail "BTATTACH_PID init" "Initialized to 0 — 'kill 0' will SIGTERM the entire process group!"
    else
        pass "BTATTACH_PID init" "Not initialized to 0 (safe)"
    fi

    # Check for raw HCI scan commands (hcitool lescan) in scripts.
    # These bypass bluetoothd and corrupt its state machine, causing
    # "org.bluez.Error.InProgress" when the Matter SDK tries to scan.
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
if ! $JSON_MODE; then
    echo "╔══════════════════════════════════════════════════════════════╗"
    echo "║         CPC Remote Radio Validation                        ║"
    echo "╠══════════════════════════════════════════════════════════════╣"
    printf "║  Instance: %-49s║\n" "${CPC_INSTANCE}"
    if [ -n "${CPC_REMOTE_HOST}" ]; then
        printf "║  Remote:   %-49s║\n" "${CPC_REMOTE_HOST}:${CPC_REMOTE_PORT}"
    else
        printf "║  Mode:     %-49s║\n" "Local radio"
    fi
    printf "║  Date:     %-49s║\n" "$(date -Iseconds)"
    echo "╚══════════════════════════════════════════════════════════════╝"
fi

check_container_env
check_dbus
check_cpc
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
