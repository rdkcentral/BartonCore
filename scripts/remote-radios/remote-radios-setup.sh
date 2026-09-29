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
# Run this on your WORKSTATION (Ubuntu) where the USB radios are plugged in:
#
#     curl -fsSL <raw-url>/scripts/remote-radios/remote-radios-setup.sh | bash -s -- <user>@<devserver>
#
# or, from a checkout:
#
#     scripts/remote-radios/remote-radios-setup.sh <user>@<devserver>
#
# What it does:
#   * First run: detects your Silabs Zigbee/Thread radio and a dedicated
#     Bluetooth USB dongle (prompting only if the choice is ambiguous), and
#     saves the selection to ~/.config/remote-radios/config.
#   * Installs a per-user systemd service that runs usbipd and binds the
#     Bluetooth dongle (one-time sudo to place the unit).
#   * Brings up both radios to the dev server:
#       - Silabs  : serial-over-SSH tunnel (remote-serial.py -> UNIX socket)
#       - Bluetooth: usb-ip, reverse-tunnelled over SSH
#   * Monitors health and tears everything down cleanly on logout or when the
#     corporate VPN drops.  Safe to re-run any time; recovers from unclean
#     prior sessions.
#
# Re-running validates the saved radios and (re)establishes the tunnels.
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

# usb-ip binds on this local TCP port on the workstation (usbipd default 3240)
# and we reverse-tunnel it to a per-user UNIX socket on the dev server (no sshd
# GatewayPorts change needed); socat in the container bridges it back to TCP.

# Silabs local relay port for remote-serial.py (loopback only).
SILABS_LOCAL_PORT=20000

# Known Silabs serial radio USB IDs (VID:PID).
SILABS_IDS="10c4:ea60 1366:0105 1366:1024"

# Directory to raw scripts (this script's own dir when run from a checkout).
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" 2>/dev/null && pwd || echo "")"

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
SSH_TARGET="${SSH_TARGET}"
SILABS_SERIAL="${SILABS_SERIAL}"     # persistent USB serial of the Silabs radio
SILABS_DEV="${SILABS_DEV}"
BT_BUSID="${BT_BUSID}"
BT_VIDPID="${BT_VIDPID}"
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
install_usbipd_unit() {
    [ -n "${BT_BUSID:-}" ] || return 0

    local helper="${CONFIG_DIR}/usbipd-bind.sh"
    mkdir -p "$CONFIG_DIR" "$SYSTEMD_USER_DIR"

    # usbipd + bind need root; we run them via a tiny sudo helper invoked by a
    # user systemd service.  Installing a NOPASSWD sudoers drop-in (one-time
    # sudo) keeps subsequent runs password-free.
    cat > "$helper" <<'HELPER'
#!/usr/bin/env bash
# Started by the remote-radios user service. Runs usbipd and binds the dongle.
# Root-only (invoked via sudo by the user service).
set -u
BUSID="$1"
log() { echo "[usbipd-bind] $*"; }

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

# Start usbipd if not already running.
if ! pgrep -x usbipd >/dev/null 2>&1; then
    usbipd -D
    sleep 1
fi

# Bind the dongle.  A Bluetooth dongle's interfaces are normally claimed by
# btusb; `usbip bind` detaches them first.  Treat "already bound" as success,
# but surface any other failure with diagnostics instead of hiding it.
bind_out="$(usbip bind -b "$BUSID" 2>&1)"
bind_rc=$?
if [ $bind_rc -ne 0 ]; then
    if echo "$bind_out" | grep -qi "already bound"; then
        log "${BUSID} already bound."
    else
        log "ERROR: usbip bind -b ${BUSID} failed: ${bind_out}"
        # One retry after forcing the interfaces off their kernel driver, which
        # clears the common half-detached btusb state.
        for intf in /sys/bus/usb/devices/${BUSID}:*; do
            [ -e "$intf/driver" ] && echo "$(basename "$intf")" > "$intf/driver/unbind" 2>/dev/null || true
        done
        sleep 1
        if usbip bind -b "$BUSID" 2>/dev/null; then
            log "${BUSID} bound after interface reset."
        else
            log "ERROR: still cannot bind ${BUSID}. Is the busid correct and the"
            log "       device present? Try: usbip list -l"
            exit 1
        fi
    fi
fi

# Verify the device is now exportable before declaring success.
if ! usbip list -l 2>/dev/null | grep -q "busid ${BUSID} .*(.*)" ; then
    log "WARNING: ${BUSID} not shown by 'usbip list -l' after bind."
fi
log "usb-ip export ready for ${BUSID}."

# Stay in foreground so systemd tracks the service; exit unbinds on stop.
trap 'usbip unbind -b "$BUSID" 2>/dev/null || true' EXIT
while pgrep -x usbipd >/dev/null 2>&1; do sleep 5; done
HELPER
    chmod +x "$helper"

    local sudoers
    sudoers="/etc/sudoers.d/remote-radios-$(id -un)"
    if ! sudo -n test -f "$sudoers" 2>/dev/null; then
        info "Installing a one-time sudoers rule + systemd unit for usb-ip (needs sudo once)..."
        # NOPASSWD only for the specific helper — nothing broader.
        echo "$(id -un) ALL=(root) NOPASSWD: ${helper} *" | \
            sudo tee "$sudoers" >/dev/null
        sudo chmod 440 "$sudoers"
    fi

    cat > "${SYSTEMD_USER_DIR}/remote-radios-usbip.service" <<EOF
[Unit]
Description=remote-radios usb-ip export of Bluetooth dongle ${BT_BUSID}
After=network.target

[Service]
Type=simple
ExecStart=/usr/bin/sudo -n ${helper} ${BT_BUSID}
Restart=on-failure
RestartSec=3

[Install]
WantedBy=default.target
EOF

    systemctl --user daemon-reload
    systemctl --user enable --now remote-radios-usbip.service
    ok "usb-ip export service running for dongle ${BT_BUSID}."
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
    # Curl'd standalone: fetch remote-serial.py alongside from the same base URL.
    if [ -n "${REMOTE_RADIOS_RAW_BASE:-}" ]; then
        local dst="${CONFIG_DIR}/remote-serial.py"
        curl -fsSL "${REMOTE_RADIOS_RAW_BASE}/remote-serial.py" -o "$dst" && { echo "$dst"; return 0; }
    fi
    echo ""
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
        -o ExitOnForwardFailure=yes \
        -o StreamLocalBindUnlink=yes \
        -o ControlMaster=no -o ControlPath=none \
        -R "${BT_USBIP_SOCK_REMOTE}:127.0.0.1:${USBIPD_LOCAL_PORT}" \
        "$SSH_TARGET" \
        > "${STATE_DIR}/bt-tunnel.log" 2>&1 &
    BT_TUNNEL_PID=$!
    echo "$BT_TUNNEL_PID" > "${STATE_DIR}/bt-tunnel.pid"
}

write_remote_env_hint() {
    # Drop a small env file on the dev server so setupDockerEnv.sh / dockerw can
    # pick up the radio parameters automatically.
    ssh -o BatchMode=yes "$SSH_TARGET" \
        "mkdir -p ~/.remote-radios/radios ~/.remote-radios/usbip && chmod 700 ~/.remote-radios && cat > ~/.remote-radios/radios.env <<EOF
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
    info "Monitoring tunnels (Ctrl-C to stop). Tears down on VPN drop or logout."
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

        # Restart any tunnel process that died.
        if [ -n "${SILABS_DEV:-}" ] && [ -n "${SILABS_TUNNEL_PID:-}" ] \
            && ! kill -0 "$SILABS_TUNNEL_PID" 2>/dev/null; then
            warn "Silabs tunnel died — restarting."
            start_silabs_tunnel
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
main() {
    require_cmds
    mkdir -p "$STATE_DIR"; chmod 700 "$STATE_DIR"

    load_config
    SSH_TARGET="${1:-${SSH_TARGET:-}}"
    [ -n "$SSH_TARGET" ] || die "Usage: remote-radios-setup.sh <user>@<devserver>"

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

    install_usbipd_unit
    resolve_remote_params
    start_silabs_tunnel
    start_bt_tunnel
    write_remote_env_hint

    echo
    ok "Remote radios are set up."
    info "On the dev server, run your devcontainer with the radio overlay:"
    printf '    %s./dockerw -T bash%s\n' "$BOLD" "$NC"
    echo

    monitor_loop
}

main "$@"
