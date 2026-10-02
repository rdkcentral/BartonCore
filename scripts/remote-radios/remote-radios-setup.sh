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

# Where to fetch those files when there is no checkout to copy them from.
RAW_REPO="${REMOTE_RADIOS_REPO:-rdkcentral/BartonCore}"
RAW_REF="${REMOTE_RADIOS_REF:-main}"
RAW_BASE="${REMOTE_RADIOS_RAW_BASE:-https://raw.githubusercontent.com/${RAW_REPO}/${RAW_REF}/scripts/remote-radios}"

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
# automatically when the holder dies, so it never goes stale.
mkdir -p /run/remote-radios
exec 9>"/run/remote-radios/usbipd-bind-${BUSID}.lock"
if ! flock -n 9; then
    log "another instance is already managing ${BUSID}; exiting."
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
        usbipd -D
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

    local f tmp
    for f in "$SCRIPT_NAME" remote-serial.py; do
        tmp="${staging}/${f}"
        if [ -n "$SCRIPT_DIR" ] && [ "$SCRIPT_DIR" != "$INSTALL_DIR" ] \
            && [ -f "${SCRIPT_DIR}/${f}" ]; then
            cp -- "${SCRIPT_DIR}/${f}" "$tmp"
        elif [ "$SCRIPT_DIR" = "$INSTALL_DIR" ] && [ -f "${INSTALL_DIR}/${f}" ]; then
            # Re-run of the already-installed copy; nothing to refresh.
            continue
        else
            command -v curl >/dev/null 2>&1 \
                || die "curl is required to install ${f}; install curl or run from a checkout."
            curl -fsSL "${RAW_BASE}/${f}" -o "$tmp" || die \
                "Could not download ${f} from ${RAW_BASE}.
       Check the ref exists, or set REMOTE_RADIOS_REF / REMOTE_RADIOS_RAW_BASE."
        fi
        if ! cmp -s "$tmp" "${INSTALL_DIR}/${f}"; then
            install -m 0755 "$tmp" "${INSTALL_DIR}/${f}"
            RUNTIME_CHANGED=1
        fi
        rm -f "$tmp"
    done

    [ -s "$INSTALLED_SETUP" ] || die "Runtime install failed: ${INSTALLED_SETUP} is missing."
    ok "Workstation runtime installed in ${INSTALL_DIR}"
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
    ssh -o BatchMode=yes -o ConnectTimeout=5 "$SSH_TARGET" true 2>/dev/null
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
    REMOTE_UID=$(ssh -o BatchMode=yes -o ConnectTimeout=10 "$SSH_TARGET" 'id -u') \
        || die "Cannot SSH to ${SSH_TARGET} (passwordless key auth required)."
    local remote_home
    remote_home=$(ssh -o BatchMode=yes "$SSH_TARGET" 'echo $HOME')
    SILABS_SOCK_REMOTE="${remote_home}/.remote-radios/radios/silabs.sock"
    # The Bluetooth dongle's usbipd is reverse-tunnelled to a per-user UNIX
    # socket on the dev server (like the Silabs radio), so no sshd GatewayPorts
    # change is required.  socat inside the container bridges it to TCP for
    # `usbip attach`.
    BT_USBIP_SOCK_REMOTE="${remote_home}/.remote-radios/usbip/usbipd.sock"

    # Create the per-user 0700 socket directories on the dev server BEFORE any
    # reverse tunnel starts — sshd cannot bind a StreamLocal (UNIX socket)
    # forward if its parent directory does not exist ("remote port forwarding
    # failed for listen path").
    ssh -o BatchMode=yes "$SSH_TARGET" \
        'mkdir -p ~/.remote-radios/radios ~/.remote-radios/usbip && chmod 700 ~/.remote-radios ~/.remote-radios/radios ~/.remote-radios/usbip' \
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
    ssh -o BatchMode=yes -o ConnectTimeout=8 "$SSH_TARGET" \
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
        if ssh -o BatchMode=yes -o ConnectTimeout=8 "$SSH_TARGET" \
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
    ssh -o BatchMode=yes "$SSH_TARGET" \
        "mkdir -p ~/.remote-radios/radios ~/.remote-radios/usbip ~/.remote-radios/claims && chmod 700 ~/.remote-radios && cat > ~/.remote-radios/radios.env <<EOF
# generated by remote-radios-setup.sh on the developer workstation
SILABS_SOCKET_HOST=${SILABS_SOCK_REMOTE}
SILABS_SOCKET=/run/remote-radios/radios/silabs.sock
BT_USBIP_SOCKET_HOST=$([ -n "${BT_BUSID:-}" ] && echo "${BT_USBIP_SOCK_REMOTE}" || echo "")
BT_USBIP_SOCKET=$([ -n "${BT_BUSID:-}" ] && echo "/run/remote-radios/usbip/usbipd.sock" || echo "")
EOF" 2>/dev/null || warn "Could not write remote radios.env hint."
}

# ---------------------------------------------------------------------------
# Teardown + monitor
# ---------------------------------------------------------------------------
teardown() {
    info "Tearing down remote-radios tunnels..."
    [ -n "${SILABS_TUNNEL_PID:-}" ] && kill "$SILABS_TUNNEL_PID" 2>/dev/null || true
    [ -n "${BT_TUNNEL_PID:-}" ] && kill "$BT_TUNNEL_PID" 2>/dev/null || true
    # Remove remote sockets + env hint.
    ssh -o BatchMode=yes -o ConnectTimeout=5 "$SSH_TARGET" \
        "rm -f ${SILABS_SOCK_REMOTE} ${BT_USBIP_SOCK_REMOTE:-} ~/.remote-radios/radios.env" 2>/dev/null || true
    ok "Teardown complete."
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
            start_silabs_tunnel
            start_bt_tunnel
            write_remote_env_hint
            continue
        fi

        # Hot-plug recovery.  The service starts at login, which is routinely
        # before the radios are plugged in, so a missing device is a normal
        # state to recover from rather than an error.
        if [ -n "${SILABS_SERIAL:-}" ] && [ -z "${SILABS_DEV:-}" ]; then
            SILABS_DEV="$(resolve_silabs_dev "$SILABS_SERIAL")"
            if [ -n "$SILABS_DEV" ]; then
                ok "Silabs radio appeared at ${SILABS_DEV}."
                start_silabs_tunnel
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
  --service <user>@<devserver>   Run as the service itself (used by the unit;
                                 not normally invoked by hand).
  -h, --help                     Show this help.
EOF
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
    start_bind_helper
    resolve_remote_params
    start_silabs_tunnel
    start_bt_tunnel
    write_remote_env_hint
    ok "Remote radios are forwarded to ${SSH_TARGET}."
    monitor_loop
}

main() {
    require_cmds
    mkdir -p "$STATE_DIR"; chmod 700 "$STATE_DIR"

    case "${1:-}" in
        -h|--help) usage; exit 0 ;;
        --service) SERVICE_MODE=1; shift ;;
    esac

    if [ "$SERVICE_MODE" -eq 1 ]; then
        # The unit passes the target explicitly; fall back to the saved one.
        load_config
        SSH_TARGET="${1:-${SSH_TARGET:-}}"
        run_as_service
        return
    fi

    load_config
    SSH_TARGET="${1:-${SSH_TARGET:-}}"
    [ -n "$SSH_TARGET" ] || { usage; exit 1; }

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

    install_runtime_files
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

