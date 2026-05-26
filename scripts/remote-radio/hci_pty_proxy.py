#!/usr/bin/env python3
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

"""
HCI UART (H4) PTY proxy that strips the LE Extended Advertising feature
bit from the LE Read Local Supported Features response.

This forces the Linux kernel to use legacy BLE scan and connection
commands (0x200b/0x200c/0x200d) instead of the extended versions
(0x2041/0x2042/0x2039).

Background
----------
The Silicon Labs CPC BLE firmware reports Extended Advertising support
in its LE feature set, but when the kernel sends LE Set Extended Scan
Enable (0x2042) to *disable* scanning, the firmware responds with the
legacy opcode 0x200c.  The kernel treats this as an unexpected event,
discards the response, and the 0x2042 command times out.  Because
scanning cannot be stopped, LE Create Connection is rejected with
"Command Disallowed" (0x0c), and BLE connections never succeed.

By clearing the Extended Advertising feature bit during controller
initialisation the kernel never issues Extended commands and the
firmware's legacy responses are handled correctly.

Usage
-----
    hci_pty_proxy.py <bridge_pts> <symlink_path>

  bridge_pts   — PTY device created by bt_host_cpc_hci_bridge
                 (e.g. /dev/pts/1)
  symlink_path — Convenience symlink that btattach will open
                 (e.g. /var/run/bt-hci-bridge/pts_hci)

The proxy opens *bridge_pts*, creates a new PTY pair, updates
*symlink_path* to point at the new slave, then relays traffic
bidirectionally.  btattach should be started *after* this proxy.
"""

import os
import pty
import select
import sys
import termios
import time
import tty


# Feature bit to clear — byte offset 1, bit 4 (overall bit 12)
# in the 8-byte LE features array.
#
#   Byte 1, bit 4 = HCI_LE_EXT_ADV (0x10)
#
# Kernel reference: include/net/bluetooth/hci.h
_LE_FEAT_BYTE = 1
_LE_EXT_ADV_BIT = 0x10

# HCI Command Complete event for LE Read Local Supported Features
# with success status.
#
#   04         — H4 event packet indicator
#   0e         — HCI_EV_CMD_COMPLETE
#   0c         — parameter length (12)
#   01         — Num_HCI_Command_Packets
#   03 20      — opcode 0x2003  (LE Read Local Supported Features)
#   00         — status Success
#
# The next 8 bytes are the LE features.
_MATCH_PREFIX = bytes([0x04, 0x0E, 0x0C, 0x01, 0x03, 0x20, 0x00])
_FEATURES_OFFSET = len(_MATCH_PREFIX)  # start of LE features in the match

# LE Meta Event (0x3E) subevent codes for connection complete events.
#
#   0x01 — LE Connection Complete (legacy, 19 param bytes)
#   0x0A — LE Enhanced Connection Complete [v1] (31 param bytes)
#   0x29 — LE Enhanced Connection Complete [v2] (34 param bytes)
#
# Both enhanced variants include Local/Peer Resolvable Private Address
# fields (6 bytes each = 12 extra bytes) that the legacy event lacks.
# v2 additionally includes Advertising_Handle (1) and Sync_Handle (2).
#
# When we strip the Extended Advertising feature bit the kernel only
# registers a handler for subevent 0x01, so we must translate 0x0A and
# 0x29 into 0x01 by removing the extra fields.
_SUBEVENT_CONN_COMPLETE = 0x01
_SUBEVENT_ENH_CONN_COMPLETE_V1 = 0x0A
_SUBEVENT_ENH_CONN_COMPLETE_V2 = 0x29

# Offsets within the LE Meta Event *parameters* (after subevent byte):
#   Status(1) + Handle(2) + Role(1) + AddrType(1) + Addr(6) = 11 bytes
# The RPA fields start at parameter offset 11+1 (subevent byte).
_CONN_COMMON_PREFIX_LEN = 11  # bytes before the RPA fields (after subevent)
_RPA_TOTAL_LEN = 12           # Local RPA (6) + Peer RPA (6)
_V2_EXTRA_LEN = 3             # Advertising_Handle (1) + Sync_Handle (2)

# Buffer size for reads — kept small for low-latency relay.
_BUFSIZE = 4096

# Set to True for verbose HCI event logging.
_DEBUG = os.environ.get("HCI_PROXY_DEBUG", "") != ""


def _log_hci_event(data: bytes, direction: str) -> None:
    """Log HCI events for debugging, with special attention to LE Meta Events."""
    if not _DEBUG:
        return

    i = 0
    while i < len(data):
        if data[i] == 0x04 and i + 2 < len(data):
            # HCI Event packet
            evt_code = data[i + 1]
            plen = data[i + 2]
            pkt_end = i + 3 + plen

            if evt_code == 0x3E and i + 3 < len(data):
                # LE Meta Event — subevent code is first param byte
                subevent = data[i + 3]
                names = {
                    0x01: "LE Connection Complete",
                    0x02: "LE Advertising Report",
                    0x03: "LE Connection Update Complete",
                    0x04: "LE Read Remote Features Complete",
                    0x05: "LE Long Term Key Request",
                    0x0A: "LE Enhanced Connection Complete",
                    0x0D: "LE Extended Advertising Report",
                }
                name = names.get(subevent, f"Unknown(0x{subevent:02X})")
                pkt_hex = data[i : min(pkt_end, i + 30)].hex()
                print(
                    f"[hci-proxy] {direction} LE Meta subevent=0x{subevent:02X} "
                    f"({name}) plen={plen} hex={pkt_hex}",
                    flush=True,
                )
            elif evt_code == 0x0F:
                # Command Status
                if i + 6 < len(data):
                    status = data[i + 3]
                    opcode = data[i + 5] << 8 | data[i + 4]
                    print(
                        f"[hci-proxy] {direction} Command Status: "
                        f"status=0x{status:02X} opcode=0x{opcode:04X}",
                        flush=True,
                    )
            elif evt_code == 0x0E:
                # Command Complete
                if i + 6 < len(data):
                    opcode = data[i + 5] << 8 | data[i + 4]
                    print(
                        f"[hci-proxy] {direction} Command Complete: "
                        f"opcode=0x{opcode:04X}",
                        flush=True,
                    )
            elif evt_code not in (0x3E,):
                print(
                    f"[hci-proxy] {direction} Event 0x{evt_code:02X} plen={plen}",
                    flush=True,
                )

            i = max(pkt_end, i + 1)
        elif data[i] == 0x02 and i + 4 < len(data):
            # ACL data packet
            handle = (data[i + 2] << 8 | data[i + 1]) & 0x0FFF
            acl_len = data[i + 4] << 8 | data[i + 3]
            print(
                f"[hci-proxy] {direction} ACL handle={handle} len={acl_len}",
                flush=True,
            )
            i += 5 + acl_len
        else:
            i += 1


def _configure_raw(fd: int) -> None:
    """Put a PTY file descriptor into raw mode (no echo, no line editing)."""
    try:
        tty.setraw(fd)
    except termios.error:
        pass  # Not a tty — skip.


def _strip_ext_adv(data: bytes) -> bytes:
    """If *data* contains the LE features response, clear the Extended
    Advertising bit so the kernel uses legacy BLE commands."""
    idx = data.find(_MATCH_PREFIX)

    if idx < 0:
        return data

    feat_start = idx + _FEATURES_OFFSET

    if feat_start + 8 > len(data):
        return data  # Incomplete — leave unchanged (rare edge-case).

    original = data[feat_start + _LE_FEAT_BYTE]

    if not (original & _LE_EXT_ADV_BIT):
        return data  # Already clear — nothing to do.

    buf = bytearray(data)
    buf[feat_start + _LE_FEAT_BYTE] = original & ~_LE_EXT_ADV_BIT
    print(
        f"[hci-proxy] Cleared LE Extended Advertising feature bit "
        f"(0x{original:02X} -> 0x{buf[feat_start + _LE_FEAT_BYTE]:02X})",
        flush=True,
    )

    return bytes(buf)


def _translate_conn_complete(data: bytes) -> bytes:
    """Translate LE Enhanced Connection Complete v1/v2 events into the
    legacy LE Connection Complete event so the kernel processes them.

    Enhanced variants contain Local/Peer Resolvable Private Address
    fields and (in v2) Advertising_Handle + Sync_Handle that the legacy
    event lacks.  We strip those extra fields and rewrite the subevent
    code and parameter length.

    The function scans for *all* occurrences in *data* (there is almost
    always at most one, but this keeps the logic robust).
    """
    # Quick reject — must contain an LE Meta Event.
    if b"\x04\x3e" not in data:
        return data

    buf = bytearray(data)
    out = bytearray()
    i = 0

    while i < len(buf):
        # Look for H4 Event indicator (0x04) followed by LE Meta (0x3E).
        if (
            buf[i] == 0x04
            and i + 3 < len(buf)
            and buf[i + 1] == 0x3E
        ):
            plen = buf[i + 2]
            pkt_end = i + 3 + plen

            if pkt_end > len(buf):
                # Incomplete packet at end of buffer — pass through as-is.
                out.extend(buf[i:])
                break

            subevent = buf[i + 3]

            if subevent == _SUBEVENT_ENH_CONN_COMPLETE_V2 and plen >= 34:
                # v2: subevent(1) + common(11) + RPAs(12) + interval/lat/to/acc(7) + adv(1) + sync(2) = 34
                common = buf[i + 4 : i + 4 + _CONN_COMMON_PREFIX_LEN]  # 11 bytes
                tail_start = i + 4 + _CONN_COMMON_PREFIX_LEN + _RPA_TOTAL_LEN
                tail_end = pkt_end - _V2_EXTRA_LEN
                tail = buf[tail_start:tail_end]  # interval + latency + timeout + accuracy = 7 bytes
                new_plen = 1 + _CONN_COMMON_PREFIX_LEN + len(tail)  # 1 + 11 + 7 = 19

                out.append(0x04)                    # H4 event
                out.append(0x3E)                    # LE Meta Event
                out.append(new_plen)                # parameter length
                out.append(_SUBEVENT_CONN_COMPLETE) # subevent 0x01
                out.extend(common)                  # status + handle + role + addrtype + addr
                out.extend(tail)                    # interval + latency + timeout + accuracy

                print(
                    f"[hci-proxy] Translated Enhanced Connection Complete v2 "
                    f"(0x{subevent:02X}) -> legacy (0x01) "
                    f"handle={common[1] | (common[2] << 8)} "
                    f"status=0x{common[0]:02X}",
                    flush=True,
                )
                i = pkt_end
                continue

            if subevent == _SUBEVENT_ENH_CONN_COMPLETE_V1 and plen >= 31:
                # v1: subevent(1) + common(11) + RPAs(12) + interval/lat/to/acc(7) = 31
                common = buf[i + 4 : i + 4 + _CONN_COMMON_PREFIX_LEN]
                tail_start = i + 4 + _CONN_COMMON_PREFIX_LEN + _RPA_TOTAL_LEN
                tail = buf[tail_start:pkt_end]  # interval + latency + timeout + accuracy = 7
                new_plen = 1 + _CONN_COMMON_PREFIX_LEN + len(tail)

                out.append(0x04)
                out.append(0x3E)
                out.append(new_plen)
                out.append(_SUBEVENT_CONN_COMPLETE)
                out.extend(common)
                out.extend(tail)

                print(
                    f"[hci-proxy] Translated Enhanced Connection Complete v1 "
                    f"(0x{subevent:02X}) -> legacy (0x01) "
                    f"handle={common[1] | (common[2] << 8)} "
                    f"status=0x{common[0]:02X}",
                    flush=True,
                )
                i = pkt_end
                continue

        # Default: copy byte as-is.
        out.append(buf[i])
        i += 1

    return bytes(out)


def main() -> None:
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <bridge_pts> <symlink_path>", file=sys.stderr)
        sys.exit(1)

    bridge_pts = sys.argv[1]
    symlink_path = sys.argv[2]

    # Open the PTY created by bt_host_cpc_hci_bridge.
    bridge_fd = os.open(bridge_pts, os.O_RDWR | os.O_NOCTTY)
    _configure_raw(bridge_fd)

    # Create a new PTY pair for btattach.
    master_fd, slave_fd = pty.openpty()
    slave_name = os.ttyname(slave_fd)
    _configure_raw(master_fd)

    # Update the convenience symlink so btattach finds the new PTY.
    try:
        os.unlink(symlink_path)
    except FileNotFoundError:
        pass
    os.symlink(slave_name, symlink_path)

    print(
        f"[hci-proxy] Relaying: {bridge_pts} <-> {slave_name} "
        f"(symlink {symlink_path})",
        flush=True,
    )

    try:
        while True:
            readable, _, _ = select.select([bridge_fd, master_fd], [], [])

            for fd in readable:
                try:
                    data = os.read(fd, _BUFSIZE)
                except OSError:
                    data = b""

                if not data:
                    print("[hci-proxy] EOF — exiting.", flush=True)
                    return

                if fd == bridge_fd:
                    # Firmware → kernel: filter features and translate events.
                    _log_hci_event(data, "FW→HOST")
                    data = _strip_ext_adv(data)
                    data = _translate_conn_complete(data)
                    os.write(master_fd, data)
                else:
                    # Kernel → firmware: pass through unchanged.
                    _log_hci_event(data, "HOST→FW")
                    os.write(bridge_fd, data)
    finally:
        os.close(bridge_fd)
        os.close(master_fd)
        os.close(slave_fd)


if __name__ == "__main__":
    main()
