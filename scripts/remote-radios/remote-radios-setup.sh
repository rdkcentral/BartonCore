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
#
# remote-radios-setup.sh — one-command setup for using your LOCAL Zigbee/Thread
# and Bluetooth radios with a REMOTE dev server's devcontainers.
#
# Run this on your WORKSTATION (Ubuntu) where the USB radios are plugged in.
# No checkout is needed there:
#
#     curl -fsSL <raw>/scripts/remote-radios/remote-radios-setup.sh \
#       | bash -s -- <user>@<devserver>
#
# or, from a checkout:
#
#     scripts/remote-radios/remote-radios-setup.sh <user>@<devserver>
#
# Either way it installs the files it needs into ~/.config/remote-radios/bin/
# and the service runs from there, then this exits.
#
# Overridable: REMOTE_RADIOS_REPO, REMOTE_RADIOS_REF (default main) or
# REMOTE_RADIOS_RAW_BASE to install from somewhere other than rdkcentral/main.
#
# What it does:
#   * Installs its runtime (this script + remote-serial.py) to
#     ~/.config/remote-radios/bin/, from the checkout or by download.
#   * First run: detects your Silabs Zigbee/Thread radio and a dedicated
#     Bluetooth USB dongle (prompting only if the choice is ambiguous), and
#     saves the selection — and the dev server — to
#     ~/.config/remote-radios/config.
#   * One-time sudo to install two root-owned helpers and a narrow sudoers rule
#     for the `remote-radios` group (only when a Bluetooth dongle is in play).
#   * Installs and starts ONE per-user systemd service, remote-radios.service,
#     which owns the whole workstation side:
#       - runs usbipd and keeps the Bluetooth dongle bound for export
#       - Silabs   : serial-over-SSH tunnel (remote-serial.py -> UNIX socket)
#       - Bluetooth: usb-ip, reverse-tunnelled over SSH
#       - writes ~/.remote-radios/radios.env on the dev server
#     It is wanted by default.target, so it starts at login and stops at
#     logout, and re-executes this script with --service to do the work.
#   * The service monitors health, tears everything down cleanly when the dev
#     server becomes unreachable, re-establishes when it returns, and picks up
#     radios plugged in after it started.
#
# Re-running validates the saved radios and refreshes the service; it restarts
# the service only when something actually changed.
#
# If you have no radios (or don't want to use them), you never need to run this;
# the devcontainer runs with simulated Thread/Zigbee and no Bluetooth.
#
# See docs/REMOTE_RADIO_FOR_DEVELOPMENT.md for details.

set -euo pipefail

# ---------------------------------------------------------------------------
# Constants / paths
# ---------------------------------------------------------------------------
CONFIG_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/remote-radios"
CONFIG_FILE="${CONFIG_DIR}/config"
STATE_DIR="${XDG_RUNTIME_DIR:-/tmp}/remote-radios-$(id -u)"
# Written by --take-over just before it restarts the service.  The restart
# stops the old instance, whose teardown would otherwise release the lease it
# just took, and the other workstation, waiting for exactly that, would take it
# straight back before the new instance starts.
TAKEOVER_MARKER="${STATE_DIR}/take-over"
SYSTEMD_USER_DIR="$HOME/.config/systemd/user"

# Root-executed helpers live outside $HOME on purpose.  They are invoked via
# sudo with NOPASSWD, so anywhere the invoking user can write would turn that
# grant into an unrestricted root shell.  root-owned, non-writable, fixed path.
HELPER_DIR="/usr/local/lib/remote-radios"
BIND_HELPER="${HELPER_DIR}/usbipd-bind.sh"
RELEASE_HELPER="${HELPER_DIR}/usbipd-release.sh"

# The sudo grant is given to a dedicated group rather than an individual user,
# so the same one-time install serves every developer on the machine.
RR_GROUP="remote-radios"
RR_SUDOERS="/etc/sudoers.d/remote-radios"

# usb-ip binds on this local TCP port on the workstation (usbipd default 3240)
# and we reverse-tunnel it to a per-user UNIX socket on the dev server (no sshd
# GatewayPorts change needed); socat in the container bridges it back to TCP.
USBIPD_LOCAL_PORT=3240

# Silabs local relay port for remote-serial.py (loopback only).
SILABS_LOCAL_PORT=20000

# The radio normally runs here, on the workstation: cpcd drives it, and zigbeed
# runs next to it, because Zigbee's MAC timing (answering beacon requests during
# a joining device's scan) cannot tolerate the round trip to the dev server.
# Only cpcd's client sockets (for the dev server's otbr-agent) and zigbeed's
# EZSP link (for ZigbeeCore) cross the tunnel, both via radio-relay.py.
#
# While an hh4 QEMU guest holds the claim on the radio, the workstation instead
# forwards the raw serial port (remote-serial.py), since the guest runs its own
# cpcd.  RADIO_MODE is "cpc" or "serial" accordingly.
CPC_INSTANCE="cpcd_rr"
CPC_RELAY_PORT=20001
EZSP_RELAY_PORT=20002
RADIO_MODE="cpc"
CPC_BINARIES="cpcd zigbeed libcpc.so.3"
# cpcd and zigbeed run from here.  ZigbeeCore builds the workstation's cpcd and
# libcpc to keep cpcd's sockets under the working directory (rather than the
# shared, possibly root-owned /dev/shm), and zigbeed keeps the Zigbee network's
# tokens here, so the network follows this workstation across reboots.
RADIO_DIR="${XDG_STATE_HOME:-$HOME/.local/state}/remote-radios"
CPC_SOCKET_DIR="${RADIO_DIR}/cpcd/${CPC_INSTANCE}"

# Known Silabs serial radio USB IDs (VID:PID).
SILABS_IDS="10c4:ea60 1366:0105 1366:1024"

# Directory of this script when it was run from a checkout.  Deliberately empty
# when piped in from curl: BASH_SOURCE is then "bash" (or unset), and taking its
# dirname would silently resolve to the current directory.
if [ -n "${BASH_SOURCE[0]:-}" ] && [ -f "${BASH_SOURCE[0]:-}" ]; then
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
else
    SCRIPT_DIR=""
fi
SCRIPT_NAME="remote-radios-setup.sh"

# A developer's workstation is not necessarily a machine they check the repo out
# on — that is the whole point of the curl|bash entry point — so the setup
# installs the two files the service needs into the user's own config dir and
# the service runs from there.  Unlike the sudo helpers these are executed as
# the user, never as root, so keeping them under $HOME is not a privilege risk.
INSTALL_DIR="${CONFIG_DIR}/bin"
INSTALLED_SETUP="${INSTALL_DIR}/${SCRIPT_NAME}"

# Version of the *installed layout*, bumped whenever a previously set-up
# workstation needs something migrated rather than merely overwritten.  It is
# recorded in VERSION_FILE once setup completes, so a later run can tell which
# layout it is upgrading from.
#   1  remote-radios-usbip.service only; sudo helper under $HOME; the SSH
#      tunnels were supervised by a foreground run of this script.
#   2  one remote-radios.service owning both radios; root helpers in
#      /usr/local/lib/remote-radios; runtime installed in ${INSTALL_DIR}.
SETUP_VERSION=2
VERSION_FILE="${CONFIG_DIR}/version"

# Where to fetch those files when there is no checkout to copy them from.
RAW_REPO="${REMOTE_RADIOS_REPO:-rdkcentral/BartonCore}"
RAW_REF="${REMOTE_RADIOS_REF:-main}"
RAW_BASE="${REMOTE_RADIOS_RAW_BASE:-https://raw.githubusercontent.com/${RAW_REPO}/${RAW_REF}/scripts/remote-radios}"

# resolve_raw_base — re-derive the download location once the saved config has
# been read.  Precedence: an explicit environment override, then the ref this
# workstation was installed from, then the default branch.  Remembering the ref
# matters because a workstation installed from a branch or a commit must not
# silently replace itself with whatever happens to be on the default branch the
# next time setup is re-run.
resolve_raw_base() {
    RAW_REF="${REMOTE_RADIOS_REF:-${SETUP_REF:-main}}"
    RAW_BASE="${REMOTE_RADIOS_RAW_BASE:-https://raw.githubusercontent.com/${RAW_REPO}/${RAW_REF}/scripts/remote-radios}"
}

# Single user service that owns everything on the workstation side: the usb-ip
# export of the Bluetooth dongle and both SSH tunnels.  It is a user unit wanted
# by default.target, so it starts at login and stops at logout.
SERVICE_NAME="remote-radios.service"
LEGACY_SERVICE_NAME="remote-radios-usbip.service"

# Set when this process IS the service (see --service), rather than the
# interactive installer that sets the service up.
SERVICE_MODE=0

# ---------------------------------------------------------------------------
# Logging
# ---------------------------------------------------------------------------
BOLD=$'\033[1m'; GREEN=$'\033[0;32m'; RED=$'\033[0;31m'; YELLOW=$'\033[1;33m'; NC=$'\033[0m'
info() { printf '%s[remote-radios]%s %s\n' "$BOLD" "$NC" "$*"; }
ok()   { printf '%s[remote-radios] OK:%s %s\n' "$GREEN" "$NC" "$*"; }
warn() { printf '%s[remote-radios] WARN:%s %s\n' "$YELLOW" "$NC" "$*" >&2; }
die()  { printf '%s[remote-radios] ERROR:%s %s\n' "$RED" "$NC" "$*" >&2; exit 1; }

# ---------------------------------------------------------------------------
# Prerequisite checks
# ---------------------------------------------------------------------------
require_cmds() {
    local missing=()
    for c in ssh usbip python3 socat; do
        command -v "$c" >/dev/null 2>&1 || missing+=("$c")
    done
    if [ ${#missing[@]} -gt 0 ]; then
        die "Missing required commands: ${missing[*]}
  Install with:  sudo apt-get install -y openssh-client usbip python3 socat
  (usbip is provided by the 'usbip' or 'linux-tools-generic' package)"
    fi
    python3 -c 'import serial' 2>/dev/null || \
        warn "python3 'pyserial' not found — install with: pip install --user pyserial"
}

# ---------------------------------------------------------------------------
# Radio detection
# ---------------------------------------------------------------------------

# find_silabs_candidates — echo lines "<devnode> <vid:pid> <serial> <desc>".
find_silabs_candidates() {
    local dev vid pid serial desc idpath
    for dev in /dev/ttyACM* /dev/ttyUSB*; do
        [ -e "$dev" ] || continue
        idpath=$(udevadm info -q property -n "$dev" 2>/dev/null || true)
        vid=$(echo "$idpath" | sed -n 's/^ID_VENDOR_ID=//p')
        pid=$(echo "$idpath" | sed -n 's/^ID_MODEL_ID=//p')
        [ -n "$vid" ] && [ -n "$pid" ] || continue
        local key="${vid,,}:${pid,,}"
        case " $SILABS_IDS " in
            *" $key "*)
                serial=$(echo "$idpath" | sed -n 's/^ID_SERIAL_SHORT=//p')
                desc=$(echo "$idpath" | sed -n 's/^ID_MODEL=//p' | tr '_' ' ')
                echo "$dev $key ${serial:-none} ${desc:-Silabs}"
                ;;
        esac
    done
}

# hci_usb_path — given an hciN name, echo the owning USB device sysfs dir.
hci_usb_path() {
    local hci="$1" real usbpath
    [ -e "/sys/class/bluetooth/${hci}" ] || return 1
    real=$(readlink -f "/sys/class/bluetooth/${hci}")
    usbpath="$real"
    while [ -n "$usbpath" ] && [ "$usbpath" != "/" ]; do
        if [ -f "$usbpath/idVendor" ]; then
            echo "$usbpath"
            return 0
        fi
        usbpath=$(dirname "$usbpath")
    done
    return 1
}

# active_bt_hci_usb_paths — echo the USB sysfs path backing ONLY the adapter the
# workstation is actively using (its *default* bluetoothd controller), so that
# adapter is never claimed for forwarding.  Other enumerated adapters (e.g. a
# dedicated dongle) remain available.
#
# Resolution order:
#   1. The controller bluetoothctl marks as [default] -> its hciN -> USB path.
#   2. Fallback: hci0 (conventional primary) when bluetoothctl is unavailable.
active_bt_hci_usb_paths() {
    local default_addr="" hci addr hcipath

    if command -v bluetoothctl >/dev/null 2>&1; then
        # "Controller <ADDR> <name> [default]" — grab the default one's address.
        default_addr=$(timeout 5 bluetoothctl list 2>/dev/null \
            | awk '/\[default\]/{print $2; exit}')
    fi

    if [ -n "$default_addr" ]; then
        # Map the BD address back to an hciN via hciconfig.
        for hcipath in /sys/class/bluetooth/hci*; do
            [ -e "$hcipath" ] || continue
            hci=$(basename "$hcipath")
            addr=$(hciconfig "$hci" 2>/dev/null | awk '/BD Address:/{print $3; exit}')
            if [ "${addr^^}" = "${default_addr^^}" ]; then
                hci_usb_path "$hci"
                return 0
            fi
        done
    fi

    # Fallback: treat hci0 as the workstation's primary if present.
    if [ -e /sys/class/bluetooth/hci0 ]; then
        hci_usb_path hci0
    fi
}

# find_bt_candidates — echo lines "<busid> <vid:pid> <desc>" for USB Bluetooth
# dongles, EXCLUDING any adapter currently bound to the workstation's own
# bluetoothd (so the developer's local Bluetooth is never disturbed).
find_bt_candidates() {
    local active_paths busid vid pid desc devpath real
    active_paths="$(active_bt_hci_usb_paths)"

    # Enumerate USB devices; a Bluetooth device has bDeviceClass e0 (wireless)
    # OR exposes an hci interface.  We match on class e0 and on known dongles.
    for devpath in /sys/bus/usb/devices/*; do
        [ -f "$devpath/idVendor" ] || continue
        [ -f "$devpath/busnum" ] || continue
        local dclass
        dclass=$(cat "$devpath/bDeviceClass" 2>/dev/null || echo "")
        # Class e0 = Wireless Controller (covers BT dongles like TP-Link UB500).
        [ "$dclass" = "e0" ] || continue

        real=$(readlink -f "$devpath")
        # Skip if this USB device backs an active workstation HCI adapter.
        local skip=0 ap
        for ap in $active_paths; do
            case "$(readlink -f "$ap")" in
                "$real"*|"$real") skip=1; break;;
            esac
            case "$real" in
                "$(readlink -f "$ap")"*) skip=1; break;;
            esac
        done
        [ "$skip" = "1" ] && continue

        busid=$(basename "$devpath")
        # usbip busids look like "1-2" / "1-2.1"; skip root hubs/interfaces.
        case "$busid" in
            *usb*|*:*) continue;;
        esac
        vid=$(cat "$devpath/idVendor" 2>/dev/null)
        pid=$(cat "$devpath/idProduct" 2>/dev/null)
        desc=$(cat "$devpath/product" 2>/dev/null || echo "Bluetooth dongle")
        echo "$busid ${vid,,}:${pid,,} $desc"
    done
}

# choose_one — given candidate lines on stdin, a human label, and the field to
# return, auto-select if exactly one, prompt if several, empty if none.
# Echoes the chosen first field (devnode or busid).
choose_one() {
    local label="$1"; shift
    local -a lines=()
    local l
    while IFS= read -r l; do [ -n "$l" ] && lines+=("$l"); done

    if [ "${#lines[@]}" -eq 0 ]; then
        echo ""
        return 0
    fi
    if [ "${#lines[@]}" -eq 1 ]; then
        info "Auto-selected ${label}: ${lines[0]}" >&2
        echo "${lines[0]%% *}"
        return 0
    fi

    warn "Multiple ${label} candidates found:"
    local i=1
    for l in "${lines[@]}"; do
        printf '   %d) %s\n' "$i" "$l" >&2
        i=$((i + 1))
    done
    local choice
    printf '%sSelect %s [1-%d]: %s' "$BOLD" "$label" "${#lines[@]}" "$NC" >&2
    read -r choice </dev/tty
    [[ "$choice" =~ ^[0-9]+$ ]] && [ "$choice" -ge 1 ] && [ "$choice" -le "${#lines[@]}" ] \
        || die "Invalid selection."
    echo "${lines[$((choice - 1))]%% *}"
}

# ---------------------------------------------------------------------------
# Config load/save
# ---------------------------------------------------------------------------
load_config() {
    [ -f "$CONFIG_FILE" ] && . "$CONFIG_FILE" || true
}

save_config() {
    mkdir -p "$CONFIG_DIR"
    cat > "$CONFIG_FILE" <<EOF
# remote-radios developer config — generated by remote-radios-setup.sh
# Edit or delete this file to re-run detection.
SSH_TARGET="${SSH_TARGET:-}"
SETUP_REF="${RAW_REF:-main}"           # ref this workstation installs from
SILABS_SERIAL="${SILABS_SERIAL:-}"     # persistent USB serial of the Silabs radio
SILABS_DEV="${SILABS_DEV:-}"
BT_BUSID="${BT_BUSID:-}"
BT_VIDPID="${BT_VIDPID:-}"
EOF
    chmod 600 "$CONFIG_FILE"
    ok "Saved config to ${CONFIG_FILE}"
}

# resolve_silabs_dev — re-resolve the Silabs device node from its saved serial
# (device nodes like /dev/ttyACM0 are not stable across reboots/replugs).
resolve_silabs_dev() {
    local want="$1" line
    while IFS= read -r line; do
        set -- $line
        if [ "$3" = "$want" ]; then echo "$1"; return 0; fi
    done < <(find_silabs_candidates)
    echo ""
}

# ---------------------------------------------------------------------------
# First-run detection
# ---------------------------------------------------------------------------
detect_radios() {
    info "Detecting local radios..."

    # Silabs (required for Zigbee/Thread).
    SILABS_DEV="$(find_silabs_candidates | choose_one 'Silabs Zigbee/Thread radio')"
    if [ -n "$SILABS_DEV" ]; then
        SILABS_SERIAL=$(udevadm info -q property -n "$SILABS_DEV" 2>/dev/null \
            | sed -n 's/^ID_SERIAL_SHORT=//p')
        ok "Silabs radio: ${SILABS_DEV} (serial ${SILABS_SERIAL:-unknown})"
    else
        warn "No Silabs Zigbee/Thread radio detected — Thread/Zigbee will be unavailable."
        SILABS_SERIAL=""
    fi

    # Bluetooth dongle (optional; never the workstation's active adapter).
    local bt_line
    bt_line="$(find_bt_candidates | choose_one 'Bluetooth USB dongle')"
    BT_BUSID="$bt_line"
    if [ -n "$BT_BUSID" ]; then
        BT_VIDPID=$(cat "/sys/bus/usb/devices/${BT_BUSID}/idVendor" 2>/dev/null),$(cat "/sys/bus/usb/devices/${BT_BUSID}/idProduct" 2>/dev/null)
        ok "Bluetooth dongle: busid ${BT_BUSID}"
    else
        warn "No dedicated Bluetooth dongle detected — Bluetooth/Matter-BLE will be unavailable."
        warn "(Your workstation's built-in Bluetooth is intentionally never used.)"
        BT_VIDPID=""
    fi

    [ -n "$SILABS_DEV" ] || [ -n "$BT_BUSID" ] || \
        die "No usable radios found. Plug in a Silabs radio and/or a Bluetooth dongle and re-run."
}

# ---------------------------------------------------------------------------
# Per-user systemd unit for usbipd + bind (one-time sudo to install)
# ---------------------------------------------------------------------------
install_privileged_helpers() {
    [ -n "${BT_BUSID:-}" ] || return 0

    local staging="${CONFIG_DIR}/staging"
    local helper="${staging}/usbipd-bind.sh"
    local release="${staging}/usbipd-release.sh"
    mkdir -p "$staging" "$SYSTEMD_USER_DIR"

    # usbipd + bind need root; we run them via two tiny sudo helpers invoked by
    # a user systemd service.  The helpers are staged here and then installed
    # root-owned under ${HELPER_DIR}; a NOPASSWD sudoers drop-in for that fixed
    # path (one-time sudo) keeps subsequent runs password-free.
    cat > "$helper" <<'HELPER'
#!/usr/bin/env bash
# Started by the remote-radios user service. Runs usbipd and binds the dongle.
# Root-only (invoked via sudo by the user service).
set -u
BUSID="${1:-}"
log() { echo "[usbipd-bind] $*"; }

# This runs as root with an argument supplied by an unprivileged caller, so the
# busid is validated before it reaches usbip or any path construction.
if ! [[ "$BUSID" =~ ^[0-9]+-[0-9]+(\.[0-9]+)*$ ]]; then
    log "ERROR: refusing to act on malformed busid '${BUSID}'."
    exit 1
fi

# Single-instance guard.  A user systemd manager cannot signal these root-owned
# helpers, so `systemctl --user restart` leaves the previous one running; two
# watchdogs would then fight over the same dongle.  The kernel drops this lock
# automatically when the holder dies, so it never goes stale -- provided no
# child inherits the descriptor, which is why usbipd is started with 9>&-.
mkdir -p /run/remote-radios
exec 9>"/run/remote-radios/usbipd-bind-${BUSID}.lock"
if ! flock -n 9; then
    # Distinguish a genuine second instance from a descriptor leak, which
    # otherwise looks identical and silently stops the dongle being bound ever
    # again.
    holder="$(cat "/run/remote-radios/usbipd-bind-${BUSID}.pid" 2>/dev/null || true)"
    if [ -n "$holder" ] && [ -d "/proc/${holder}" ]; then
        log "another instance (pid ${holder}) is already managing ${BUSID}; exiting."
    else
        log "ERROR: the lock for ${BUSID} is held but no bind helper is running."
        log "       Something inherited the lock descriptor -- almost certainly a"
        log "       usbipd left over from an older version of this helper."
        log "       Clear it with:  sudo pkill -x usbipd"
    fi
    exit 0
fi
echo "$$" > "/run/remote-radios/usbipd-bind-${BUSID}.pid"

# Load kernel modules for the usb-ip server side.  usbip-host provides the
# bind driver; without it `usbip bind` fails with "unable to bind device".
modprobe usbip-core 2>/dev/null || true
if ! modprobe usbip-host 2>/dev/null; then
    log "ERROR: could not load usbip-host module."
    log "       Install kernel usbip support (e.g. linux-tools-\$(uname -r) / the"
    log "       usbip modules for your kernel) and retry."
    exit 1
fi
if ! grep -qw usbip_host /proc/modules 2>/dev/null; then
    log "ERROR: usbip-host module not present after modprobe; cannot bind ${BUSID}."
    exit 1
fi

ensure_usbipd() {
    if ! pgrep -x usbipd >/dev/null 2>&1; then
        # Close the lock fd in the child.  usbipd daemonizes and would
        # otherwise inherit fd 9 and hold the single-instance flock for its
        # whole lifetime -- outliving this helper, so that every later instance
        # sees the lock taken and exits, and the dongle is never bound again.
        usbipd -D 9>&-
        sleep 1
    fi
}

device_present() { [ -e "/sys/bus/usb/devices/${BUSID}" ]; }

# The device is exported only while it sits on the usbip-host driver.  Anything
# that re-enumerates the dongle (replug, suspend/resume, a USB port reset)
# hands it back to btusb, which silently ends the export even though this
# service is still "running".
#
# Note this is a *device*-level check, not a per-interface one: `usbip bind`
# detaches the interfaces and binds the whole device to usbip-host, so the
# /sys/bus/usb/devices/<busid>:* interface entries disappear while exported.
# Probing those would report a healthy export as unbound and rebind forever.
is_bound() {
    local drv
    drv="$(basename "$(readlink -f "/sys/bus/usb/devices/${BUSID}/driver" 2>/dev/null)" 2>/dev/null)"
    [ "$drv" = "usbip-host" ]
}

# Bind the dongle.  A Bluetooth dongle's interfaces are normally claimed by
# btusb; `usbip bind` detaches them first.  Treat "already bound" as success,
# but surface any other failure with diagnostics instead of hiding it.
do_bind() {
    local bind_out bind_rc intf
    bind_out="$(usbip bind -b "$BUSID" 2>&1)"
    bind_rc=$?
    if [ $bind_rc -ne 0 ]; then
        if echo "$bind_out" | grep -qi "already bound"; then
            log "${BUSID} already bound."
        else
            log "ERROR: usbip bind -b ${BUSID} failed: ${bind_out}"
            # One retry after forcing the interfaces off their kernel driver,
            # which clears the common half-detached btusb state.
            for intf in /sys/bus/usb/devices/${BUSID}:*; do
                [ -e "$intf/driver" ] && echo "$(basename "$intf")" > "$intf/driver/unbind" 2>/dev/null || true
            done
            sleep 1
            if usbip bind -b "$BUSID" 2>/dev/null; then
                log "${BUSID} bound after interface reset."
            else
                log "ERROR: still cannot bind ${BUSID}. Is the busid correct and the"
                log "       device present? Try: usbip list -l"
                return 1
            fi
        fi
    fi
    return 0
}

ensure_usbipd
do_bind || exit 1

# Verify the device is now exportable before declaring success.
if ! usbip list -l 2>/dev/null | grep -q "busid ${BUSID} .*(.*)" ; then
    log "WARNING: ${BUSID} not shown by 'usbip list -l' after bind."
fi
log "usb-ip export ready for ${BUSID}."

# Stay in foreground so systemd tracks the service; exit unbinds on stop.
#
# Reassert the export on every pass rather than just watching usbipd: a lost
# binding is invisible to a liveness check, so without this the service can
# report healthy for days while exporting nothing and the dev-server container
# retries an attach that can never succeed.
trap 'rm -f "/run/remote-radios/usbipd-bind-${BUSID}.pid"; usbip unbind -b "$BUSID" 2>/dev/null || true' EXIT
while true; do
    sleep 5

    ensure_usbipd

    if ! device_present; then
        continue
    fi

    if ! is_bound; then
        log "WARNING: ${BUSID} lost its usb-ip binding (driver reverted); re-binding."
        if do_bind; then
            log "usb-ip export restored for ${BUSID}."
        else
            log "ERROR: re-bind of ${BUSID} failed; retrying."
        fi
    fi
done
HELPER
    chmod +x "$helper"

    # Release helper: the counterpart that makes `systemctl --user stop` mean
    # something.  The bind helper runs as root, so a user systemd manager cannot
    # signal it — without this the watchdog survives "stop" and re-binds the
    # dongle within seconds, and the developer can never reclaim it for local
    # Bluetooth use.
    cat > "$release" <<'RELEASE'
#!/usr/bin/env bash
# Stops the remote-radios usb-ip export and returns the dongle to the host.
# Root-only (invoked via sudo by the user service's ExecStopPost).
set -u
BUSID="${1:-}"
log() { echo "[usbipd-release] $*"; }

if ! [[ "$BUSID" =~ ^[0-9]+-[0-9]+(\.[0-9]+)*$ ]]; then
    log "ERROR: refusing to act on malformed busid '${BUSID}'."
    exit 1
fi

pidfile="/run/remote-radios/usbipd-bind-${BUSID}.pid"
if [ -r "$pidfile" ]; then
    pid="$(cat "$pidfile" 2>/dev/null || true)"
    # Confirm the PID really is our helper before signalling it; PIDs are
    # recycled, and this runs as root.
    if [ -n "${pid:-}" ] && tr '\0' ' ' < "/proc/${pid}/cmdline" 2>/dev/null \
        | grep -q "usbipd-bind.sh ${BUSID}"; then
        kill "$pid" 2>/dev/null || true
        for _ in 1 2 3 4 5; do
            kill -0 "$pid" 2>/dev/null || break
            sleep 1
        done
        kill -9 "$pid" 2>/dev/null || true
    fi
    rm -f "$pidfile"
fi

# The watchdog unbinds on exit, but do it here too: the helper may have been
# killed outright, or never have been running at all.
usbip unbind -b "$BUSID" 2>/dev/null || true
log "released ${BUSID}."
RELEASE
    chmod +x "$release"

    # Probe the capability we actually need, not the file.  The drop-in grants
    # NOPASSWD for the helpers only, so a `sudo -n test -f` probe would itself
    # need a password and always report "missing" — re-running the install (and
    # hard-failing when there is no terminal to authenticate against).
    local need_install=0
    sudo -n -l "$BIND_HELPER" >/dev/null 2>&1 || need_install=1
    sudo -n -l "$RELEASE_HELPER" >/dev/null 2>&1 || need_install=1
    cmp -s "$helper" "$BIND_HELPER" || need_install=1
    cmp -s "$release" "$RELEASE_HELPER" || need_install=1

    if [ "$need_install" -eq 1 ]; then
        info "Installing root helpers, the ${RR_GROUP} group and a sudoers rule (needs sudo once)..."
        sudo install -d -m 0755 -o root -g root "$HELPER_DIR"
        sudo install -m 0755 -o root -g root "$helper"  "$BIND_HELPER"
        sudo install -m 0755 -o root -g root "$release" "$RELEASE_HELPER"

        sudo groupadd -f "$RR_GROUP"
        sudo usermod -aG "$RR_GROUP" "$(id -un)"

        # NOPASSWD for these two root-owned helpers only — nothing broader.
        # Listing a command without arguments lets sudo accept any arguments;
        # each helper validates its own busid rather than trusting the caller.
        local tmp_sudoers="${staging}/sudoers"
        cat > "$tmp_sudoers" <<EOF
# Installed by remote-radios-setup.sh.  Grants the ${RR_GROUP} group permission
# to run the usb-ip export helpers as root without a password.  The helpers are
# root-owned and not writable by the group, so this grant cannot be widened by
# editing them.
%${RR_GROUP} ALL=(root) NOPASSWD: ${BIND_HELPER}, ${RELEASE_HELPER}
EOF
        # Never install a sudoers file without checking it first: a syntax error
        # in /etc/sudoers.d can lock everyone out of sudo on the machine.
        if ! sudo visudo -c -q -f "$tmp_sudoers"; then
            die "Generated sudoers rule failed validation; refusing to install it."
        fi
        sudo install -m 0440 -o root -g root "$tmp_sudoers" "$RR_SUDOERS"
        rm -f "$tmp_sudoers"

        # Retire the older per-user rule, which pointed at a helper inside the
        # user's own home — writable by that user, and therefore equivalent to
        # passwordless root.
        local legacy="/etc/sudoers.d/remote-radios-$(id -un)"
        if sudo test -f "$legacy" 2>/dev/null; then
            sudo rm -f "$legacy"
            info "Removed legacy sudoers rule ${legacy} (helper lived in \$HOME)."
        fi
        rm -f "${CONFIG_DIR}/usbipd-bind.sh"

        # Group membership is established at login, so the running shell (and
        # the systemd user manager) will not have it yet on first install.
        if ! id -nG | tr ' ' '\n' | grep -qx "$RR_GROUP"; then
            echo
            ok "Installed. You have been added to the '${RR_GROUP}' group."
            info "Group membership only takes effect on a new login session."
            info "Log out and back in, then finish setup by running:"
            printf '    %s%s %s%s\n' "$BOLD" "$INSTALLED_SETUP" "$SSH_TARGET" "$NC"
            exit 0
        fi
    fi

    HELPERS_CHANGED="$need_install"
}

# ---------------------------------------------------------------------------
# Runtime installation
# ---------------------------------------------------------------------------
# Put the files the service needs into ${INSTALL_DIR}: copied from the checkout
# when this was run from one, downloaded from the published raw URLs when it was
# piped in from curl.  The service always runs the installed copy, so the
# workstation never needs a checkout and nothing breaks if one is moved later.
install_runtime_files() {
    local staging="${CONFIG_DIR}/staging"
    mkdir -p "$INSTALL_DIR" "$staging"
    RUNTIME_CHANGED=0

    local f tmp src
    for f in "$SCRIPT_NAME" remote-serial.py radio-relay.py; do
        tmp="${staging}/${f}"
        src=""
        if [ -n "$SCRIPT_DIR" ] && [ "$SCRIPT_DIR" != "$INSTALL_DIR" ] \
            && [ -f "${SCRIPT_DIR}/${f}" ]; then
            cp -- "${SCRIPT_DIR}/${f}" "$tmp" && src="checkout"
        elif command -v curl >/dev/null 2>&1 \
            && curl -fsSL "${RAW_BASE}/${f}" -o "$tmp" 2>/dev/null; then
            # This branch also covers re-running the already-installed copy.
            # It deliberately does NOT skip: picking up a newer version from the
            # repository is the only way an installed workstation can upgrade.
            src="download"
        fi

        if [ -z "$src" ]; then
            if [ -s "${INSTALL_DIR}/${f}" ]; then
                # Offline, or the ref moved.  Keeping a working installation is
                # better than failing a run that did not need a newer copy.
                warn "Could not refresh ${f} from ${RAW_BASE}; keeping the installed copy."
                rm -f "$tmp"
                continue
            fi
            die "Could not obtain ${f} from ${RAW_BASE}.
       Check the ref exists, or set REMOTE_RADIOS_REF / REMOTE_RADIOS_RAW_BASE."
        fi

        if ! cmp -s "$tmp" "${INSTALL_DIR}/${f}"; then
            # Replace by rename, never in place.  This script may be the file
            # being replaced, and bash reads a script lazily as it executes —
            # overwriting the inode under a running shell corrupts the rest of
            # the run, whereas a rename leaves it reading the old inode safely.
            chmod 0755 "$tmp"
            mv -f "$tmp" "${INSTALL_DIR}/${f}"
            RUNTIME_CHANGED=1
            info "Updated ${f} (from ${src})."
        fi
        rm -f "$tmp"
    done

    [ -s "$INSTALLED_SETUP" ] || die "Runtime install failed: ${INSTALLED_SETUP} is missing."
    ok "Workstation runtime installed in ${INSTALL_DIR}"
}

# maybe_reexec_updated_setup — hand the rest of the run to the copy we just
# installed, so that a newer version's logic and migrations are what actually
# run.  Only needed when this process IS the installed copy and has just been
# replaced; from a checkout or from curl the running copy is already the new
# one, and re-executing would pointlessly repeat radio detection.
maybe_reexec_updated_setup() {
    [ "${RUNTIME_CHANGED:-0}" = "1" ] || return 0
    [ -z "${REMOTE_RADIOS_REEXECED:-}" ] || return 0
    [ -n "$SCRIPT_DIR" ] && [ "$SCRIPT_DIR" = "$INSTALL_DIR" ] || return 0

    info "Setup script was updated; re-running the new version..."
    export REMOTE_RADIOS_REEXECED=1
    exec "$INSTALLED_SETUP" "$@"
}

# ---------------------------------------------------------------------------
# Upgrading an existing workstation
# ---------------------------------------------------------------------------
installed_version() {
    local v=""
    [ -f "$VERSION_FILE" ] && v="$(cat "$VERSION_FILE" 2>/dev/null)"
    case "$v" in
        '' | *[!0-9]*) ;;
        *) echo "$v"; return 0 ;;
    esac

    # No record.  Decide from what is actually on disk rather than from the mere
    # presence of a config file: a setup interrupted before it finished — most
    # often to wait for a new login session to pick up the group — also leaves a
    # config behind, and must not be mistaken for an older layout.
    if [ -e "${SYSTEMD_USER_DIR}/${LEGACY_SERVICE_NAME}" ] \
        || [ -e "${CONFIG_DIR}/usbipd-bind.sh" ]; then
        echo 1
    elif [ -f "$CONFIG_FILE" ]; then
        echo "$SETUP_VERSION"
    else
        echo 0
    fi
}

record_version() {
    mkdir -p "$CONFIG_DIR"
    printf '%s\n' "$SETUP_VERSION" > "$VERSION_FILE"
}

# migrate_installed_state — retire artifacts left by older layouts.  Everything
# it touches is also handled idempotently by the install functions; this exists
# so an upgrade says what it is doing, and so removals that belong to no single
# installer have a home.
migrate_installed_state() {
    local from
    from="$(installed_version)"
    [ "$from" -ge "$SETUP_VERSION" ] && return 0
    [ "$from" -eq 0 ] && return 0

    info "Upgrading workstation setup (installed layout v${from} -> v${SETUP_VERSION})."

    if [ "$from" -lt 2 ]; then
        # v1 supervised the tunnels in the foreground of this script, so a
        # previous run may still be holding the radios and the old unit may
        # still be exporting the dongle.  Stopping the unit here (rather than
        # leaving it to install_service_unit) releases the dongle before the
        # new unit tries to bind it.
        if [ -e "${SYSTEMD_USER_DIR}/${LEGACY_SERVICE_NAME}" ]; then
            info "Stopping ${LEGACY_SERVICE_NAME} before migrating."
            systemctl --user disable --now "$LEGACY_SERVICE_NAME" >/dev/null 2>&1 || true
        fi
        # The v1 helper lived in the user's own config dir, where the user could
        # rewrite a file that sudo then ran as root.
        if [ -e "${CONFIG_DIR}/usbipd-bind.sh" ]; then
            rm -f "${CONFIG_DIR}/usbipd-bind.sh"
            info "Removed the old in-\$HOME sudo helper."
        fi
    fi
}


# ---------------------------------------------------------------------------
# The single user service
# ---------------------------------------------------------------------------
# One unit owns the whole workstation side: the usb-ip export of the Bluetooth
# dongle and both SSH tunnels.  It re-executes this script with --service, so
# there is exactly one implementation of the supervision loop.
#
# ExecStopPost releases the dongle.  It has to be systemd's job rather than the
# service's own cleanup because the bind helper runs as root via sudo, and a
# user systemd manager cannot signal a root process — stopping the unit would
# otherwise leave the dongle bound and unavailable for local Bluetooth.
install_service_unit() {
    mkdir -p "$SYSTEMD_USER_DIR" "${CONFIG_DIR}/staging"
    local unit="${SYSTEMD_USER_DIR}/${SERVICE_NAME}"
    local unit_new="${CONFIG_DIR}/staging/${SERVICE_NAME}"
    local stop_post=""
    [ -n "${BT_BUSID:-}" ] \
        && stop_post="ExecStopPost=/usr/bin/sudo -n ${RELEASE_HELPER} ${BT_BUSID}"

    {
        cat <<EOF
[Unit]
Description=remote-radios: forward workstation radios to ${SSH_TARGET}
After=network.target

[Service]
Type=simple
ExecStart=${INSTALLED_SETUP} --service ${SSH_TARGET}
EOF
        if [ -n "$stop_post" ]; then printf '%s\n' "$stop_post"; fi
        cat <<EOF
Restart=always
RestartSec=5

[Install]
WantedBy=default.target
EOF
    } > "$unit_new"

    # Retire the unit that only exported the dongle; its work is now part of
    # this one.  Stopping it also runs its own ExecStopPost, so the dongle is
    # released cleanly before the new service claims it.
    if [ -e "${SYSTEMD_USER_DIR}/${LEGACY_SERVICE_NAME}" ]; then
        systemctl --user disable --now "$LEGACY_SERVICE_NAME" >/dev/null 2>&1 || true
        rm -f "${SYSTEMD_USER_DIR}/${LEGACY_SERVICE_NAME}"
        info "Replaced ${LEGACY_SERVICE_NAME} with ${SERVICE_NAME}."
    fi

    # `enable --now` will not restart a unit that is already active, so an
    # updated ExecStart (or a newly installed helper) would otherwise sit unused
    # until the next login.  Restart only when something actually changed.
    local unit_changed=0
    cmp -s "$unit_new" "$unit" || unit_changed=1
    install -m 0644 "$unit_new" "$unit"
    rm -f "$unit_new"

    systemctl --user daemon-reload
    systemctl --user enable --now "$SERVICE_NAME"
    if [ "$unit_changed" -eq 1 ] || [ "${HELPERS_CHANGED:-0}" -eq 1 ] \
        || [ "${RUNTIME_CHANGED:-0}" -eq 1 ]; then
        systemctl --user restart "$SERVICE_NAME"
    fi
}


# ---------------------------------------------------------------------------
# VPN detection (tear down if the corporate VPN drops)
# ---------------------------------------------------------------------------
vpn_is_up() {
    # Heuristic: a tun/tap/ppp/wg interface exists AND the dev server is
    # reachable.  If the user has no VPN interface, fall back to reachability.
    if ip -o link show type tun 2>/dev/null | grep -q .; then
        return 0
    fi
    # No tunnel iface — treat SSH reachability as the liveness signal.
    ssh -o BatchMode=yes -o ClearAllForwardings=yes -o ConnectTimeout=5 "$SSH_TARGET" true 2>/dev/null
}

# ---------------------------------------------------------------------------
# Tunnels (Silabs serial + Bluetooth usb-ip)
# ---------------------------------------------------------------------------
REMOTE_UID=""
SILABS_SOCK_REMOTE=""
BT_USBIP_SOCK_REMOTE=""
SILABS_TUNNEL_PID=""
BT_TUNNEL_PID=""

resolve_remote_params() {
    REMOTE_UID=$(ssh -o BatchMode=yes -o ClearAllForwardings=yes -o ConnectTimeout=10 "$SSH_TARGET" 'id -u') \
        || die "Cannot SSH to ${SSH_TARGET} (passwordless key auth required)."
    local remote_home
    remote_home=$(ssh -o BatchMode=yes -o ClearAllForwardings=yes "$SSH_TARGET" 'echo $HOME')
    SILABS_SOCK_REMOTE="${remote_home}/.remote-radios/radios/silabs.sock"
    # The Bluetooth dongle's usbipd is reverse-tunnelled to a per-user UNIX
    # socket on the dev server (like the Silabs radio), so no sshd GatewayPorts
    # change is required.  socat inside the container bridges it to TCP for
    # `usbip attach`.
    BT_USBIP_SOCK_REMOTE="${remote_home}/.remote-radios/usbip/usbipd.sock"
    CPC_SOCK_REMOTE="${remote_home}/.remote-radios/cpc/relay.sock"
    EZSP_SOCK_REMOTE="${remote_home}/.remote-radios/zigbee/ezsp.sock"
    ZIGBEE_TOKENS_REMOTE="${remote_home}/.remote-radios/zigbee/host_token.nvm"

    # Create the per-user 0700 socket directories on the dev server BEFORE any
    # reverse tunnel starts — sshd cannot bind a StreamLocal (UNIX socket)
    # forward if its parent directory does not exist ("remote port forwarding
    # failed for listen path").
    ssh -o BatchMode=yes -o ClearAllForwardings=yes "$SSH_TARGET" \
        'mkdir -p ~/.remote-radios/radios ~/.remote-radios/usbip ~/.remote-radios/cpc ~/.remote-radios/zigbee && chmod 700 ~/.remote-radios ~/.remote-radios/radios ~/.remote-radios/usbip ~/.remote-radios/cpc ~/.remote-radios/zigbee' \
        || die "Could not create ~/.remote-radios socket dirs on ${SSH_TARGET}."

    ok "Remote UID ${REMOTE_UID}; Silabs socket ${SILABS_SOCK_REMOTE}; BT usbip socket ${BT_USBIP_SOCK_REMOTE}"
}

locate_remote_serial_py() {
    if [ -n "$SCRIPT_DIR" ] && [ -f "$SCRIPT_DIR/remote-serial.py" ]; then
        echo "$SCRIPT_DIR/remote-serial.py"; return 0
    fi
    # The service runs from ${INSTALL_DIR}, where install_runtime_files put it.
    if [ -f "${INSTALL_DIR}/remote-serial.py" ]; then
        echo "${INSTALL_DIR}/remote-serial.py"; return 0
    fi
    mkdir -p "$INSTALL_DIR"
    if curl -fsSL "${RAW_BASE}/remote-serial.py" -o "${INSTALL_DIR}/remote-serial.py" 2>/dev/null; then
        chmod 0755 "${INSTALL_DIR}/remote-serial.py"
        echo "${INSTALL_DIR}/remote-serial.py"; return 0
    fi
    echo ""
}

start_bind_helper() {
    [ -n "${BT_BUSID:-}" ] || return 0
    if [ ! -e "/sys/bus/usb/devices/${BT_BUSID}" ]; then
        warn "Bluetooth dongle ${BT_BUSID} not present; will export it when it appears."
        BIND_HELPER_PID=""
        return 0
    fi
    info "Exporting Bluetooth dongle ${BT_BUSID} over usb-ip..."
    # The helper takes an flock, so if a root-owned instance survived a previous
    # stop (a user systemd manager cannot kill one) this second copy exits at
    # once and the watchdog below simply sees it gone.  Harmless either way.
    sudo -n "$BIND_HELPER" "$BT_BUSID" >> "${STATE_DIR}/usbip-bind.log" 2>&1 &
    BIND_HELPER_PID=$!
    echo "$BIND_HELPER_PID" > "${STATE_DIR}/usbip-bind.pid"
}

start_silabs_tunnel() {
    [ -n "${SILABS_DEV:-}" ] || return 0
    local py; py="$(locate_remote_serial_py)"
    [ -n "$py" ] || { warn "remote-serial.py not found; skipping Silabs tunnel."; return 0; }

    info "Starting Silabs serial tunnel (${SILABS_DEV} -> ${SILABS_SOCK_REMOTE})..."
    python3 "$py" "$SSH_TARGET" \
        --port "$SILABS_DEV" \
        --socket "$SILABS_SOCK_REMOTE" \
        --local-port "$SILABS_LOCAL_PORT" \
        > "${STATE_DIR}/silabs-tunnel.log" 2>&1 &
    SILABS_TUNNEL_PID=$!
    echo "$SILABS_TUNNEL_PID" > "${STATE_DIR}/silabs-tunnel.pid"
}

start_bt_tunnel() {
    [ -n "${BT_BUSID:-}" ] || return 0
    info "Reverse-tunnelling usb-ip (local :${USBIPD_LOCAL_PORT} -> ${BT_USBIP_SOCK_REMOTE})..."
    # Explicitly remove any stale socket left by a prior (unclean) session before
    # binding.  StreamLocalBindUnlink alone is unreliable across sshd versions
    # and can leave "remote port forwarding failed for listen path" on restart.
    ssh -o BatchMode=yes -o ClearAllForwardings=yes -o ConnectTimeout=8 "$SSH_TARGET" \
        "rm -f ${BT_USBIP_SOCK_REMOTE}" 2>/dev/null || true
    # Reverse-tunnel the workstation's usbipd TCP port to a per-user UNIX socket
    # on the dev server.  A UNIX socket needs no sshd GatewayPorts change and is
    # reachable from the container via a bind mount + socat (unlike a loopback
    # TCP -R bind, which the container's bridge gateway cannot reach).
    ssh -N \
        -o ServerAliveInterval=15 -o ServerAliveCountMax=3 \
        -o ExitOnForwardFailure=no \
        -o StreamLocalBindUnlink=yes \
        -o ControlMaster=no -o ControlPath=none \
        -R "${BT_USBIP_SOCK_REMOTE}:127.0.0.1:${USBIPD_LOCAL_PORT}" \
        "$SSH_TARGET" \
        > "${STATE_DIR}/bt-tunnel.log" 2>&1 &
    BT_TUNNEL_PID=$!
    echo "$BT_TUNNEL_PID" > "${STATE_DIR}/bt-tunnel.pid"

    # ExitOnForwardFailure is deliberately off: the user's ssh config may carry
    # unrelated LocalForward/RemoteForward entries for this host, and a
    # collision on any one of them would otherwise kill this tunnel.  Since ssh
    # no longer fails fast for us, confirm the forward we actually care about.
    for _ in 1 2 3 4 5 6 7 8 9 10; do
        if ssh -o BatchMode=yes -o ClearAllForwardings=yes -o ConnectTimeout=8 "$SSH_TARGET" \
            "test -S ${BT_USBIP_SOCK_REMOTE}" 2>/dev/null; then
            return 0
        fi
        kill -0 "$BT_TUNNEL_PID" 2>/dev/null || break
        sleep 1
    done
    warn "usb-ip socket ${BT_USBIP_SOCK_REMOTE} did not appear; see ${STATE_DIR}/bt-tunnel.log"
}

write_remote_env_hint() {
    # Drop a small env file on the dev server so setupDockerEnv.sh / dockerw can
    # pick up the radio parameters automatically.
    ssh -o BatchMode=yes -o ClearAllForwardings=yes "$SSH_TARGET" \
        "mkdir -p ~/.remote-radios/radios ~/.remote-radios/usbip ~/.remote-radios/claims && chmod 700 ~/.remote-radios && cat > ~/.remote-radios/radios.env <<EOF
# generated by remote-radios-setup.sh on the developer workstation
SILABS_SOCKET_HOST=${SILABS_SOCK_REMOTE}
SILABS_SOCKET=/run/remote-radios/radios/silabs.sock
BT_USBIP_SOCKET_HOST=$([ -n "${BT_BUSID:-}" ] && echo "${BT_USBIP_SOCK_REMOTE}" || echo "")
BT_USBIP_SOCKET=$([ -n "${BT_BUSID:-}" ] && echo "/run/remote-radios/usbip/usbipd.sock" || echo "")
RADIO_MODE=${RADIO_MODE}
EOF" 2>/dev/null || warn "Could not write remote radios.env hint."
}

# ---------------------------------------------------------------------------
# The radio on this workstation (CPC mode)
# ---------------------------------------------------------------------------
# fetch_cpc_binaries — copy cpcd, zigbeed and libcpc from the developer's
# ZigbeeCore checkout on the dev server, which BartonCore's remote-radios
# already needs.  The dev server's checkout is the one place they are built, so
# there is nothing to build here; they are refreshed whenever they change.
# Returns non-zero when they are unavailable, leaving CPC mode off.
# clean_staging DIR — remove a fetch staging directory: its known files, then
# the directories, so nothing is removed recursively by path.
clean_staging() {
    rm -f "$1"/cpcd-workstation/bin/cpcd "$1"/cpcd-workstation/lib/libcpc.so.3 "$1"/zigbeed/debug/zigbeed
    rmdir "$1"/cpcd-workstation/bin "$1"/cpcd-workstation/lib "$1"/cpcd-workstation \
        "$1"/zigbeed/debug "$1"/zigbeed "$1" 2>/dev/null || true
}

fetch_cpc_binaries() {
    local zc
    zc=$(ssh -o BatchMode=yes -o ClearAllForwardings=yes -o ConnectTimeout=10 "$SSH_TARGET" \
        'sed -n "s/^ZIGBEECORE_DIR=//p" ~/.remote-radios/zigbee.env 2>/dev/null') || zc=""
    if [ -z "$zc" ]; then
        warn "ZigbeeCore is not set up on ${SSH_TARGET} (run scripts/remote-zigbee.sh up in a ZigbeeCore checkout there)."
        return 1
    fi

    local staging
    mkdir -p "${CONFIG_DIR}/staging" "$INSTALL_DIR"
    staging="$(mktemp -d "${CONFIG_DIR}/staging/cpc.XXXXXX")"
    # One transfer, as a tar stream, so there is a single round trip.  -h
    # follows libcpc's symlink chain to the real library.
    if ! ssh -o BatchMode=yes -o ClearAllForwardings=yes "$SSH_TARGET" \
        "cd '$zc/build' && tar -chf - cpcd-workstation/bin/cpcd cpcd-workstation/lib/libcpc.so.3 zigbeed/debug/zigbeed" \
        | tar -xf - -C "$staging" 2>/dev/null; then
        warn "Could not copy cpcd/zigbeed from ${zc}/build on ${SSH_TARGET}; build ZigbeeCore there first (its cpcd-workstation build is needed)."
        clean_staging "$staging"
        return 1
    fi

    local f src changed=0
    for f in $CPC_BINARIES; do
        case "$f" in
            cpcd) src="$staging/cpcd-workstation/bin/cpcd" ;;
            zigbeed) src="$staging/zigbeed/debug/zigbeed" ;;
            *) src="$staging/cpcd-workstation/lib/$f" ;;
        esac
        if ! cmp -s "$src" "${INSTALL_DIR}/$f"; then
            chmod 0755 "$src"
            mv -f "$src" "${INSTALL_DIR}/$f"
            changed=1
        fi
    done
    clean_staging "$staging"
    [ "$changed" = 1 ] && info "Updated cpcd and zigbeed from ${zc} on ${SSH_TARGET}."

    # They run here, built on the dev server, so check they actually load.
    if ! LD_LIBRARY_PATH="$INSTALL_DIR" "${INSTALL_DIR}/cpcd" --version >/dev/null 2>&1; then
        warn "cpcd from ${SSH_TARGET} does not run on this workstation (built on a newer system?)."
        return 1
    fi
    return 0
}

CPC_PIDS=""
CPC_TUNNEL_PID=""
CPC_SOCK_REMOTE=""
EZSP_SOCK_REMOTE=""
ZIGBEE_TOKENS_REMOTE=""

# run_supervised NAME CMD... — run CMD, restarting it with back-off while the
# service runs.  Each process logs to ${STATE_DIR}/NAME.log.
run_supervised() {
    local name="$1"; shift
    (
        # The service runs with set -e; a supervisor must survive its child
        # failing, which is exactly what it is there to handle.
        set +e
        trap 'kill "$child" 2>/dev/null; wait "$child" 2>/dev/null; exit 0' TERM INT
        backoff=2
        while true; do
            started=$SECONDS
            "$@" >> "${STATE_DIR}/${name}.log" 2>&1 &
            child=$!
            wait "$child"
            echo "[$(date -Is)] ${name} exited ($?)" >> "${STATE_DIR}/${name}.log"
            [ $((SECONDS - started)) -ge 60 ] && backoff=2
            sleep "$backoff" & wait $!
            backoff=$(( backoff * 2 < 30 ? backoff * 2 : 30 ))
        done
    ) &
    CPC_PIDS="${CPC_PIDS} $!"
}

# start_cpc_stack — run the radio here: cpcd, the relay agent for the dev
# server's otbr-agent, zigbeed, and its EZSP relay; then tunnel both relays.
start_cpc_stack() {
    [ -n "${SILABS_DEV:-}" ] || return 0
    [ -z "$CPC_PIDS" ] || return 0
    local relay="${INSTALL_DIR}/radio-relay.py"
    [ -n "$SCRIPT_DIR" ] && [ -f "${SCRIPT_DIR}/radio-relay.py" ] && relay="${SCRIPT_DIR}/radio-relay.py"

    info "Running the radio on this workstation (cpcd + zigbeed, ${SILABS_DEV})..."
    local conf="${STATE_DIR}/cpcd.conf"
    mkdir -p "$RADIO_DIR"; chmod 700 "$RADIO_DIR"
    cat > "$conf" <<EOF
# written by remote-radios-setup.sh
instance_name: ${CPC_INSTANCE}
bus_type: UART
uart_device_file: ${SILABS_DEV}
uart_device_baud: 115200
uart_hardflow: true
EOF
    # cpcd's sockets go in ${CPC_SOCKET_DIR}, relative to the working
    # directory, where the agent and zigbeed (through libcpc) find them.
    run_supervised cpcd sh -c "cd '$RADIO_DIR' && LD_LIBRARY_PATH='$INSTALL_DIR' exec '${INSTALL_DIR}/cpcd' --conf '$conf'"
    run_supervised cpc-relay python3 "$relay" agent \
        --socket-dir "$CPC_SOCKET_DIR" --listen "127.0.0.1:${CPC_RELAY_PORT}"
    run_supervised ezsp-relay python3 "$relay" ezsp \
        --link "${STATE_DIR}/ttyZigbeeNCP" --listen "127.0.0.1:${EZSP_RELAY_PORT}"
    # zigbeed keeps the Zigbee network's tokens in its working directory, so
    # the network follows this workstation.  It waits for cpcd and its tty.
    run_supervised zigbeed sh -c "
        until [ -S '${CPC_SOCKET_DIR}/ctrl.cpcd.sock' ] && [ -e '${STATE_DIR}/ttyZigbeeNCP' ]; do sleep 1; done
        cd '$RADIO_DIR' && LD_LIBRARY_PATH='$INSTALL_DIR' exec '${INSTALL_DIR}/zigbeed' \
            -r 'spinel+cpc://${CPC_INSTANCE}?iid=1&iid-list=0' -d 2 -p '${STATE_DIR}/ttyZigbeeNCP'"
    # ZigbeeCore learns that the network's configuration changed by watching
    # zigbeed's tokens file, which is here rather than beside it.  Mirror each
    # change as a marker of the same name in its directory on the dev server;
    # it then reads the configuration from zigbeed itself.  The marker's
    # contents are irrelevant, but writing them (rather than touching it) is
    # what ZigbeeCore's inotify watch reacts to.
    # Each report makes ZigbeeCore read every token from zigbeed over EZSP,
    # dozens of round trips through the tunnel; over a slow link that blocks it
    # for many seconds.  A join writes the tokens several times in quick
    # succession, so wait for a minute of quiet, by which time pairing is over.
    run_supervised tokens-notify python3 "$relay" notify --settle 60 \
        --file "${RADIO_DIR}/host_token.nvm" -- \
        ssh -o BatchMode=yes -o ClearAllForwardings=yes -o ConnectTimeout=8 "$SSH_TARGET" \
        "date +%s%N > ${ZIGBEE_TOKENS_REMOTE}"

    start_cpc_tunnel
}

# start_cpc_tunnel — one ssh carrying both relays to per-user sockets on the
# dev server: cpc/relay.sock (otbr-agent's cpcd sockets) and zigbee/ezsp.sock.
start_cpc_tunnel() {
    ssh -o BatchMode=yes -o ClearAllForwardings=yes -o ConnectTimeout=8 "$SSH_TARGET" \
        "rm -f ${CPC_SOCK_REMOTE} ${EZSP_SOCK_REMOTE}" 2>/dev/null || true
    ssh -N \
        -o ServerAliveInterval=15 -o ServerAliveCountMax=3 \
        -o ExitOnForwardFailure=no \
        -o StreamLocalBindUnlink=yes \
        -o ControlMaster=no -o ControlPath=none \
        -R "${CPC_SOCK_REMOTE}:127.0.0.1:${CPC_RELAY_PORT}" \
        -R "${EZSP_SOCK_REMOTE}:127.0.0.1:${EZSP_RELAY_PORT}" \
        "$SSH_TARGET" \
        > "${STATE_DIR}/cpc-tunnel.log" 2>&1 &
    CPC_TUNNEL_PID=$!
}

# stop_cpc_stack — stop everything start_cpc_stack started.  The dev server's
# sockets are left to the caller (the lease's release removes them).
stop_cpc_stack() {
    local p
    [ -n "$CPC_TUNNEL_PID" ] && { kill "$CPC_TUNNEL_PID" 2>/dev/null || true; }
    for p in $CPC_PIDS; do kill "$p" 2>/dev/null || true; done
    # wait reports a killed process as failed (143), which set -e would turn
    # into the whole service exiting.
    for p in $CPC_PIDS $CPC_TUNNEL_PID; do wait "$p" 2>/dev/null || true; done
    CPC_PIDS=""; CPC_TUNNEL_PID=""
}

# start_radio / stop_radio — whichever form of forwarding RADIO_MODE selects.
start_radio() {
    # The radios container picks its mode from which socket exists, so the
    # other mode's socket must not be left behind (by a mode switch or by a
    # previous run).
    local stale="$SILABS_SOCK_REMOTE"
    [ "$RADIO_MODE" = cpc ] || stale="${CPC_SOCK_REMOTE} ${EZSP_SOCK_REMOTE}"
    ssh -o BatchMode=yes -o ConnectTimeout=8 -o ClearAllForwardings=yes "$SSH_TARGET" \
        "rm -f ${stale}" 2>/dev/null || true
    if [ "$RADIO_MODE" = cpc ]; then start_cpc_stack; else start_silabs_tunnel; fi
}
stop_radio() {
    stop_cpc_stack
    if [ -n "${SILABS_TUNNEL_PID:-}" ]; then
        kill "$SILABS_TUNNEL_PID" 2>/dev/null || true
        wait "$SILABS_TUNNEL_PID" 2>/dev/null || true
        SILABS_TUNNEL_PID=""
    fi
}

# ---------------------------------------------------------------------------
# Ownership lease
# ---------------------------------------------------------------------------
# A developer may set up more than one workstation (an office machine and a
# laptop, say), but every one of them forwards to the same per-user sockets on
# the dev server, so only one can forward at a time.  Two at once delete and
# re-create each other's sockets in a loop, and can leave the dev server's
# Bluetooth adapter half-initialised.
#
# The forwarding workstation therefore holds a lease, ~/.remote-radios/owner on
# the dev server, and refreshes it while it runs.  Any other workstation stands
# by — no tunnels, and its dongle stays with its own Bluetooth — until the
# lease is released (the owner stopped) or goes stale (the owner vanished
# without stopping, e.g. a closed laptop), then takes over.  `--take-over`
# claims it immediately; the previous owner notices on its next refresh.
#
# The lease is updated under flock on the dev server, and timestamps come from
# the dev server's clock, so workstations' clocks need not agree.
LEASE_RENEW_SECS=30
LEASE_STALE_SECS=120
LEASE_HOLDER=""
LEASE_RENEWED_AT=0

# A stable identity for this workstation, and a readable label for messages.
MACHINE_ID="$(tr -cd 'A-Za-z0-9' < /etc/machine-id 2>/dev/null)"
[ -n "$MACHINE_ID" ] || MACHINE_ID="$(hostname | tr -cd 'A-Za-z0-9._-')"
HOST_LABEL="$(hostname -s 2>/dev/null | tr -cd 'A-Za-z0-9._-')"
[ -n "$HOST_LABEL" ] || HOST_LABEL="$MACHINE_ID"

# Runs on the dev server.  Args: mode id host stale [paths to remove on release]
#   acquire  take or refresh the lease unless another live workstation holds it;
#            then print MISSING <path> for each given path that is not a socket
#   force    take it regardless (--take-over)
#   release  give it up, and remove the given paths, only if we hold it
# Prints OK / TOOK <prev-host> / HELD <host> <since> <age> / RELEASED.
# Exit: 0 held by us (or released), 2 held by another, 3 could not lock.
LEASE_REMOTE_SCRIPT='
set -u
mode=$1; id=$2; host=$3; stale=$4; shift 4
dir="$HOME/.remote-radios"; f="$dir/owner"
mkdir -p "$dir" && chmod 700 "$dir"
exec 9>"$dir/.owner.lock"
flock -w 10 9 || { echo LOCKFAIL; exit 3; }
now=$(date +%s)
cur_id=""; cur_host=""; cur_since=""; cur_hb=0
if [ -f "$f" ]; then
    cur_id=$(sed -n "s/^machine_id=//p" "$f")
    cur_host=$(sed -n "s/^host=//p" "$f")
    cur_since=$(sed -n "s/^since=//p" "$f")
    cur_hb=$(sed -n "s/^heartbeat=//p" "$f")
fi
case "$cur_hb" in ""|*[!0-9]*) cur_hb=0 ;; esac
if [ "$mode" = release ]; then
    if [ "$cur_id" = "$id" ]; then
        rm -f "$f" "$dir/radios.env" "$@"
    fi
    echo RELEASED; exit 0
fi
age=$((now - cur_hb))
if [ -n "$cur_id" ] && [ "$cur_id" != "$id" ] && [ "$age" -lt "$stale" ] && [ "$mode" != force ]; then
    echo "HELD ${cur_host:-unknown} ${cur_since:-unknown} $age"; exit 2
fi
since=$cur_since
[ "$cur_id" = "$id" ] || since=$(date -u +%Y-%m-%dT%H:%M:%SZ)
printf "machine_id=%s\nhost=%s\nsince=%s\nheartbeat=%s\n" "$id" "$host" "$since" "$now" > "$f.tmp.$$"
mv -f "$f.tmp.$$" "$f"
if [ -n "$cur_id" ] && [ "$cur_id" != "$id" ]; then echo "TOOK ${cur_host:-unknown}"; else echo OK; fi
if [ "$mode" = acquire ]; then
    for p in "$@"; do [ -S "$p" ] || echo "MISSING $p"; done
fi
# An hh4 guest has claimed the radio (see check_radio_mode).
if [ -e "$dir/claims/silabs.claim" ]; then echo CLAIMED; fi
if [ -e "$dir/zigbee.env" ]; then echo ZIGBEECORE; fi
exit 0
'

# lease_op <mode> [remote paths...] — run the lease script on the dev server.
# Sets LEASE_HOLDER (and LEASE_REPLY); returns the script's exit status, or
# 255 when the dev server could not be reached.
lease_op() {
    local mode="$1"; shift
    local args="" a
    # The arguments pass through the dev server's login shell, which may not be
    # bash; the values are identifiers and paths, so single quotes suffice.
    for a in "$mode" "$MACHINE_ID" "$HOST_LABEL" "$LEASE_STALE_SECS" "$@"; do
        case "$a" in *\'*) warn "Refusing lease argument containing a quote: $a"; return 3 ;; esac
        args="$args '$a'"
    done
    # No forwards on this connection: the user's ssh config may define some
    # for this host, and they are not ours to set up.
    # Under `set -e` a failing command substitution would end the script, so
    # the status is captured explicitly; callers must do the same.
    local rc=0
    LEASE_REPLY="$(ssh -o BatchMode=yes -o ConnectTimeout=8 -o ClearAllForwardings=yes \
        "$SSH_TARGET" "bash -s --${args}" <<< "$LEASE_REMOTE_SCRIPT" 2>/dev/null)" || rc=$?
    LEASE_HOLDER=""
    case "$LEASE_REPLY" in
        HELD\ *) LEASE_HOLDER="$(set -- $LEASE_REPLY; echo "$2 (forwarding since $3)")" ;;
        TOOK\ *) LEASE_HOLDER="$(set -- $LEASE_REPLY; echo "$2")" ;;
    esac
    [ "$rc" -eq 0 ] && LEASE_RENEWED_AT=$SECONDS
    return $rc
}

# wait_for_lease — block until this workstation holds the lease.
wait_for_lease() {
    local announced="" unreachable=""
    local rc
    while true; do
        rc=0
        lease_op acquire || rc=$?
        case $rc in
            0)
                case "$LEASE_REPLY" in
                    TOOK*) ok "Took over the radios from ${LEASE_HOLDER}, which stopped refreshing its lease." ;;
                esac
                [ -n "$announced" ] && ok "This workstation now forwards the radios to ${SSH_TARGET}."
                return 0
                ;;
            2)
                if [ "$announced" != "$LEASE_HOLDER" ]; then
                    info "Radios for ${SSH_TARGET} are being forwarded from ${LEASE_HOLDER}; standing by."
                    info "To use this workstation instead, stop remote-radios.service there, or run:"
                    info "    ${INSTALLED_SETUP} --take-over"
                    announced="$LEASE_HOLDER"
                fi
                ;;
            *)
                if [ -z "$unreachable" ]; then
                    warn "Could not check radio ownership on ${SSH_TARGET}; retrying."
                    unreachable=1
                fi
                ;;
        esac
        sleep 15
    done
}

# release_dongle — hand the Bluetooth dongle back to this workstation.
release_dongle() {
    [ -n "${BT_BUSID:-}" ] || return 0
    sudo -n "$RELEASE_HELPER" "$BT_BUSID" >> "${STATE_DIR}/usbip-bind.log" 2>&1 || true
    BIND_HELPER_PID=""
}

# stand_down — another workstation has taken the lease: stop forwarding without
# touching the sockets on the dev server, which are now the new owner's.
stand_down() {
    warn "${LEASE_HOLDER:-Another workstation} has taken over forwarding the radios; standing down."
    if [ -n "${SILABS_TUNNEL_PID:-}" ]; then
        kill -USR1 "$SILABS_TUNNEL_PID" 2>/dev/null || true
        wait "$SILABS_TUNNEL_PID" 2>/dev/null || true
        SILABS_TUNNEL_PID=""
    fi
    stop_cpc_stack
    if [ -n "${BT_TUNNEL_PID:-}" ]; then
        kill "$BT_TUNNEL_PID" 2>/dev/null || true
        BT_TUNNEL_PID=""
    fi
    release_dongle
}

# start_forwarding — export the dongle and bring up both tunnels.
start_forwarding() {
    start_bind_helper
    start_radio
    start_bt_tunnel
    write_remote_env_hint
}

# ---------------------------------------------------------------------------
# Teardown + monitor
# ---------------------------------------------------------------------------
teardown() {
    info "Tearing down remote-radios tunnels..."
    stop_radio
    [ -n "${BT_TUNNEL_PID:-}" ] && kill "$BT_TUNNEL_PID" 2>/dev/null || true
    # Give up the lease, removing the remote sockets and env hint — but only if
    # this workstation still holds it; otherwise they belong to the new owner.
    if takeover_pending; then
        info "Keeping the lease for the take-over restart."
    else
        lease_op release ${SILABS_SOCK_REMOTE:+"$SILABS_SOCK_REMOTE"} \
            ${BT_USBIP_SOCK_REMOTE:+"$BT_USBIP_SOCK_REMOTE"} \
            ${CPC_SOCK_REMOTE:+"$CPC_SOCK_REMOTE"} ${EZSP_SOCK_REMOTE:+"$EZSP_SOCK_REMOTE"} || true
    fi
    ok "Teardown complete."
}

# check_remote_sockets — after a renewal, restart any tunnel whose socket on
# the dev server is gone.  Two misses in a row are required, so a tunnel that
# is still connecting is left alone.
SILABS_SOCK_MISSES=0
BT_SOCK_MISSES=0
check_remote_sockets() {
    if [ -n "${SILABS_TUNNEL_PID:-}" ] && grep -qxF "MISSING ${SILABS_SOCK_REMOTE}" <<< "$LEASE_REPLY"; then
        SILABS_SOCK_MISSES=$((SILABS_SOCK_MISSES + 1))
        if [ "$SILABS_SOCK_MISSES" -ge 2 ]; then
            warn "Silabs socket ${SILABS_SOCK_REMOTE} is gone from ${SSH_TARGET} — restarting its tunnel."
            kill "$SILABS_TUNNEL_PID" 2>/dev/null || true
            wait "$SILABS_TUNNEL_PID" 2>/dev/null || true
            SILABS_SOCK_MISSES=0
            start_silabs_tunnel
        fi
    else
        SILABS_SOCK_MISSES=0
    fi
    if [ -n "${BT_TUNNEL_PID:-}" ] && grep -qxF "MISSING ${BT_USBIP_SOCK_REMOTE}" <<< "$LEASE_REPLY"; then
        BT_SOCK_MISSES=$((BT_SOCK_MISSES + 1))
        if [ "$BT_SOCK_MISSES" -ge 2 ]; then
            warn "usb-ip socket ${BT_USBIP_SOCK_REMOTE} is gone from ${SSH_TARGET} — restarting its tunnel."
            kill "$BT_TUNNEL_PID" 2>/dev/null || true
            wait "$BT_TUNNEL_PID" 2>/dev/null || true
            BT_SOCK_MISSES=0
            start_bt_tunnel
        fi
    else
        BT_SOCK_MISSES=0
    fi
    if [ -n "${CPC_TUNNEL_PID:-}" ] && grep -qE "^MISSING (${CPC_SOCK_REMOTE}|${EZSP_SOCK_REMOTE})$" <<< "$LEASE_REPLY"; then
        CPC_SOCK_MISSES=$((CPC_SOCK_MISSES + 1))
        if [ "$CPC_SOCK_MISSES" -ge 2 ]; then
            warn "Radio relay sockets are gone from ${SSH_TARGET} — restarting their tunnel."
            kill "$CPC_TUNNEL_PID" 2>/dev/null || true
            wait "$CPC_TUNNEL_PID" 2>/dev/null || true
            CPC_SOCK_MISSES=0
            start_cpc_tunnel
        fi
    else
        CPC_SOCK_MISSES=0
    fi
}
CPC_SOCK_MISSES=0

# check_radio_mode — follow hh4's claim on the radio.  A QEMU guest runs its
# own cpcd, so while its claim exists the raw serial port is forwarded instead
# of running the radio here; when the claim clears, the radio comes back here.
check_radio_mode() {
    local want=cpc newly_available=0
    grep -qx CLAIMED <<< "$LEASE_REPLY" && want=serial
    [ "$want" = "$RADIO_MODE" ] && return 0
    if [ "$want" = cpc ] && [ "${CPC_AVAILABLE:-0}" != 1 ]; then
        # ZigbeeCore may have been set up since this service started.  Its
        # hint file is reported with the lease, so fetching zigbeed is only
        # attempted once it exists, and at most every 10 minutes after a
        # failed attempt (a hint without a build, say).
        if [ -n "${SILABS_SERIAL:-}" ] && grep -qx ZIGBEECORE <<< "$LEASE_REPLY" \
            && [ "$(date +%s)" -ge "${CPC_FETCH_RETRY_AT:-0}" ]; then
            if fetch_cpc_binaries; then
                CPC_AVAILABLE=1 newly_available=1
                info "ZigbeeCore is now set up on the dev server; running the radio on this workstation."
            else
                CPC_FETCH_RETRY_AT=$(( $(date +%s) + 600 ))
            fi
        fi
        if [ "${CPC_AVAILABLE:-0}" != 1 ]; then
            want=serial
            [ "$want" = "$RADIO_MODE" ] && return 0
        fi
    fi
    if [ "$want" = serial ]; then
        info "An hh4 guest has claimed the radio; forwarding its serial port instead."
    elif [ "$newly_available" = 0 ]; then
        info "The radio's claim has cleared; running the radio on this workstation again."
    fi
    stop_radio
    RADIO_MODE="$want"
    start_radio
    write_remote_env_hint
}

monitor_loop() {
    info "Supervising radios and tunnels. Tears down on VPN drop or stop."
    trap 'teardown; exit 0' INT TERM

    while true; do
        sleep 10

        if ! vpn_is_up; then
            warn "VPN/dev-server unreachable — tearing down and waiting for it to return..."
            teardown
            # Wait for VPN to come back, then re-establish.
            while ! vpn_is_up; do sleep 10; done
            info "Connectivity restored — re-establishing tunnels..."
            resolve_remote_params
            # Another workstation may have taken over while this one was away.
            wait_for_lease
            start_radio
            start_bt_tunnel
            write_remote_env_hint
            continue
        fi

        # Keep the lease fresh, and give way if another workstation has taken
        # it (`--take-over`, or this one was away long enough for it to go
        # stale).  An unreachable dev server is left to the VPN check above.
        # The renewal also checks that our sockets still exist there: if
        # something removed one while its ssh stays connected, nothing would
        # otherwise notice, and this workstation would keep the lease while
        # forwarding nothing.
        if [ $((SECONDS - LEASE_RENEWED_AT)) -ge "$LEASE_RENEW_SECS" ]; then
            local rc=0
            lease_op acquire \
                ${SILABS_TUNNEL_PID:+${SILABS_SOCK_REMOTE:+"$SILABS_SOCK_REMOTE"}} \
                ${BT_TUNNEL_PID:+${BT_USBIP_SOCK_REMOTE:+"$BT_USBIP_SOCK_REMOTE"}} \
                ${CPC_TUNNEL_PID:+"$CPC_SOCK_REMOTE" "$EZSP_SOCK_REMOTE"} || rc=$?
            if [ "$rc" -eq 2 ]; then
                stand_down
                wait_for_lease
                start_forwarding
                continue
            fi
            if [ "$rc" -eq 0 ]; then
                check_remote_sockets
                check_radio_mode
            fi
        fi

        # Hot-plug recovery.  The service starts at login, which is routinely
        # before the radios are plugged in, so a missing device is a normal
        # state to recover from rather than an error.
        if [ -n "${SILABS_SERIAL:-}" ] && [ -z "${SILABS_DEV:-}" ]; then
            SILABS_DEV="$(resolve_silabs_dev "$SILABS_SERIAL")"
            if [ -n "$SILABS_DEV" ]; then
                ok "Silabs radio appeared at ${SILABS_DEV}."
                start_radio
            fi
        fi
        if [ -n "${BT_BUSID:-}" ] && [ -z "${BIND_HELPER_PID:-}" ] \
            && [ -e "/sys/bus/usb/devices/${BT_BUSID}" ]; then
            ok "Bluetooth dongle ${BT_BUSID} appeared."
            start_bind_helper
        fi

        # Keep the dongle exported.  The helper re-binds the device itself if
        # the driver reverts; this covers the helper process going away.
        if [ -n "${BIND_HELPER_PID:-}" ] && ! kill -0 "$BIND_HELPER_PID" 2>/dev/null; then
            if [ -e "/sys/bus/usb/devices/${BT_BUSID}" ]; then
                warn "usb-ip export helper died — restarting."
                start_bind_helper
            else
                warn "Bluetooth dongle ${BT_BUSID} was unplugged."
                BIND_HELPER_PID=""
            fi
        fi

        # Restart any tunnel process that died.
        if [ -n "${SILABS_DEV:-}" ] && [ -n "${SILABS_TUNNEL_PID:-}" ] \
            && ! kill -0 "$SILABS_TUNNEL_PID" 2>/dev/null; then
            # Distinguish an unplugged radio from a dropped tunnel: retrying a
            # tunnel to a device node that no longer exists just spins.
            if [ -e "$SILABS_DEV" ]; then
                warn "Silabs tunnel died — restarting."
                start_silabs_tunnel
            else
                warn "Silabs radio ${SILABS_DEV} was unplugged; waiting for it to return."
                SILABS_DEV=""
                SILABS_TUNNEL_PID=""
            fi
        fi
        if [ -n "${BT_BUSID:-}" ] && [ -n "${BT_TUNNEL_PID:-}" ] \
            && ! kill -0 "$BT_TUNNEL_PID" 2>/dev/null; then
            warn "Bluetooth usb-ip tunnel died — restarting."
            start_bt_tunnel
        fi
        # CPC mode: the tunnel, and noticing an unplugged radio (cpcd itself is
        # restarted by its supervisor, which would otherwise spin).
        if [ -n "${CPC_TUNNEL_PID:-}" ] && ! kill -0 "$CPC_TUNNEL_PID" 2>/dev/null; then
            warn "Radio relay tunnel died — restarting."
            start_cpc_tunnel
        fi
        if [ -n "$CPC_PIDS" ] && [ -n "${SILABS_DEV:-}" ] && [ ! -e "$SILABS_DEV" ]; then
            warn "Silabs radio ${SILABS_DEV} was unplugged; waiting for it to return."
            stop_cpc_stack
            SILABS_DEV=""
        fi
    done
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
usage() {
    cat <<EOF
Usage: ${SCRIPT_NAME} <user>@<devserver>

Sets up this workstation to forward its Silabs radio and Bluetooth dongle to a
shared dev server, and installs a user service that keeps them forwarded.  The
service starts at login and stops at logout; re-run this script only to change
the dev server or to pick up an updated checkout.

Options:
  --take-over [<user>@<devserver>]
                                 Forward from this workstation now, taking over
                                 from another of your workstations that is
                                 currently forwarding.  Without this, a second
                                 workstation waits until the first stops (or has
                                 been unreachable for ${LEASE_STALE_SECS}s).
  --service <user>@<devserver>   Run as the service itself (used by the unit;
                                 not normally invoked by hand).
  -h, --help                     Show this help.
EOF
}

# take_over — make this workstation the one forwarding the radios now, rather
# than waiting for the current owner to stop or go stale.
# takeover_pending — a --take-over restart is in progress.  Only a recent marker
# counts, so one left behind by an interrupted take-over cannot keep this
# workstation from ever releasing the lease.
takeover_pending() {
    [ -f "$TAKEOVER_MARKER" ] && [ -n "$(find "$TAKEOVER_MARKER" -mmin -5 2>/dev/null)" ]
}

take_over() {
    load_config
    SSH_TARGET="${1:-${SSH_TARGET:-}}"
    [ -n "$SSH_TARGET" ] || die "No dev server configured; run ${SCRIPT_NAME} <user>@<devserver> first."
    [ -f "${SYSTEMD_USER_DIR}/${SERVICE_NAME}" ] \
        || die "${SERVICE_NAME} is not installed here; run ${SCRIPT_NAME} <user>@<devserver> first."

    lease_op force || die "Could not reach ${SSH_TARGET} to take over the radios."

    case "$LEASE_REPLY" in
        TOOK*)
            ok "Took over the radios from ${LEASE_HOLDER}."
            # Let the previous owner notice and stand down before this one
            # creates its sockets, so the two never forward at the same time.
            local settle=$((LEASE_RENEW_SECS + 15))
            info "Waiting ${settle}s for ${LEASE_HOLDER} to stand down..."
            sleep "$settle"
            ;;
        *) ok "This workstation already holds the radios for ${SSH_TARGET}." ;;
    esac

    mkdir -p "$STATE_DIR"; chmod 700 "$STATE_DIR"
    : > "$TAKEOVER_MARKER"
    systemctl --user restart "$SERVICE_NAME"
    ok "remote-radios.service restarted; it forwards from this workstation."
}

# Resolve the radios named in the saved config, without prompting.
resolve_saved_radios() {
    if [ -n "${SILABS_SERIAL:-}" ]; then
        SILABS_DEV="$(resolve_silabs_dev "$SILABS_SERIAL")"
    fi
}

# The service: no prompting, no privileged installation — just bring the radios
# up and supervise them.  A radio that is absent right now is not fatal; the
# monitor loop picks it up when it is plugged in.
run_as_service() {
    load_config
    [ -n "${SSH_TARGET:-}" ] || die "No dev server configured; run ${SCRIPT_NAME} <user>@<devserver> first."
    resolve_saved_radios

    [ -n "${SILABS_SERIAL:-}${BT_BUSID:-}" ] \
        || die "No radios configured; run ${SCRIPT_NAME} <user>@<devserver> first."

    [ -n "${SILABS_DEV:-}" ] && ok "Silabs radio: ${SILABS_DEV}"
    resolve_remote_params
    # Only one of the developer's workstations forwards at a time; until this
    # one holds the lease it leaves the dongle alone and opens no tunnels.
    if takeover_pending; then
        # Reassert the lease --take-over just took, rather than queue behind a
        # workstation that may have grabbed it during the restart.
        rm -f "$TAKEOVER_MARKER"
        lease_op force || wait_for_lease
    else
        wait_for_lease
    fi
    # Run the radio here unless the binaries are unavailable, or an hh4 guest
    # holds the radio's claim; either way the serial port is forwarded instead.
    CPC_AVAILABLE=0
    if [ -n "${SILABS_SERIAL:-}" ] && fetch_cpc_binaries; then CPC_AVAILABLE=1; fi
    RADIO_MODE=cpc
    if [ "$CPC_AVAILABLE" != 1 ] || grep -qx CLAIMED <<< "${LEASE_REPLY:-}"; then
        RADIO_MODE=serial
    fi
    info "Radio mode: ${RADIO_MODE}."
    start_forwarding
    ok "Remote radios are forwarded to ${SSH_TARGET}."
    monitor_loop
}

main() {
    require_cmds
    mkdir -p "$STATE_DIR"; chmod 700 "$STATE_DIR"

    case "${1:-}" in
        -h|--help) usage; exit 0 ;;
        --service) SERVICE_MODE=1; shift ;;
        --take-over) shift; take_over "$@"; exit $? ;;
    esac

    if [ "$SERVICE_MODE" -eq 1 ]; then
        # The unit passes the target explicitly; fall back to the saved one.
        load_config
        resolve_raw_base
        SSH_TARGET="${1:-${SSH_TARGET:-}}"
        run_as_service
        return
    fi

    load_config
    SSH_TARGET="${1:-${SSH_TARGET:-}}"
    [ -n "$SSH_TARGET" ] || { usage; exit 1; }

    # Retire anything left by an older layout before touching the new one.
    migrate_installed_state

    # The saved config may name the ref this workstation tracks.
    resolve_raw_base

    # Refresh the installed runtime up front. If this process is itself the
    # installed copy and a newer version exists, the rest of the setup — and
    # any migrations it carries — is performed by that newer version.
    install_runtime_files
    maybe_reexec_updated_setup "$@"

    # The installed layout is now current, so record it here rather than at the
    # end.  Setup can legitimately stop before it finishes — most often to wait
    # for a new login session to pick up the group — and a run that got this far
    # must not be mistaken for an older layout and "migrated" on the next pass.
    record_version

    # Detect radios on first run, or re-validate saved ones.
    if [ -z "${SILABS_SERIAL:-}${BT_BUSID:-}" ]; then
        detect_radios
        save_config
    else
        info "Using saved radio config (${CONFIG_FILE})."
        # Re-resolve the Silabs device node from its stable serial.
        if [ -n "${SILABS_SERIAL:-}" ]; then
            SILABS_DEV="$(resolve_silabs_dev "$SILABS_SERIAL")"
            if [ -z "$SILABS_DEV" ]; then
                warn "Saved Silabs radio (serial ${SILABS_SERIAL}) not found — re-detecting."
                detect_radios; save_config
            else
                ok "Silabs radio: ${SILABS_DEV}"
            fi
        fi
        # Validate the BT dongle is still present.
        if [ -n "${BT_BUSID:-}" ] && [ ! -e "/sys/bus/usb/devices/${BT_BUSID}" ]; then
            warn "Saved Bluetooth dongle (busid ${BT_BUSID}) not found — re-detecting."
            detect_radios; save_config
        fi
    fi

    # Persist the dev server so the service can run without arguments.
    save_config

    # Verify we can actually reach the dev server before installing anything
    # that claims to keep a connection to it.
    resolve_remote_params

    install_privileged_helpers
    install_service_unit

    echo
    ok "Remote radios are set up, and will be forwarded automatically at login."
    info "Service:  systemctl --user status ${SERVICE_NAME}"
    info "Logs:     journalctl --user -u ${SERVICE_NAME} -f"
    echo
    info "On the dev server, run your devcontainer with the radio overlay:"
    printf '    %s./dockerw -T bash%s\n' "$BOLD" "$NC"
    echo
}

main "$@"

