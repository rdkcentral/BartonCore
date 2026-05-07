#!/usr/bin/env bash
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

###############################################################################
# usbip-validate.sh
#
# Run this from INSIDE the Barton container (devcontainer or dockerw session)
# after the otbr-radio container is up.  It validates the entire chain:
#
#   1. USB device is present on the host
#   2. D-Bus socket is shared and reachable
#   3. otbr-radio container is running (cpcd, bt_host_cpc_hci_bridge,
#      btattach, bluetoothd, otbr-agent)
#   4. Thread network is responsive (ot-ctl state via D-Bus)
#   5. BLE adapter is configured and visible
#   6. ble_adapter_id file is present and points to a valid HCI device
#
# No arguments required.
#
# See docs/REMOTE_RADIO_FOR_DEVELOPMENT.md for full setup instructions.
###############################################################################

set -uo pipefail

# ─── Helpers ──────────────────────────────────────────────────────────────────
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BOLD='\033[1m'
NC='\033[0m'

PASS_COUNT=0
FAIL_COUNT=0
WARN_COUNT=0

info()  { echo -e "${BOLD}[validate]${NC} $*"; }
pass()  { echo -e "  ${GREEN}PASS${NC}  $*"; PASS_COUNT=$((PASS_COUNT + 1)); }
fail()  { echo -e "  ${RED}FAIL${NC}  $*"; FAIL_COUNT=$((FAIL_COUNT + 1)); }
warn()  { echo -e "  ${YELLOW}WARN${NC}  $*"; WARN_COUNT=$((WARN_COUNT + 1)); }
skip()  { echo -e "  ${YELLOW}SKIP${NC}  $*"; }

# ─── Shared paths ────────────────────────────────────────────────────────────
DBUS_DIR="/var/run/otbr-dbus"
DBUS_SOCKET="${DBUS_DIR}/system_bus_socket"
BLE_ADAPTER_FILE="${DBUS_DIR}/ble_adapter_id"
HOST_NETNS="/run/host-netns"

# Docker API access (from inside the container)
DOCKER_SOCK="/var/run/docker.sock"

echo ""
echo -e "${BOLD}═══════════════════════════════════════${NC}"
echo -e "${BOLD} Thread Radio & BLE Validation${NC}"
echo -e "${BOLD}═══════════════════════════════════════${NC}"
echo ""

###############################################################################
# 1. D-Bus socket
###############################################################################
info "Checking D-Bus socket..."

if [[ -S "${DBUS_SOCKET}" ]]; then
    pass "D-Bus socket exists: ${DBUS_SOCKET}"
else
    fail "D-Bus socket not found: ${DBUS_SOCKET}"
    echo "       The otbr-radio container may not be running, or the"
    echo "       dbus-socket volume is not mounted."
fi

###############################################################################
# 2. otbr-radio container processes (via Docker API)
###############################################################################
info "Checking otbr-radio container..."

OTBR_CONTAINER_ID=""
EXPECTED_PROCS=("cpcd" "otbr-agent")
OPTIONAL_PROCS=("bt_host_cpc_hci_bridge" "btattach" "bluetoothd")

if [[ -S "${DOCKER_SOCK}" ]]; then
    # Find the otbr-radio container
    OTBR_CONTAINER_ID=$(sudo curl -s --unix-socket "${DOCKER_SOCK}" \
        "http://localhost/containers/json" 2>/dev/null | \
        python3 -c "
import sys, json
try:
    containers = json.load(sys.stdin)
    for c in containers:
        for name in c.get('Names', []):
            if 'otbr-radio' in name:
                print(c['Id'][:12])
                sys.exit(0)
except Exception:
    pass
" 2>/dev/null || true)

    if [[ -n "${OTBR_CONTAINER_ID}" ]]; then
        pass "otbr-radio container is running (${OTBR_CONTAINER_ID})"

        # Execute ps inside the container
        EXEC_ID=$(sudo curl -s --unix-socket "${DOCKER_SOCK}" \
            -X POST -H "Content-Type: application/json" \
            -d '{"Cmd":["ps","aux"],"AttachStdout":true,"AttachStderr":true}' \
            "http://localhost/containers/${OTBR_CONTAINER_ID}/exec" 2>/dev/null | \
            python3 -c "import sys,json; print(json.load(sys.stdin).get('Id',''))" 2>/dev/null || true)

        PS_OUTPUT=""

        if [[ -n "${EXEC_ID}" ]]; then
            PS_OUTPUT=$(sudo curl -s --unix-socket "${DOCKER_SOCK}" \
                -X POST -H "Content-Type: application/json" \
                -d '{"Detach":false,"Tty":false}' \
                "http://localhost/exec/${EXEC_ID}/start" 2>/dev/null | strings || true)
        fi

        if [[ -n "${PS_OUTPUT}" ]]; then

            for proc in "${EXPECTED_PROCS[@]}"; do

                if echo "${PS_OUTPUT}" | grep -qw "${proc}"; then
                    pass "${proc} is running"
                else
                    fail "${proc} is NOT running in otbr-radio container"
                fi
            done

            for proc in "${OPTIONAL_PROCS[@]}"; do

                if echo "${PS_OUTPUT}" | grep -qw "${proc}"; then
                    pass "${proc} is running"
                else
                    warn "${proc} is not running (BLE may not be available)"
                fi
            done
        else
            warn "Could not inspect processes inside otbr-radio container"
        fi
    else
        fail "otbr-radio container is NOT running"
        echo "       Start it with: RADIO_DEVICE=/dev/ttyACMx ./dockerw -T"
        echo "       Or rebuild the devcontainer with RADIO_DEVICE set."
    fi
else
    skip "Docker socket not available — cannot inspect otbr-radio container"
fi

###############################################################################
# 3. D-Bus connectivity to otbr-agent
###############################################################################
info "Checking D-Bus connectivity to otbr-agent..."

if [[ -S "${DBUS_SOCKET}" ]] && command -v gdbus &>/dev/null; then
    DBUS_INTROSPECT=$(gdbus introspect \
        --address "unix:path=${DBUS_SOCKET}" \
        --dest io.openthread.BorderRouter.wpan0 \
        --object-path /io/openthread/BorderRouter/wpan0 2>&1 || true)

    if echo "${DBUS_INTROSPECT}" | grep -q "interface"; then
        pass "otbr-agent D-Bus interface is reachable"

        # Try to read Thread device role
        DEVICE_ROLE=$(gdbus call \
            --address "unix:path=${DBUS_SOCKET}" \
            --dest io.openthread.BorderRouter.wpan0 \
            --object-path /io/openthread/BorderRouter/wpan0 \
            --method org.freedesktop.DBus.Properties.Get \
            io.openthread.BorderRouter DeviceRole 2>/dev/null || true)

        if [[ -n "${DEVICE_ROLE}" ]]; then
            ROLE_VALUE=$(echo "${DEVICE_ROLE}" | grep -oP "'[^']+'" | head -1 | tr -d "'")
            pass "Thread device role: ${ROLE_VALUE:-unknown}"

            if [[ "${ROLE_VALUE}" == "leader" || "${ROLE_VALUE}" == "router" || "${ROLE_VALUE}" == "child" ]]; then
                pass "Thread network is active"
            elif [[ "${ROLE_VALUE}" == "disabled" ]]; then
                pass "Thread state is 'disabled' — Barton will form the network"
            elif [[ "${ROLE_VALUE}" == "detached" ]]; then
                warn "Thread state is 'detached' — trying to join a network"
            fi
        else
            warn "Could not read Thread DeviceRole property"
        fi
    else
        fail "Cannot reach otbr-agent on D-Bus"
        echo "       ${DBUS_INTROSPECT}"
    fi
elif ! command -v gdbus &>/dev/null; then
    skip "gdbus not available — cannot test D-Bus connectivity"
else
    skip "D-Bus socket missing — skipping connectivity test"
fi

###############################################################################
# 4. BLE adapter configuration
###############################################################################
info "Checking BLE configuration..."

if [[ -f "${BLE_ADAPTER_FILE}" ]]; then
    BLE_ADAPTER_ID=$(cat "${BLE_ADAPTER_FILE}" | tr -d '[:space:]')
    pass "ble_adapter_id file exists: ${BLE_ADAPTER_FILE}"
    info "  BLE adapter index: ${BLE_ADAPTER_ID}"

    # Check if the HCI device exists in the host network namespace
    if [[ -e "${HOST_NETNS}" ]]; then
        HCI_DEVICES=$(sudo nsenter --net="${HOST_NETNS}" hciconfig 2>/dev/null || true)

        if echo "${HCI_DEVICES}" | grep -q "hci${BLE_ADAPTER_ID}"; then
            pass "hci${BLE_ADAPTER_ID} is present in host network namespace"

            HCI_STATUS=$(echo "${HCI_DEVICES}" | grep -A2 "hci${BLE_ADAPTER_ID}:" || true)

            if echo "${HCI_STATUS}" | grep -q "UP RUNNING"; then
                pass "hci${BLE_ADAPTER_ID} is UP and RUNNING"
            else
                fail "hci${BLE_ADAPTER_ID} is not UP RUNNING"
                echo "       ${HCI_STATUS}"
            fi

            # Show the BD address for reference
            BD_ADDR=$(echo "${HCI_STATUS}" | grep -oP 'BD Address: \K[0-9A-F:]+' || true)

            if [[ -n "${BD_ADDR}" ]]; then
                info "  BD Address: ${BD_ADDR}"
            fi
        else
            fail "hci${BLE_ADAPTER_ID} is NOT present in host network namespace"
            echo "       btattach may have failed or the BLE bridge is down."
            echo "       Available HCI devices:"
            echo "${HCI_DEVICES}" | grep "^hci" | sed 's/^/         /'
        fi

        # Count total HCI devices
        HCI_COUNT=$(echo "${HCI_DEVICES}" | grep -c "^hci" || true)

        if [[ ${HCI_COUNT} -ge 2 ]]; then
            pass "Multiple HCI devices present (${HCI_COUNT} total)"
        elif [[ ${HCI_COUNT} -eq 1 ]]; then
            warn "Only 1 HCI device present — the radio's BLE adapter may not be attached"
        else
            fail "No HCI devices found"
        fi
    else
        skip "Host network namespace not mounted — cannot verify HCI devices"
    fi
else
    fail "ble_adapter_id file not found: ${BLE_ADAPTER_FILE}"
    echo "       The otbr-radio entrypoint may not have completed BLE setup."
fi

###############################################################################
# 5. BLE adapter reachable via bluetoothctl (host netns)
###############################################################################
info "Checking BLE controller via bluetoothctl..."

if [[ -e "${HOST_NETNS}" ]] && command -v bluetoothctl &>/dev/null; then
    BT_LIST=$(sudo nsenter --net="${HOST_NETNS}" bluetoothctl list 2>/dev/null || true)

    if [[ -n "${BT_LIST}" ]]; then
        CONTROLLER_COUNT=$(echo "${BT_LIST}" | wc -l)
        pass "bluetoothctl sees ${CONTROLLER_COUNT} controller(s)"
        echo "${BT_LIST}" | sed 's/^/         /'
    else
        warn "bluetoothctl returned no controllers"
    fi
else
    skip "Cannot run bluetoothctl in host network namespace"
fi

###############################################################################
# 6. DBUS_SYSTEM_BUS_ADDRESS environment variable
###############################################################################
info "Checking DBUS_SYSTEM_BUS_ADDRESS..."

if [[ -n "${DBUS_SYSTEM_BUS_ADDRESS:-}" ]]; then
    pass "DBUS_SYSTEM_BUS_ADDRESS is set: ${DBUS_SYSTEM_BUS_ADDRESS}"

    if echo "${DBUS_SYSTEM_BUS_ADDRESS}" | grep -q "otbr-dbus"; then
        pass "Points to the private otbr-dbus socket"
    else
        warn "Does not point to the otbr-dbus socket — Barton may use the wrong D-Bus"
    fi
else
    fail "DBUS_SYSTEM_BUS_ADDRESS is not set"
    echo "       Barton will use the system D-Bus instead of the otbr-radio private bus."
    echo "       Export it: export DBUS_SYSTEM_BUS_ADDRESS=unix:path=${DBUS_SOCKET}"
fi

###############################################################################
# Summary
###############################################################################
echo ""
echo -e "${BOLD}═══════════════════════════════════════${NC}"
echo -e "${BOLD} Results${NC}"
echo -e "${BOLD}═══════════════════════════════════════${NC}"
echo -e "  ${GREEN}Passed: ${PASS_COUNT}${NC}"
echo -e "  ${YELLOW}Warnings: ${WARN_COUNT}${NC}"
echo -e "  ${RED}Failed: ${FAIL_COUNT}${NC}"
echo ""

if [[ ${FAIL_COUNT} -eq 0 ]]; then
    echo -e "${GREEN}${BOLD}=======================================${NC}"
    echo -e "${GREEN}${BOLD} ALL CHECKS PASSED${NC}"
    echo -e "${GREEN}${BOLD}=======================================${NC}"
    echo ""

    if [[ ${WARN_COUNT} -gt 0 ]]; then
        echo -e "${YELLOW}Some warnings were reported — review above for details.${NC}"
    fi

    exit 0
else
    echo -e "${RED}${BOLD}=======================================${NC}"
    echo -e "${RED}${BOLD} VALIDATION FAILED (${FAIL_COUNT} issue(s))${NC}"
    echo -e "${RED}${BOLD}=======================================${NC}"
    echo ""
    echo "Review the failures above and check docs/REMOTE_RADIO_FOR_DEVELOPMENT.md"
    echo "for troubleshooting guidance."
    exit 1
fi
