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
# usbip-attach-remote.sh
#
# Run this on the REMOTE DEV SERVER where Docker and the devcontainer run.
# It expects that the local workstation has already run usbip-attach-local.sh
# and has an SSH reverse tunnel forwarding to a per-user loopback address.
#
# This script:
#
#   1. Ensures required packages and kernel modules are loaded.
#   2. Computes the per-user loopback address (127.0.<hi>.<lo> from UID).
#   3. Verifies the USB-IP tunnel is reachable.
#   4. Lists available devices and auto-detects the BRD2703 radio.
#   5. Detaches any stale attachment belonging to this user only.
#   6. Attaches the remote USB device.
#   7. Waits for the /dev/ttyACM* device to appear.
#   8. Saves state so usbip-detach-remote.sh can clean up safely.
#
# No arguments required.
#
# Requires 'GatewayPorts clientspecified' in sshd_config on this server.
#
# See docs/REMOTE_RADIO_FOR_DEVELOPMENT.md for full setup instructions.
###############################################################################

set -euo pipefail

# ─── Constants ────────────────────────────────────────────────────────────────
SILABS_VID="10c4"
SILABS_PID="ea60"
SEGGER_VID="1366"
SEGGER_PID="0105"
RADIO_IDS=("${SILABS_VID}:${SILABS_PID}" "${SEGGER_VID}:${SEGGER_PID}")
USBIP_PORT=3240
# Each user gets a unique loopback address so multiple developers can share
# a server.  Matches the address used by usbip-attach-local.sh.
#   UID 1000 → 127.0.3.232, UID 1001 → 127.0.3.233, etc.
USBIP_LOOPBACK="127.0.$(( ($(id -u) >> 8) & 255 )).$(( $(id -u) & 255 ))"
STATE_FILE="/tmp/.usbip-radio-remote-$(id -u)"

# ─── Helpers ──────────────────────────────────────────────────────────────────
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BOLD='\033[1m'
NC='\033[0m'

info()  { echo -e "${BOLD}[usbip-remote]${NC} $*"; }
ok()    { echo -e "${GREEN}[usbip-remote] OK:${NC} $*"; }
warn()  { echo -e "${YELLOW}[usbip-remote] WARNING:${NC} $*" >&2; }
fail()  { echo -e "${RED}[usbip-remote] ERROR:${NC} $*" >&2; }

die() {
    fail "$@"
    echo ""
    echo -e "${RED}[usbip-remote] FAILED — see errors above.${NC}"
    exit 1
}

# ─── Pre-flight checks ───────────────────────────────────────────────────────
info "Checking prerequisites..."
info "Per-user loopback: ${USBIP_LOOPBACK}:${USBIP_PORT} (UID $(id -u))"

if [[ "$(uname -s)" != "Linux" ]]; then
    die "This script only works on Linux."
fi

if ! command -v usbip &>/dev/null; then
    info "usbip not found — installing..."
    sudo apt-get update -qq
    sudo apt-get install -y -qq linux-tools-generic hwdata
fi

if ! command -v usbip &>/dev/null; then
    die "usbip still not found after install."
fi

ok "usbip is available."

# ─── Verify kernel modules ────────────────────────────────────────────────────
info "Checking required kernel modules..."

MISSING_MODULES=()

for mod in vhci_hcd usbip_core; do

    if ! grep -qw "${mod}" /proc/modules 2>/dev/null; then
        MISSING_MODULES+=("${mod}")
    fi
done

if [[ ${#MISSING_MODULES[@]} -gt 0 ]]; then
    fail "The following required kernel modules are not loaded:"
    echo ""

    for mod in "${MISSING_MODULES[@]}"; do
        echo -e "    ${RED}•${NC} ${BOLD}${mod}${NC}"
    done

    echo ""
    echo -e "  These modules must be loaded by a system administrator."
    echo -e "  Please ask an administrator to run:"
    echo ""

    for mod in "${MISSING_MODULES[@]}"; do
        echo -e "    ${BOLD}sudo modprobe ${mod}${NC}"
    done

    echo ""
    echo -e "  To make this persistent across reboots, ask them to add the"
    echo -e "  module names to ${BOLD}/etc/modules-load.d/usbip.conf${NC}."
    die "Required kernel modules are not loaded."
fi

ok "Kernel modules loaded (vhci_hcd, usbip_core)."

# ─── Verify tunnel is up ─────────────────────────────────────────────────────
info "Checking USB-IP tunnel on ${USBIP_LOOPBACK}:${USBIP_PORT}..."

if ! ss -tlnp 2>/dev/null | grep -q "${USBIP_LOOPBACK}:${USBIP_PORT}" && \
   ! netstat -tlnp 2>/dev/null | grep -q "${USBIP_LOOPBACK}:${USBIP_PORT}"; then
    die "Nothing is listening on ${USBIP_LOOPBACK}:${USBIP_PORT}." \
        "Make sure usbip-attach-local.sh is running on your workstation" \
        "with an SSH reverse tunnel open." \
        "The remote sshd must have 'GatewayPorts clientspecified' enabled."
fi

ok "Tunnel is reachable at ${USBIP_LOOPBACK}:${USBIP_PORT}."

# ─── List available devices ──────────────────────────────────────────────────
info "Listing remote USB devices..."

DEVICE_LIST=""

if ! DEVICE_LIST=$(usbip list -r "${USBIP_LOOPBACK}" 2>&1); then
    fail "usbip list failed with output:"
    echo "${DEVICE_LIST}" | head -20
    die "Failed to list remote devices. Is the tunnel open?"
fi

echo "${DEVICE_LIST}" | head -20
echo ""

# ─── Find the BRD2703 radio ──────────────────────────────────────────────────
info "Looking for BRD2703 radio (${RADIO_IDS[*]})..."

RADIO_BUSID=""
MATCHED_ID=""

while IFS= read -r line; do

    for rid in "${RADIO_IDS[@]}"; do

        if echo "${line}" | grep -qi "${rid}"; then
            RADIO_BUSID=$(echo "${line}" | grep -oP '^\s*\K[0-9.-]+' || true)

            if [[ -z "${RADIO_BUSID}" ]]; then
                RADIO_BUSID=$(echo "${line}" | awk -F: '{print $1}' | tr -d ' ')
            fi

            MATCHED_ID="${rid}"
            break 2
        fi
    done
done <<< "${DEVICE_LIST}"

if [[ -z "${RADIO_BUSID}" ]]; then
    die "No BRD2703 radio found in the remote device list." \
        "Looked for USB IDs: ${RADIO_IDS[*]}" \
        "Check that the radio is plugged in and bound on the local workstation."
fi

ok "Found radio at bus ID: ${RADIO_BUSID} (${MATCHED_ID})"

# ─── Detach any stale attachment for THIS user ────────────────────────────────
# Only touch vhci ports recorded in our own state file.
if [[ -f "${STATE_FILE}" ]]; then
    PREV_PORT=$(grep '^VHCI_PORT=' "${STATE_FILE}" 2>/dev/null | cut -d= -f2 || true)

    if [[ -n "${PREV_PORT}" ]]; then
        info "Detaching stale USB-IP vhci port ${PREV_PORT} from previous session..."
        sudo usbip detach -p "${PREV_PORT}" 2>/dev/null || true
        sleep 1
    fi

    rm -f "${STATE_FILE}"
fi

# ─── Snapshot existing ttyACM devices ─────────────────────────────────────────
TTY_BEFORE=$(ls /dev/ttyACM* 2>/dev/null | sort || true)

# ─── Snapshot existing vhci ports ─────────────────────────────────────────────
VHCI_BEFORE=$(usbip port 2>/dev/null | grep -oP 'Port\s+\K[0-9]+' | sort || true)

# ─── Attach the device ───────────────────────────────────────────────────────
info "Attaching device ${RADIO_BUSID} via ${USBIP_LOOPBACK}..."

sudo usbip attach -r "${USBIP_LOOPBACK}" -b "${RADIO_BUSID}"

# ─── Identify the new vhci port (for clean detach later) ─────────────────────
sleep 1
VHCI_AFTER=$(usbip port 2>/dev/null | grep -oP 'Port\s+\K[0-9]+' | sort || true)
NEW_VHCI_PORT=""

for port in ${VHCI_AFTER}; do

    if ! echo "${VHCI_BEFORE}" | grep -qw "${port}"; then
        NEW_VHCI_PORT="${port}"
        break
    fi
done

# ─── Wait for the ttyACM device to appear ────────────────────────────────────
info "Waiting for /dev/ttyACM* device to appear..."

RADIO_DEVICE=""
WAIT_MAX=10
waited=0

while [[ ${waited} -lt ${WAIT_MAX} ]]; do
    sleep 1
    waited=$((waited + 1))

    TTY_AFTER=$(ls /dev/ttyACM* 2>/dev/null | sort || true)

    for tty in ${TTY_AFTER}; do

        if ! echo "${TTY_BEFORE}" | grep -qw "${tty}"; then
            RADIO_DEVICE="${tty}"
            break
        fi
    done

    if [[ -n "${RADIO_DEVICE}" ]]; then
        break
    fi
done

if [[ -z "${RADIO_DEVICE}" ]]; then
    # Fall back to the last ttyACM device if we can't diff
    RADIO_DEVICE=$(ls /dev/ttyACM* 2>/dev/null | tail -1 || true)
fi

if [[ -z "${RADIO_DEVICE}" ]]; then
    die "No /dev/ttyACM* device appeared after attaching."
fi

ok "Radio device appeared: ${RADIO_DEVICE}"

# ─── Verify the device is accessible ─────────────────────────────────────────
if [[ ! -c "${RADIO_DEVICE}" ]]; then
    die "${RADIO_DEVICE} exists but is not a character device."
fi

ok "Device is accessible."

# ─── Save state for teardown ─────────────────────────────────────────────────
cat > "${STATE_FILE}" <<EOF
RADIO_BUSID=${RADIO_BUSID}
RADIO_DEVICE=${RADIO_DEVICE}
VHCI_PORT=${NEW_VHCI_PORT:-unknown}
USBIP_LOOPBACK=${USBIP_LOOPBACK}
TIMESTAMP=$(date -Iseconds)
EOF
ok "State saved to ${STATE_FILE}"

# ─── Print final instructions ────────────────────────────────────────────────
echo ""
echo -e "${GREEN}${BOLD}=======================================${NC}"
echo -e "${GREEN}${BOLD} REMOTE ATTACH COMPLETE${NC}"
echo -e "${GREEN}${BOLD}=======================================${NC}"
echo -e "${GREEN} Radio device:  ${RADIO_DEVICE}${NC}"
echo -e "${GREEN} Tunnel:        ${USBIP_LOOPBACK}:${USBIP_PORT}${NC}"
echo -e "${GREEN} VHCI port:     ${NEW_VHCI_PORT:-unknown}${NC}"
echo -e "${GREEN}${BOLD}=======================================${NC}"
echo ""
info "To use with dockerw:"
echo ""
echo -e "    ${BOLD}RADIO_DEVICE=${RADIO_DEVICE} ./dockerw -T bash${NC}"
echo ""
info "To use with the devcontainer, add to docker/.env or export:"
echo ""
echo -e "    ${BOLD}export RADIO_DEVICE=${RADIO_DEVICE}${NC}"
echo ""
info "Then rebuild the devcontainer."
info "After containers are up, run ${BOLD}scripts/remote-radio/usbip-validate.sh${NC} to verify everything."
info "To tear down, run ${BOLD}scripts/remote-radio/usbip-detach-remote.sh${NC}"
echo ""
