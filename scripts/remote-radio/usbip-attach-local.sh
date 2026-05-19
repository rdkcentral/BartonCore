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
# usbip-attach-local.sh <remote-host>
#
# Run this on the LOCAL WORKSTATION (laptop) where the BRD2703 xG24 radio is
# physically plugged in via USB.  It:
#
#   1. Ensures required packages and kernel modules are loaded.
#   2. Auto-detects the Silicon Labs radio by USB vendor/product ID.
#   3. Binds the device to USB-IP.
#   4. Starts the USB-IP daemon if not already running.
#   5. Opens an SSH reverse tunnel to the remote server using a per-user
#      loopback address (127.0.<hi>.<lo> from remote UID) so multiple
#      developers can share a server without port conflicts.
#      Requires GatewayPorts clientspecified in sshd_config on the server.
#
# Usage:
#   scripts/usbip-attach-local.sh user@remote-server
#   scripts/usbip-attach-local.sh remote-server    # uses current username
#
# Leave the terminal open while you work.  Press Ctrl-C to close the tunnel.
# Run scripts/usbip-detach-local.sh to clean up.
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
STATE_FILE="/tmp/.usbip-radio-local-$(id -u)"

# ─── Helpers ──────────────────────────────────────────────────────────────────
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BOLD='\033[1m'
NC='\033[0m'

info()  { echo -e "${BOLD}[usbip-local]${NC} $*"; }
ok()    { echo -e "${GREEN}[usbip-local] OK:${NC} $*"; }
warn()  { echo -e "${YELLOW}[usbip-local] WARNING:${NC} $*" >&2; }
fail()  { echo -e "${RED}[usbip-local] ERROR:${NC} $*" >&2; }

die() {
    fail "$@"
    echo ""
    echo -e "${RED}[usbip-local] FAILED — see errors above.${NC}"
    exit 1
}

# ─── Parse arguments ─────────────────────────────────────────────────────────
if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <[user@]remote-host>"
    echo ""
    echo "  remote-host    The SSH host where Docker and the devcontainer run."
    echo "                 Prefix with user@ if your remote username differs."
    echo ""
    echo "Examples:"
    echo "  $0 devserver.example.com"
    echo "  $0 jdoe@devserver.example.com"
    exit 1
fi

SSH_TARGET="$1"

# Extract user and host for display.  If no user@ prefix, use current user.
if [[ "${SSH_TARGET}" == *@* ]]; then
    REMOTE_USER="${SSH_TARGET%%@*}"
    REMOTE_HOST="${SSH_TARGET#*@}"
else
    REMOTE_USER="$(whoami)"
    REMOTE_HOST="${SSH_TARGET}"
    SSH_TARGET="${REMOTE_USER}@${REMOTE_HOST}"
fi

# ─── Pre-flight checks ───────────────────────────────────────────────────────
info "Checking prerequisites..."

if [[ "$(uname -s)" != "Linux" ]]; then
    die "This script only works on Linux."
fi

if ! command -v usbip &>/dev/null; then
    info "usbip not found — installing..."
    sudo apt-get update -qq
    sudo apt-get install -y -qq linux-tools-generic linux-cloud-tools-generic
fi

if ! command -v usbip &>/dev/null; then
    die "usbip still not found after install.  Check your kernel version and packages."
fi

ok "usbip is available."

# ─── Resolve the remote UID for per-user loopback address ─────────────────────
info "Resolving remote UID for ${SSH_TARGET}..."
REMOTE_UID=$(ssh -o ConnectTimeout=10 "${SSH_TARGET}" "id -u" 2>/dev/null) || \
    die "Cannot SSH to ${SSH_TARGET}.  Check connectivity and credentials."

if ! [[ "${REMOTE_UID}" =~ ^[0-9]+$ ]]; then
    die "Got invalid UID '${REMOTE_UID}' from remote host."
fi

# Each user gets a unique loopback address so multiple developers can share
# a server.  Both usbip list and usbip attach connect to this address on the
# standard port 3240.  Requires 'GatewayPorts clientspecified' in sshd_config.
#   UID 1000 → 127.0.3.232, UID 1001 → 127.0.3.233, etc.
USBIP_LOOPBACK="127.0.$(( (REMOTE_UID >> 8) & 255 )).$(( REMOTE_UID & 255 ))"
ok "Remote UID: ${REMOTE_UID}, tunnel: ${USBIP_LOOPBACK}:${USBIP_PORT}"

# ─── Verify kernel modules ────────────────────────────────────────────────────
info "Checking required kernel modules..."

MISSING_MODULES=()

for mod in usbip_core usbip_host vhci_hcd; do

    if ! grep -qw "${mod}" /proc/modules 2>/dev/null; then
        # Try to load it (may fail without sudo privileges)
        sudo modprobe "${mod}" 2>/dev/null || true

        if ! grep -qw "${mod}" /proc/modules 2>/dev/null; then
            MISSING_MODULES+=("${mod}")
        fi
    fi
done

if [[ ${#MISSING_MODULES[@]} -gt 0 ]]; then
    fail "The following required kernel modules are not loaded:"
    echo ""

    for mod in "${MISSING_MODULES[@]}"; do
        echo -e "    ${RED}•${NC} ${BOLD}${mod}${NC}"
    done

    echo ""
    echo -e "  Please load them with:"
    echo ""

    for mod in "${MISSING_MODULES[@]}"; do
        echo -e "    ${BOLD}sudo modprobe ${mod}${NC}"
    done

    echo ""
    die "Required kernel modules are not loaded."
fi

ok "Kernel modules loaded."

# ─── Find the BRD2703 radio ───────────────────────────────────────────────────
info "Searching for BRD2703 radio (${RADIO_IDS[*]})..."

RADIO_BUSID=""
MATCHED_ID=""

while IFS= read -r line; do

    for rid in "${RADIO_IDS[@]}"; do

        if echo "${line}" | grep -qi "${rid}"; then
            RADIO_BUSID=$(echo "${line}" | grep -oP 'busid\s+\K[0-9.-]+' || echo "${line}" | awk '{print $3}')
            MATCHED_ID="${rid}"
            break 2
        fi
    done
done < <(usbip list -l 2>/dev/null)

# Fallback: scan sysfs directly
if [[ -z "${RADIO_BUSID}" ]]; then

    for dev in /sys/bus/usb/devices/*/idVendor; do
        devDir=$(dirname "${dev}")
        vid=$(cat "${devDir}/idVendor" 2>/dev/null || true)
        pid=$(cat "${devDir}/idProduct" 2>/dev/null || true)

        for rid in "${RADIO_IDS[@]}"; do
            IFS=: read -r rVid rPid <<< "${rid}"

            if [[ "${vid}" == "${rVid}" && "${pid}" == "${rPid}" ]]; then
                RADIO_BUSID=$(basename "${devDir}")
                MATCHED_ID="${rid}"
                break 2
            fi
        done
    done
fi

if [[ -z "${RADIO_BUSID}" ]]; then
    die "No BRD2703 radio found.  Is it plugged in via USB?" \
        "Looked for USB IDs: ${RADIO_IDS[*]}"
fi

ok "Found radio at bus ID: ${RADIO_BUSID} (${MATCHED_ID})"

# ─── Bind the device ─────────────────────────────────────────────────────────
info "Binding device ${RADIO_BUSID} to USB-IP..."

# Unbind first in case it's already bound (idempotent).
sudo usbip unbind -b "${RADIO_BUSID}" 2>/dev/null || true
sudo usbip bind -b "${RADIO_BUSID}"

if ! usbip list -l 2>/dev/null | grep -q "${RADIO_BUSID}"; then
    die "Device ${RADIO_BUSID} does not appear in 'usbip list -l' after binding."
fi

ok "Device bound."

# ─── Start the USB-IP daemon ─────────────────────────────────────────────────
if ss -tlnp 2>/dev/null | grep -q ":${USBIP_PORT}\b" || \
   netstat -tlnp 2>/dev/null | grep -q ":${USBIP_PORT}\b"; then
    ok "usbipd already listening on port ${USBIP_PORT}."
else
    info "Starting usbipd on port ${USBIP_PORT}..."
    sudo usbipd -D
    sleep 1

    if ss -tlnp 2>/dev/null | grep -q ":${USBIP_PORT}\b" || \
       netstat -tlnp 2>/dev/null | grep -q ":${USBIP_PORT}\b"; then
        ok "usbipd started."
    else
        die "usbipd failed to start on port ${USBIP_PORT}."
    fi
fi

# ─── Save state for teardown ─────────────────────────────────────────────────
cat > "${STATE_FILE}" <<EOF
RADIO_BUSID=${RADIO_BUSID}
SSH_TARGET=${SSH_TARGET}
USBIP_LOOPBACK=${USBIP_LOOPBACK}
USBIP_PORT=${USBIP_PORT}
EOF
ok "State saved to ${STATE_FILE}"

# ─── Open SSH reverse tunnel ─────────────────────────────────────────────────
echo ""
info "Opening SSH reverse tunnel:"
info "  ${USBIP_LOOPBACK}:${USBIP_PORT} on remote → localhost:${USBIP_PORT}"
info "Keep this terminal open while you work."
info "When done, press Ctrl-C or run ${BOLD}scripts/remote-radio/usbip-detach-local.sh${NC}"
echo ""
echo -e "${GREEN}${BOLD}=======================================${NC}"
echo -e "${GREEN}${BOLD} LOCAL SETUP COMPLETE${NC}"
echo -e "${GREEN}${BOLD}=======================================${NC}"
echo -e "${GREEN} Radio:       ${RADIO_BUSID} (${MATCHED_ID})${NC}"
echo -e "${GREEN} Tunnel:      localhost:${USBIP_PORT} → ${USBIP_LOOPBACK}:${USBIP_PORT}${NC}"
echo -e "${GREEN} Remote user: ${REMOTE_USER} (UID ${REMOTE_UID})${NC}"
echo -e "${GREEN}${BOLD}=======================================${NC}"
echo ""
info "On the remote server, run:"
echo ""
echo -e "    ${BOLD}scripts/remote-radio/usbip-attach-remote.sh${NC}"
echo ""

exec ssh -R "${USBIP_LOOPBACK}:${USBIP_PORT}:localhost:${USBIP_PORT}" "${SSH_TARGET}"
