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
# usbip-detach-remote.sh
#
# Run this on the REMOTE DEV SERVER to tear down the USB-IP attachment
# started by usbip-attach-remote.sh.  It:
#
#   1. Reads the saved state file to identify this user's vhci port.
#   2. Detaches ONLY this user's device (does not affect other users).
#   3. Removes the state file.
#
# No arguments required.
#
# This script is safe on shared servers — it only touches the vhci port
# recorded in the current user's state file.
###############################################################################

set -euo pipefail

# ─── Constants ────────────────────────────────────────────────────────────────
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

# ─── Load state ──────────────────────────────────────────────────────────────
if [[ ! -f "${STATE_FILE}" ]]; then
    die "No state file found at ${STATE_FILE}." \
        "Either usbip-attach-remote.sh was not run, or it was already cleaned up."
fi

VHCI_PORT=$(grep '^VHCI_PORT=' "${STATE_FILE}" 2>/dev/null | cut -d= -f2 || true)
RADIO_DEVICE=$(grep '^RADIO_DEVICE=' "${STATE_FILE}" 2>/dev/null | cut -d= -f2 || true)
RADIO_BUSID=$(grep '^RADIO_BUSID=' "${STATE_FILE}" 2>/dev/null | cut -d= -f2 || true)

info "State loaded from ${STATE_FILE}"
info "  VHCI port:    ${VHCI_PORT:-unknown}"
info "  Radio device: ${RADIO_DEVICE:-unknown}"
info "  Bus ID:       ${RADIO_BUSID:-unknown}"

# ─── Detach the device ───────────────────────────────────────────────────────
if [[ -n "${VHCI_PORT}" && "${VHCI_PORT}" != "unknown" ]]; then
    info "Detaching vhci port ${VHCI_PORT}..."

    if sudo usbip detach -p "${VHCI_PORT}" 2>/dev/null; then
        ok "Device detached from vhci port ${VHCI_PORT}."
    else
        warn "Detach returned an error — device may already be detached."
    fi

    # Wait briefly for the ttyACM device to disappear
    if [[ -n "${RADIO_DEVICE}" && -e "${RADIO_DEVICE}" ]]; then
        info "Waiting for ${RADIO_DEVICE} to disappear..."
        waited=0

        while [[ -e "${RADIO_DEVICE}" && ${waited} -lt 5 ]]; do
            sleep 1
            waited=$((waited + 1))
        done

        if [[ -e "${RADIO_DEVICE}" ]]; then
            warn "${RADIO_DEVICE} still exists after detach."
        else
            ok "${RADIO_DEVICE} removed."
        fi
    fi
else
    warn "No vhci port recorded — cannot safely detach."
    warn "Use 'usbip port' to list attached devices and 'sudo usbip detach -p <port>' manually."
fi

# ─── Clean up state file ─────────────────────────────────────────────────────
rm -f "${STATE_FILE}"
ok "State file removed."

# ─── Done ─────────────────────────────────────────────────────────────────────
echo ""
echo -e "${GREEN}${BOLD}=======================================${NC}"
echo -e "${GREEN}${BOLD} REMOTE TEARDOWN COMPLETE${NC}"
echo -e "${GREEN}${BOLD}=======================================${NC}"
echo ""
info "The USB device has been detached from this server."
info "Close the SSH tunnel on your workstation (Ctrl-C) or run"
info "${BOLD}scripts/remote-radio/usbip-detach-local.sh${NC} to unbind and stop usbipd."
echo ""
