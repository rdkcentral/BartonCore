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
# usbip-detach-local.sh
#
# Run this on the LOCAL WORKSTATION (laptop) to tear down the USB-IP session
# started by usbip-attach-local.sh.  It:
#
#   1. Reads the saved state file to identify the bound device.
#   2. Unbinds the radio from USB-IP.
#   3. Stops the USB-IP daemon.
#   4. Removes the state file.
#
# No arguments required.
#
# Note: The SSH reverse tunnel is terminated by closing the terminal where
# usbip-attach-local.sh is running, or by killing the ssh process.  This
# script does NOT kill ssh sessions — it only cleans up USB-IP state.
###############################################################################

set -euo pipefail

# ─── Constants ────────────────────────────────────────────────────────────────
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

# ─── Load state ──────────────────────────────────────────────────────────────
RADIO_BUSID=""

if [[ -f "${STATE_FILE}" ]]; then
    RADIO_BUSID=$(grep '^RADIO_BUSID=' "${STATE_FILE}" 2>/dev/null | cut -d= -f2 || true)
    info "Loaded state from ${STATE_FILE}"
else
    warn "No state file found at ${STATE_FILE}."
    warn "Will attempt auto-detection."
fi

# ─── Auto-detect if no state ─────────────────────────────────────────────────
SILABS_VID="10c4"
SILABS_PID="ea60"
SEGGER_VID="1366"
SEGGER_PID="0105"
RADIO_IDS=("${SILABS_VID}:${SILABS_PID}" "${SEGGER_VID}:${SEGGER_PID}")

if [[ -z "${RADIO_BUSID}" ]]; then
    info "Scanning for BRD2703 radio (${RADIO_IDS[*]})..."

    for dev in /sys/bus/usb/devices/*/idVendor; do
        devDir=$(dirname "${dev}")
        vid=$(cat "${devDir}/idVendor" 2>/dev/null || true)
        pid=$(cat "${devDir}/idProduct" 2>/dev/null || true)

        for rid in "${RADIO_IDS[@]}"; do
            IFS=: read -r rVid rPid <<< "${rid}"

            if [[ "${vid}" == "${rVid}" && "${pid}" == "${rPid}" ]]; then
                RADIO_BUSID=$(basename "${devDir}")
                break 2
            fi
        done
    done
fi

# ─── Unbind the device ───────────────────────────────────────────────────────
if [[ -n "${RADIO_BUSID}" ]]; then
    info "Unbinding device ${RADIO_BUSID} from USB-IP..."
    sudo usbip unbind -b "${RADIO_BUSID}" 2>/dev/null || true
    ok "Device unbound."
else
    warn "No radio bus ID found — skipping unbind."
fi

# ─── Stop usbipd ─────────────────────────────────────────────────────────────
info "Stopping usbipd..."

if pgrep -x usbipd &>/dev/null; then
    sudo pkill -x usbipd 2>/dev/null || true
    sleep 1

    if pgrep -x usbipd &>/dev/null; then
        warn "usbipd is still running."
    else
        ok "usbipd stopped."
    fi
else
    ok "usbipd was not running."
fi

# ─── Clean up state file ─────────────────────────────────────────────────────
if [[ -f "${STATE_FILE}" ]]; then
    rm -f "${STATE_FILE}"
    ok "State file removed."
fi

# ─── Done ─────────────────────────────────────────────────────────────────────
echo ""
echo -e "${GREEN}${BOLD}=======================================${NC}"
echo -e "${GREEN}${BOLD} LOCAL TEARDOWN COMPLETE${NC}"
echo -e "${GREEN}${BOLD}=======================================${NC}"
echo ""
info "The SSH tunnel (if still open) must be closed separately"
info "by pressing Ctrl-C in the terminal running usbip-attach-local.sh."
echo ""
