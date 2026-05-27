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
HCI UART (H4) PTY proxy that works around Silicon Labs CPC BLE firmware
limitations by filtering and synthesizing HCI responses.

Workaround 1 — Extended Advertising feature bit stripping
----------------------------------------------------------
The CPC firmware reports Extended Advertising support in its LE
feature set, but responds to Extended commands with legacy opcodes.
We strip the bit so the kernel never uses Extended commands.

Workaround 2 — Read Local Name interception
---------------------------------------------
The CPC firmware does NOT support HCI Read Local Name (0x0C14).  It
returns a 1-byte "Unknown HCI Command" error instead of the expected
249-byte response.  The kernel logs "unexpected cc 0x0c14 length: 1
< 249" and mishandles the command completion — this corrupts the HCI
command queue, causes background scan-stop timeouts (-ETIMEDOUT),
and leaves bluetoothd in a permanently stuck state where
StartDiscovery() returns "InProgress" even though no scan is active.

We intercept outgoing Read Local Name commands and return a synthetic
success response with a placeholder name, so the unsupported command
never reaches the CPC firmware.

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

# --- Read Local Supported Commands response stripping ---
#
# On kernel 6.8+, use_ext_scan() / use_ext_adv() in hci_sync.c check
# hdev->commands[] (the Supported Commands bitmap from opcode 0x1002)
# instead of the LE features.  We must also clear the relevant command
# bits so the kernel never issues Extended Advertising / Scan commands.
#
# We match the Command Complete opcode bytes (length-agnostic) rather
# than a fixed-length prefix, because the firmware may return a
# different parameter length than the spec's 68 bytes.
#
# Pattern inside the event (after H4 0x04 + event code 0x0E + plen):
#   01         — Num_HCI_Command_Packets
#   02 10      — opcode 0x1002  (Read Local Supported Commands)
#   00         — status Success
_CMDS_CC_OPCODE = bytes([0x01, 0x02, 0x10, 0x00])  # after plen byte

# Byte offsets (0-based) within the 64-byte Supported_Commands bitmap
# to clear entirely.
#
#   Byte 36 — LE Set Advertising Set Random Address,
#             LE Set Extended Advertising Parameters / Data / Enable,
#             LE Read Max Adv Data Length, etc.
#   Byte 37 — LE Clear Advertising Sets, LE Set Periodic Adv Params /
#             Data / Enable, LE Set Extended Scan Parameters / Enable,
#             LE Extended Create Connection, LE Periodic Adv Create Sync
_EXT_ADV_CMD_BYTES = (36, 37)

# HCI Command Complete event for LE Read Local Supported Features
# with success status.

# --- Read Local Name interception ---
#
# HCI Read Local Name command (H4 format):
#   01         — H4 command packet indicator
#   14 0c      — opcode 0x0C14 (Read Local Name)
#   00         — parameter length 0
_READ_LOCAL_NAME_CMD = bytes([0x01, 0x14, 0x0C, 0x00])

# Synthetic name returned to the kernel (248 bytes, null-terminated).
_SYNTHETIC_NAME = b"SiLabs CPC BLE"
_SYNTHETIC_NAME_PADDED = _SYNTHETIC_NAME + b"\x00" * (248 - len(_SYNTHETIC_NAME))

# Pre-built Command Complete response (H4 format, 255 bytes total):
#   04         — H4 event
#   0e         — HCI_EV_CMD_COMPLETE
#   fc         — parameter length (252)
#   01         — Num_HCI_Command_Packets
#   14 0c      — opcode 0x0C14
#   00         — status Success
#   <248 bytes> — name
_READ_LOCAL_NAME_RESPONSE = (
    bytes([0x04, 0x0E, 0xFC, 0x01, 0x14, 0x0C, 0x00]) + _SYNTHETIC_NAME_PADDED
)

# --- Read Class of Device interception ---
#
# HCI Read Class of Device command (H4 format):
#   01         — H4 command packet indicator
#   23 0c      — opcode 0x0C23 (Read Class of Device)
#   00         — parameter length 0
_READ_CLASS_OF_DEV_CMD = bytes([0x01, 0x23, 0x0C, 0x00])

# Pre-built Command Complete response (H4 format):
#   04         — H4 event
#   0e         — HCI_EV_CMD_COMPLETE
#   07         — parameter length (7 = 1 + 2 + 1 + 3)
#   01         — Num_HCI_Command_Packets
#   23 0c      — opcode 0x0C23
#   00         — status Success
#   00 00 00   — Class of Device (uncategorized)
_READ_CLASS_OF_DEV_RESPONSE = bytes(
    [0x04, 0x0E, 0x07, 0x01, 0x23, 0x0C, 0x00, 0x00, 0x00, 0x00]
)

# --- Table of HCI commands that the CPC BLE firmware cannot handle ---
#
# For each (opcode_le_bytes, param_len_byte, description) we store the
# full H4 command bytes and a pre-built synthetic success response.
# The response must be exactly the length the kernel expects so the
# "unexpected cc" check passes.
#
# Adding new commands: look up the opcode and expected Command Complete
# response format in the Bluetooth Core Spec Vol 4 Part E, then add
# the bytes here.
_INTERCEPTED_COMMANDS: dict[bytes, tuple[bytes, str]] = {
    _READ_LOCAL_NAME_CMD: (_READ_LOCAL_NAME_RESPONSE, "Read Local Name (0x0C14)"),
    _READ_CLASS_OF_DEV_CMD: (
        _READ_CLASS_OF_DEV_RESPONSE,
        "Read Class of Device (0x0C23)",
    ),
}

# --- Generic short-response fixer for Command Complete events ---
#
# The CPC firmware returns "Unknown HCI Command" (status 0x01) with a
# 1-byte payload for any unsupported command.  The kernel expects at
# least N bytes (opcode-specific) and discards the event when it's too
# short, logging "unexpected cc <opcode> length: 1 < N".  This can
# corrupt the HCI command queue.
#
# For known opcodes we maintain a table of minimum response lengths.
# When we see a short Command Complete from the firmware, we pad it to
# the expected length so the kernel processes it normally (and sees the
# error status).
_OPCODE_MIN_RESPONSE_LEN: dict[int, int] = {
    # opcode → minimum payload bytes after status (to pad with zeros)
    0x0C14: 248,  # Read Local Name: name[248]
    0x0C23: 3,  # Read Class of Device: class[3]
    0x0C33: 1,  # Read Page Scan Activity: interval(2)+window(2) — but might be 4
    0x0C45: 3,  # Read Inquiry Mode: mode(1)
    0x0C55: 1,  # Read Simple Pairing Mode: mode(1)
    0x1005: 8,  # Read Buffer Size: acl_mtu(2)+sco_mtu(1)+acl_pkts(2)+sco_pkts(2)
}
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


def _strip_ext_adv_cmds(data: bytes) -> bytes:
    """Clear Extended Advertising / Scan command bits from the
    Read Local Supported Commands (0x1002) response.

    On kernel 6.8+, ``use_ext_scan()`` and ``use_ext_adv()`` in
    ``hci_sync.c`` decide whether to issue Extended commands by
    checking ``hdev->commands[36-37]`` — the Supported Commands
    bitmap — instead of the LE features.  We must zero these bytes
    so the kernel sticks to legacy scan / advertising commands.
    """
    # Parse HCI event structure (same approach as _log_hci_event)
    # to robustly find the Command Complete for opcode 0x1002.
    i = 0

    while i < len(data) - 6:
        if data[i] != 0x04:
            i += 1
            continue

        evt_code = data[i + 1]
        plen = data[i + 2]
        pkt_end = i + 3 + plen

        if evt_code != 0x0E or i + 6 >= len(data):
            i = max(pkt_end, i + 1)
            continue

        opcode = data[i + 5] << 8 | data[i + 4]

        if opcode != 0x1002:
            i = max(pkt_end, i + 1)
            continue

        status = data[i + 6]

        if _DEBUG:
            snippet = data[i : min(i + 15, len(data))].hex(" ")
            print(
                f"[hci-proxy] _strip_ext_adv_cmds: found 0x1002 at i={i} "
                f"plen={plen} status=0x{status:02X} raw={snippet}",
                flush=True,
            )

        if status != 0x00:
            if _DEBUG:
                print(
                    "[hci-proxy] _strip_ext_adv_cmds: status not success, " "skipping",
                    flush=True,
                )
            return data

        # Supported commands bitmap starts at i+7 (after H4, event code,
        # plen, num_hci_cmd_pkts, opcode_lo, opcode_hi, status).
        cmds_start = i + 7

        if cmds_start + 64 > len(data):
            return data  # Incomplete — leave unchanged.

        buf = bytearray(data)
        cleared: list[str] = []

        for byte_idx in _EXT_ADV_CMD_BYTES:
            offset = cmds_start + byte_idx

            if buf[offset] != 0:
                cleared.append(f"byte {byte_idx}: 0x{buf[offset]:02X} -> 0x00")
                buf[offset] = 0x00

        if cleared:
            print(
                "[hci-proxy] Cleared Extended Advertising/Scan command "
                "support bits in Read Local Supported Commands: " + ", ".join(cleared),
                flush=True,
            )

        return bytes(buf)

    return data


def _intercept_unsupported_commands(data: bytes, host_fd: int) -> bytes:
    """Intercept HCI commands that the CPC BLE firmware does not support.

    For each command in _INTERCEPTED_COMMANDS, if the command bytes appear
    in *data* (heading to the firmware), we:
      1. Remove the command from *data* (don't forward to firmware)
      2. Write a synthetic Command Complete response to *host_fd* (kernel)

    Returns *data* with intercepted commands removed.
    """
    for cmd_bytes, (response, desc) in _INTERCEPTED_COMMANDS.items():
        idx = data.find(cmd_bytes)

        if idx < 0:
            continue

        print(
            f"[hci-proxy] Intercepted {desc} — "
            f"returning synthetic response (CPC firmware does not support this command)",
            flush=True,
        )

        # Send the pre-built fake response back to the kernel.
        os.write(host_fd, response)

        # Remove the command from the data.
        data = data[:idx] + data[idx + len(cmd_bytes) :]

    return data


def _fix_short_cc(data: bytes) -> bytes:
    """Fix short Command Complete events from the CPC firmware.

    When the firmware returns "Unknown HCI Command" (status != 0x00) with
    only the 1-byte status and no payload, the kernel expects more bytes
    and discards the event ("unexpected cc ... length: 1 < N"), corrupting
    the command queue.

    We pad such events to the minimum expected length so the kernel
    processes them normally.
    """
    # Quick reject — must contain an HCI Event indicator + Command Complete.
    if b"\x04\x0e" not in data:
        return data

    buf = bytearray(data)
    out = bytearray()
    i = 0

    while i < len(buf):
        # Look for H4 Event (0x04) + Command Complete (0x0E)
        if buf[i] == 0x04 and i + 1 < len(buf) and buf[i + 1] == 0x0E:
            if i + 2 >= len(buf):
                out.extend(buf[i:])
                break

            plen = buf[i + 2]
            pkt_end = i + 3 + plen

            if pkt_end > len(buf):
                out.extend(buf[i:])
                break

            # Command Complete: plen includes num_cmd(1) + opcode(2) + params
            # Minimum plen = 4 means just num_cmd(1) + opcode(2) + status(1)
            if plen >= 4 and i + 6 < len(buf):
                opcode = buf[i + 4] | (buf[i + 5] << 8)
                status = buf[i + 6]

                # --- Strip ext adv/scan command bits from 0x1002 ---
                # On kernel 6.8+, use_ext_scan()/use_ext_adv() check
                # hdev->commands[36-37] instead of LE features.
                if opcode == 0x1002 and status == 0x00:
                    cmds_start = i + 7  # supported commands bitmap
                    if cmds_start + 64 <= len(buf):
                        cleared = []
                        for byte_idx in _EXT_ADV_CMD_BYTES:
                            offset = cmds_start + byte_idx
                            if buf[offset] != 0:
                                cleared.append(
                                    f"byte {byte_idx}: " f"0x{buf[offset]:02X} -> 0x00"
                                )
                                buf[offset] = 0x00
                        if cleared:
                            print(
                                "[hci-proxy] Cleared ext adv/scan command "
                                "bits in Supported Commands: " + ", ".join(cleared),
                                flush=True,
                            )

                # Only fix if status is non-zero (error) AND the response
                # is short (just status, no payload).
                payload_len = plen - 4  # bytes after status
                if (
                    status != 0
                    and payload_len == 0
                    and opcode in _OPCODE_MIN_RESPONSE_LEN
                ):
                    needed = _OPCODE_MIN_RESPONSE_LEN[opcode]
                    new_plen = 4 + needed
                    print(
                        f"[hci-proxy] Padding short CC for opcode 0x{opcode:04X} "
                        f"(status=0x{status:02X}): {plen} → {new_plen} bytes",
                        flush=True,
                    )
                    out.append(0x04)
                    out.append(0x0E)
                    out.append(new_plen)
                    out.extend(buf[i + 3 : pkt_end])  # original params
                    out.extend(b"\x00" * needed)  # padding
                    i = pkt_end
                    continue

            # Not a short CC we need to fix — pass through.
            out.extend(buf[i:pkt_end])
            i = pkt_end
            continue

        out.append(buf[i])
        i += 1

    return bytes(out)


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
                    # Firmware → kernel: filter features, translate events,
                    # and fix short error responses.
                    _log_hci_event(data, "FW→HOST")
                    data = _strip_ext_adv(data)
                    data = _strip_ext_adv_cmds(data)
                    data = _translate_conn_complete(data)
                    data = _fix_short_cc(data)
                    os.write(master_fd, data)
                else:
                    # Kernel → firmware: intercept unsupported commands.
                    _log_hci_event(data, "HOST→FW")
                    data = _intercept_unsupported_commands(data, master_fd)
                    if data:  # Forward remaining bytes (if any).
                        os.write(bridge_fd, data)
    finally:
        os.close(bridge_fd)
        os.close(master_fd)
        os.close(slave_fd)


if __name__ == "__main__":
    main()
