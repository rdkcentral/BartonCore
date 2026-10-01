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
Remote Radio Forwarder — forward a locally attached Silicon Labs radio and/or
a usb-ip device (such as a Bluetooth dongle) to a remote dev server over SSH.

Run this on the WORKSTATION where the radios are physically connected.  It
works on both Linux and Windows, and is the supported entry point on Windows,
where remote-radios-setup.sh (bash, systemd, sudo) cannot run.  It:

  1. Auto-detects the Silicon Labs radio serial port.
  2. Starts a local TCP server that relays bytes between the serial port and
     a local TCP client.
  3. Opens an SSH reverse tunnel that binds a per-user UNIX socket on the
     remote dev server (~/.remote-radios/radios/silabs.sock) forwarding to the
     local TCP server.  The consumer on the dev server bind-mounts that socket
     and runs socat (UNIX -> PTY) so cpcd sees a virtual serial device.
  4. Optionally forwards the local usb-ip daemon's TCP port (3240) to
     ~/.remote-radios/usbip/usbipd.sock, so the dev server can `usbip attach`
     a Bluetooth dongle.  No relay is needed for this: the usb-ip daemon
     already listens on TCP.
  5. Self-heals: reconnects serial and SSH on failure with backoff.

Using a per-user UNIX socket (instead of a TCP port) gives natural per-user
isolation via 0700 directory permissions and needs no sshd GatewayPorts change.

Sharing a device over usb-ip is privileged.  On Windows this script runs
`usbipd bind` directly (which needs an Administrator console).  On Linux,
remote-radios-setup.sh owns binding — it installs a root helper, a narrow
sudoers rule and a watchdog that re-binds the device if the driver reverts —
so this script only discovers there.

Requirements (workstation):
  - Python 3.10+
  - pyserial  (pip install pyserial)
  - ssh client on PATH
  - for --usbip: usbipd-win on Windows, usbip tools on Linux

Usage:
  python remote-serial.py user@devserver.example.com
  python remote-serial.py user@devserver.example.com --port COM3
  python remote-serial.py user@devserver.example.com --usbip
  python remote-serial.py user@devserver.example.com --usbip 2-4
  python remote-serial.py user@devserver.example.com --no-serial --usbip

The script keeps running until Ctrl-C.  The SSH tunnel and serial relay
are restarted automatically if either side disconnects.

On Linux this is normally invoked by remote-radios-setup.sh rather than run
directly.  See docs/REMOTE_RADIO_FOR_DEVELOPMENT.md for full setup instructions.
"""

import argparse
import datetime
import glob
import os
import platform
import re
import signal
import socket
import subprocess
import sys
import threading
import time

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------
SILABS_VID = "10C4"
SILABS_PID = "EA60"
SEGGER_VID = "1366"
SEGGER_PID = "0105"
# The J-Link CDC exposed by multiprotocol RCP firmware on xG24 dev kits.
SEGGER_PID_RCP = "1024"
SERIAL_BAUD = 115200
BASE_PORT = 20000
RELAY_BUF_SIZE = 4096

# Per-adapter default for RTS/CTS hardware flow control, used only when neither
# --rtscts nor --no-rtscts is given.  Keyed on VID:PID rather than VID alone:
# the SEGGER J-Link VCP on older WSTKs (1366:0105) does not reliably drive the
# RTS/CTS lines, so requesting flow control leaves RTS deasserted and a
# flow-controlled radio stays silent.  The J-Link CDC exposed by multiprotocol
# RCP firmware (1366:1024) does drive them and is verified working with flow
# control on, so it is deliberately absent and falls through to the default.
# Anything not listed defaults to on; the relay's watchdog corrects a wrong
# guess at runtime.
RTSCTS_HINTS = {"1366:0105": False, "10c4:ea60": True}

# usb-ip: usbipd (Linux) and usbipd-win (Windows) both listen on 3240.
USBIP_PORT = 3240
# TP-Link UB500.  Preferred when several devices are shareable, because it is
# the dongle this workflow is built around.
UB500_VID_PID = "2357:0604"
# Both `usbip` and `usbipd-win` spell bus IDs the same way: 2-4, 1-9.1, ...
USBIP_BUSID_RE = re.compile(r"^[0-9]+-[0-9]+(\.[0-9]+)*$")

# ---------------------------------------------------------------------------
# Logging helpers
# ---------------------------------------------------------------------------
BOLD = "\033[1m"
GREEN = "\033[0;32m"
RED = "\033[0;31m"
YELLOW = "\033[1;33m"
NC = "\033[0m"

# Disable ANSI on Windows unless the terminal supports it.
if platform.system() == "Windows":
    try:
        os.system("")  # enable VT100 on Windows 10+
    except Exception:
        BOLD = GREEN = RED = YELLOW = NC = ""


def _ts() -> str:
    """Return a compact timestamp for log lines."""
    return datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]


def info(msg: str) -> None:
    print(f"{_ts()} {BOLD}[remote-serial]{NC} {msg}", flush=True)


def ok(msg: str) -> None:
    print(f"{_ts()} {GREEN}[remote-serial] OK:{NC} {msg}", flush=True)


def warn(msg: str) -> None:
    print(f"{_ts()} {YELLOW}[remote-serial] WARNING:{NC} {msg}", file=sys.stderr, flush=True)


def fail(msg: str) -> None:
    print(f"{_ts()} {RED}[remote-serial] ERROR:{NC} {msg}", file=sys.stderr, flush=True)


def die(msg: str) -> None:
    fail(msg)
    sys.exit(1)


# ---------------------------------------------------------------------------
# Serial port detection
# ---------------------------------------------------------------------------
def find_radio_port() -> str | None:
    """Auto-detect the Silicon Labs radio serial port.

    If multiple matching devices are found, returns None so the caller
    can report the ambiguity and require --port.
    """
    try:
        from serial.tools.list_ports import comports
    except ImportError:
        die(
            "pyserial is not installed.  Install it with:\n"
            "    pip install pyserial"
        )

    target_ids = [
        (SILABS_VID.lower(), SILABS_PID.lower()),
        (SEGGER_VID.lower(), SEGGER_PID.lower()),
        (SEGGER_VID.lower(), SEGGER_PID_RCP.lower()),
    ]

    matches = []

    for port_info in comports():
        vid = f"{port_info.vid:04x}" if port_info.vid else ""
        pid = f"{port_info.pid:04x}" if port_info.pid else ""

        for t_vid, t_pid in target_ids:

            if vid == t_vid and pid == t_pid:
                matches.append(port_info)
                break

    if len(matches) == 1:
        return matches[0].device

    if len(matches) > 1:
        fail("Multiple Silicon Labs radios found:")

        for m in matches:
            info(f"  {m.device}  ({m.description})")

        die("Specify which radio to use with --port /dev/ttyACM0")

    return None


def port_vid_pid(device: str) -> str | None:
    """Return the lower-case "vid:pid" of a serial port, if known."""
    try:
        from serial.tools.list_ports import comports
    except ImportError:
        return None

    for port_info in comports():

        if port_info.device == device and port_info.vid and port_info.pid:
            return f"{port_info.vid:04x}:{port_info.pid:04x}"

    return None


def resolve_rtscts(cli_value: bool | None, vid_pid: str | None) -> tuple[bool, str]:
    """Decide the RTS/CTS flow-control mode and report where it came from.

    Precedence: an explicit --rtscts/--no-rtscts flag wins, then the adapter's
    USB VID:PID hint, then on.  Getting this wrong is not a hard failure but a
    silent one — a flow-controlled radio simply never answers — so the relay
    also runs a watchdog that flips the mode once if the radio stays mute.
    """
    if cli_value is not None:
        return cli_value, "--rtscts/--no-rtscts flag"

    if vid_pid and vid_pid in RTSCTS_HINTS:
        return RTSCTS_HINTS[vid_pid], f"USB {vid_pid} hint"

    return True, "default"


# ---------------------------------------------------------------------------
# SSH helpers
# ---------------------------------------------------------------------------
def parse_ssh_target(target: str) -> tuple[str, str]:
    """Split user@host into (user, host).  If no user@, use current user."""
    if "@" in target:
        user, host = target.split("@", 1)
    else:
        user = os.environ.get("USER") or os.environ.get("USERNAME") or "unknown"
        host = target

    return user, host


def get_remote_uid(ssh_target: str) -> int:
    """Fetch the remote user's UID via SSH."""
    info(f"Resolving remote UID for {ssh_target}...")

    try:
        result = subprocess.run(
            ["ssh", "-o", "ConnectTimeout=10", ssh_target, "id -u"],
            capture_output=True,
            text=True,
            timeout=30,
        )
    except FileNotFoundError:
        die("ssh is not available on PATH.")
    except subprocess.TimeoutExpired:
        die(f"SSH to {ssh_target} timed out.")

    if result.returncode != 0:
        die(
            f"Cannot SSH to {ssh_target}.\n"
            f"  stderr: {result.stderr.strip()}"
        )

    uid_str = result.stdout.strip()

    if not uid_str.isdigit():
        die(f"Got invalid UID '{uid_str}' from remote host.")

    return int(uid_str)


def compute_default_socket(user: str) -> str:
    """Default per-user remote socket path served by the SSH reverse tunnel."""
    return f"/home/{user}/.remote-radios/radios/silabs.sock"


def compute_usbip_socket(user: str) -> str:
    """Default per-user remote socket path for the forwarded usb-ip endpoint."""
    return f"/home/{user}/.remote-radios/usbip/usbipd.sock"


# ---------------------------------------------------------------------------
# usb-ip discovery and binding
# ---------------------------------------------------------------------------
# A Bluetooth dongle is forwarded by tunnelling the local usb-ip daemon's TCP
# port to a UNIX socket on the dev server; the consumer there runs
# `usbip attach`.  Discovery and binding are the only genuinely
# platform-specific parts, because Windows uses usbipd-win and Linux uses the
# in-tree usbip tools.
#
# On Linux, remote-radios-setup.sh owns binding: it installs a root helper and
# a narrow sudoers rule, and keeps the device bound with a watchdog.  This
# module therefore only *discovers* on Linux and defers binding to that script.
# On Windows there is no such wrapper, so binding happens here.


def _run_capture(cmd: list[str], timeout: int = 20) -> str | None:
    """Run a command and return stdout, or None if it is missing or failed."""
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    except (FileNotFoundError, subprocess.TimeoutExpired, OSError):
        return None

    if result.returncode != 0:
        return None

    return result.stdout


def _parse_usbipd_windows(output: str) -> list[tuple[str, str, bool]]:
    """Parse `usbipd list` (usbipd-win) into (busid, description, shared).

    Rows under "Connected:" read `BUSID  VID:PID  DEVICE...  STATE`, where
    STATE is one of "Not shared", "Shared", "Shared (forced)" or "Attached".
    Note that "Not shared" also ends in the word "shared", so the state must be
    matched in full rather than by its last token.
    """
    devices: list[tuple[str, str, bool]] = []
    in_connected = False

    for line in output.splitlines():
        stripped = line.strip()

        if stripped.lower().startswith("connected:"):
            in_connected = True
            continue

        # Any other section header (e.g. "Persisted:") ends the connected list.
        if in_connected and stripped.endswith(":") and " " not in stripped:
            break

        if not in_connected or not stripped or stripped.startswith("BUSID"):
            continue

        # BUSID  VID:PID  DEVICE (may contain single spaces)  STATE
        match = re.match(
            r"^(\S+)\s+([0-9a-fA-F]{4}:[0-9a-fA-F]{4})\s+(.*?)\s{2,}(\S.*)$",
            stripped,
        )

        if not match:
            continue

        busid, vid_pid, description, state = match.groups()

        if not USBIP_BUSID_RE.match(busid):
            continue

        normalized = state.strip().lower()
        shared = normalized.startswith("shared") or normalized == "attached"
        devices.append((busid, f"{vid_pid} {description.strip()}", shared))

    return devices


def _parse_usbip_linux(output: str) -> list[tuple[str, str, bool]]:
    """Parse `usbip list -r 127.0.0.1` output into (busid, description, True).

    Only already-exported devices appear, so everything listed is shared.
    """
    devices: list[tuple[str, str, bool]] = []

    for line in output.splitlines():
        match = re.match(r"^\s*([0-9]+-[0-9.]+):\s*(.+)$", line)

        if not match:
            continue

        busid, description = match.groups()

        if USBIP_BUSID_RE.match(busid):
            devices.append((busid, description.strip(), True))

    return devices


def _read_sysfs(path: str) -> str:
    """Read a sysfs attribute, returning '' when it is absent."""
    try:
        with open(path, "r") as handle:
            return handle.read().strip()
    except OSError:
        return ""


def _discover_usbip_sysfs() -> list[tuple[str, str, bool]]:
    """List Linux devices already bound to usbip-host, straight from sysfs.

    `usbip list -r` only reports devices that are *available*; once the dev
    server attaches one it disappears from that listing.  Reading sysfs keeps
    an in-use dongle discoverable.
    """
    devices: list[tuple[str, str, bool]] = []

    for entry in sorted(glob.glob("/sys/bus/usb/devices/*")):
        busid = os.path.basename(entry)

        if not USBIP_BUSID_RE.match(busid):
            continue

        # usbip_status only exists while the device is bound to usbip-host.
        if not os.path.exists(os.path.join(entry, "usbip_status")):
            continue

        vid = _read_sysfs(os.path.join(entry, "idVendor"))
        pid = _read_sysfs(os.path.join(entry, "idProduct"))
        product = _read_sysfs(os.path.join(entry, "product"))
        manufacturer = _read_sysfs(os.path.join(entry, "manufacturer"))
        label = " ".join(p for p in (manufacturer, product) if p)
        devices.append((busid, f"{vid}:{pid} {label}".strip(), True))

    return devices


def discover_usbip() -> list[tuple[str, str, bool]]:
    """Return (busid, description, shared) for usb-ip capable local devices."""
    if platform.system() == "Windows":
        output = _run_capture(["usbipd", "list"])
        return _parse_usbipd_windows(output) if output else []

    # Linux: enumerating exportable devices requires a local usbipd, and only
    # covers devices that are not currently attached — so merge in sysfs.
    output = _run_capture(["usbip", "list", "-r", "127.0.0.1"])
    devices = _parse_usbip_linux(output) if output else []
    seen = {busid for busid, _, _ in devices}

    for device in _discover_usbip_sysfs():
        if device[0] not in seen:
            devices.append(device)

    return devices


def bind_usbip(busid: str) -> bool:
    """Share a device over usb-ip.  Windows only; Linux defers to setup.sh."""
    if not USBIP_BUSID_RE.match(busid):
        fail(f"Refusing to bind malformed bus ID '{busid}'.")
        return False

    if platform.system() != "Windows":
        fail(
            f"Device {busid} is not shared over usb-ip.\n"
            "  On Linux, run scripts/remote-radios/remote-radios-setup.sh instead —\n"
            "  it installs the privileged helper that binds and re-binds the dongle."
        )
        return False

    info(f"Sharing {busid} over usb-ip (usbipd bind)...")
    result = subprocess.run(
        ["usbipd", "bind", "--busid", busid],
        capture_output=True, text=True,
    )

    if result.returncode == 0:
        ok(f"Device {busid} is now shared.")
        return True

    fail(
        f"Could not share {busid}: {result.stderr.strip() or result.stdout.strip()}\n"
        "  usbipd bind needs an Administrator console.  Open one and run:\n"
        f"      usbipd bind --busid {busid}"
    )
    return False


def select_usbip_busid(requested: str) -> str | None:
    """Resolve the bus ID to forward, binding it first when necessary.

    'auto' prefers the TP-Link UB500, then falls back to a single unambiguous
    candidate.  Anything else is treated as an explicit bus ID.
    """
    devices = discover_usbip()

    if requested != "auto":
        if not USBIP_BUSID_RE.match(requested):
            fail(f"'{requested}' is not a valid usb-ip bus ID (expected e.g. 2-4).")
            return None

        for busid, _, shared in devices:
            if busid == requested:
                return requested if shared or bind_usbip(requested) else None

        # Not enumerated (common on Linux when usbipd is not yet running).
        return requested

    if not devices:
        warn(
            "No usb-ip devices found.  On Windows, check that usbipd-win is "
            "installed; on Linux, that usbipd is running."
        )
        return None

    preferred = [d for d in devices if UB500_VID_PID in d[1]]
    candidates = preferred or devices

    if len(candidates) > 1:
        fail("Several usb-ip devices are available — choose one with --usbip BUSID:")

        for busid, description, shared in candidates:
            state = "shared" if shared else "not shared"
            print(f"    {busid:<10} {description}  [{state}]")

        return None

    busid, description, shared = candidates[0]
    ok(f"Bluetooth dongle: {busid} ({description})")

    if not shared and not bind_usbip(busid):
        return None

    return busid


# ---------------------------------------------------------------------------
# Serial ↔ TCP relay
# ---------------------------------------------------------------------------
class SerialRelay:
    """Relay bytes between a serial port and TCP clients.

    Architecture:
      - The serial port is opened once and kept open for the lifetime of
        the relay.  This prevents resetting the radio's CPC state machine.
      - A serial-reader thread runs continuously, forwarding data to the
        current TCP client or discarding it when no client is connected.
        This prevents stale data from accumulating in the kernel serial
        buffer during TCP reconnection gaps.
      - A TCP-writer thread reads from the current TCP client and writes
        to the serial port.
      - Only one TCP client at a time (cpcd is the sole consumer via socat).

    Self-healing: USB unplug → serial reopen; socat reconnect → new TCP
    client accepted seamlessly without disturbing the serial port.
    """

    def __init__(
        self,
        serial_port: str,
        listen_port: int,
        baud: int = SERIAL_BAUD,
        rtscts: bool = True,
    ):
        self.serial_port = serial_port
        self.listen_port = listen_port
        self.baud = baud
        self._stop = threading.Event()
        self._server_sock: socket.socket | None = None
        # Guarded by _client_lock.  Set to the active TCP socket or None.
        self._client: socket.socket | None = None
        self._client_lock = threading.Lock()
        # Flow control, plus the byte counters the watchdog reasons about.
        self._rtscts = rtscts
        self._flipped = False
        self._tx = 0  # client → radio
        self._rx = 0  # radio → client
        self._ser = None

    def start(self) -> None:
        """Bind the TCP server socket, then start the relay threads.

        Binding synchronously ensures the port is ready before the SSH
        tunnel is opened — otherwise the remote socat may connect before
        the listen socket exists and get ECONNREFUSED.
        """
        self._server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._server_sock.settimeout(2.0)
        self._server_sock.bind(("127.0.0.1", self.listen_port))
        self._server_sock.listen(1)
        ok(f"TCP server listening on 127.0.0.1:{self.listen_port}")

        t = threading.Thread(target=self._run, daemon=True, name="serial-relay")
        t.start()

    def stop(self) -> None:
        """Signal the relay to stop."""
        self._stop.set()

        if self._server_sock:
            try:
                self._server_sock.close()
            except OSError:
                pass

    def _set_client(self, client: socket.socket | None) -> None:
        """Swap the active TCP client, closing the old one."""
        with self._client_lock:
            old = self._client
            self._client = client

        if old is not None:
            try:
                old.close()
            except OSError:
                pass

    def _open_serial(self):
        """Open the serial port, retrying on failure."""
        import serial

        backoff = 1

        while not self._stop.is_set():

            try:
                ser = serial.Serial(
                    self.serial_port,
                    self.baud,
                    rtscts=self._rtscts,
                    timeout=0.1,
                )
                mode = "hw flow control" if self._rtscts else "no flow control"
                ok(f"Serial port {self.serial_port} opened ({self.baud} baud, {mode})")
                return ser
            except serial.SerialException as e:
                warn(f"Cannot open {self.serial_port}: {e} — retrying in {backoff}s")
                time.sleep(backoff)
                backoff = min(backoff * 2, 30)

        return None

    def _run(self) -> None:
        try:
            import serial as serial_mod
        except ImportError:
            die("pyserial is not installed.  Install it with:\n    pip install pyserial")

        # Open the serial port once and keep it open.
        ser = self._open_serial()

        if ser is None:
            return

        self._ser = ser

        # Watch for the silent-radio signature of a wrong flow-control mode.
        threading.Thread(
            target=self._flow_watchdog, daemon=True, name="flow-watchdog"
        ).start()

        # Start the serial reader thread — it runs for the entire lifetime
        # of the relay, forwarding data to the current TCP client or
        # discarding it when none is connected.
        serial_reader = threading.Thread(
            target=self._serial_reader_loop,
            args=(ser, serial_mod),
            daemon=True,
            name="serial-reader",
        )
        serial_reader.start()

        # Main loop: accept TCP clients and read from them.
        while not self._stop.is_set():
            # Accept a TCP client.
            try:
                client, addr = self._server_sock.accept()
            except socket.timeout:
                continue
            except OSError:
                break

            info(f"TCP client connected from {addr}")
            client.settimeout(0.5)
            self._set_client(client)

            # Read from TCP client → serial port, until client disconnects.
            self._tcp_to_serial_loop(client, ser, serial_mod)

            info("TCP client disconnected — keeping serial port open.")
            self._set_client(None)

            # Check if serial port is still healthy after a USB unplug.
            if not ser.is_open:
                warn("Serial port closed — attempting to reopen...")
                ser = self._open_serial()

                if ser is None:
                    break

                self._ser = ser

                # Restart the serial reader for the new port instance.
                serial_reader = threading.Thread(
                    target=self._serial_reader_loop,
                    args=(ser, serial_mod),
                    daemon=True,
                    name="serial-reader",
                )
                serial_reader.start()

            if not self._stop.is_set():
                info("Waiting for next TCP connection...")

    def _serial_reader_loop(self, ser, serial_mod) -> None:
        """Continuously read from serial port.

        Sends data to the current TCP client if one is connected.
        Discards data otherwise (prevents stale bytes from accumulating
        in the kernel serial buffer during TCP reconnection gaps).
        """

        while not self._stop.is_set():

            try:
                data = ser.read(RELAY_BUF_SIZE)
            except serial_mod.SerialException:
                info("serial reader: port error — stopping reader")
                break
            except OSError:
                break

            if not data:
                continue

            self._rx += len(data)

            with self._client_lock:
                client = self._client

            if client is not None:

                try:
                    client.sendall(data)
                except OSError:
                    # TCP client went away; the main loop will handle it.
                    pass

    def _tcp_to_serial_loop(self, client: socket.socket, ser, serial_mod) -> None:
        """Read from TCP client and write to serial port until disconnect."""

        while not self._stop.is_set():

            try:
                data = client.recv(RELAY_BUF_SIZE)
            except socket.timeout:
                continue
            except OSError as e:
                info(f"tcp→serial: recv error: {e}")
                break

            if not data:
                break

            self._tx += len(data)

            try:
                ser.write(data)
            except serial_mod.SerialException as e:
                warn(f"tcp→serial: serial write error: {e}")
                # Close the port so _run's is_open check triggers a reopen.
                # On Windows, PermissionError / "Access is denied" leaves the
                # pyserial is_open flag True even though the handle is dead.
                try:
                    ser.close()
                except Exception:
                    pass
                break

    def _flow_watchdog(self) -> None:
        """Flip RTS/CTS once if the radio stays mute while the client talks.

        A wrong flow-control mode fails silently: RTS is left deasserted and a
        flow-controlled radio simply never answers, which looks identical to a
        dead radio.  So if the client is sending but nothing comes back after a
        grace window, flip the mode and drop the port — _run reopens it, and
        cpcd's next retry gets through.  Only ever done once, so a genuinely
        dead radio does not cause the mode to oscillate.
        """
        grace = 6.0

        # Wait until the client has actually sent something.
        while not self._stop.is_set() and self._tx == 0:
            self._stop.wait(0.5)

        if self._stop.is_set() or self._flipped:
            return

        deadline = time.time() + grace

        while not self._stop.is_set() and time.time() < deadline:

            if self._rx > 0:
                return  # Radio answered — the current mode is fine.

            self._stop.wait(0.5)

        if self._stop.is_set() or self._rx > 0 or self._flipped:
            return

        self._flipped = True
        self._rtscts = not self._rtscts
        warn(
            f"Radio silent (client→radio={self._tx}, radio→client=0) after "
            f"{grace:.0f}s — flipping RTS/CTS flow control to "
            f"{'on' if self._rtscts else 'off'} and reopening the port"
        )

        if self._ser is not None:

            try:
                self._ser.close()
            except Exception:
                pass


# ---------------------------------------------------------------------------
# SSH tunnel
# ---------------------------------------------------------------------------
class SSHTunnel:
    """Manage an SSH reverse tunnel to a remote UNIX socket, auto-reconnecting.

    Binds a per-user UNIX socket on the remote (remote_socket) that forwards to
    the local TCP relay.  StreamLocalBindUnlink=yes makes sshd remove a stale
    socket left behind by an unclean prior session.  The remote-radios
    container bind-mounts remote_socket and runs socat (UNIX -> PTY) for cpcd.
    """

    def __init__(self, ssh_target: str, remote_socket: str, local_port: int):
        self.ssh_target = ssh_target
        self.remote_socket = remote_socket
        self.local_port = local_port
        self._stop = threading.Event()
        self._process: subprocess.Popen | None = None

    def start(self) -> None:
        """Start the tunnel in a background thread with auto-reconnect."""
        t = threading.Thread(target=self._run, daemon=True, name="ssh-tunnel")
        t.start()

    def stop(self) -> None:
        """Signal the tunnel to stop and remove the remote socket."""
        self._stop.set()

        if self._process:
            try:
                self._process.terminate()
            except OSError:
                pass

        # Best-effort cleanup of the remote socket on shutdown.
        try:
            subprocess.run(
                [
                    "ssh", "-o", "ConnectTimeout=5",
                    "-o", "ClearAllForwardings=yes",
                    self.ssh_target,
                    f"rm -f {self.remote_socket}",
                ],
                capture_output=True, timeout=10,
            )
        except Exception:
            pass

    def _ensure_remote_dir(self) -> None:
        """Create the 0700 socket dir (and its ~/.remote-radios parent)."""
        remote_dir = os.path.dirname(self.remote_socket)
        # Also lock down the top-level ~/.remote-radios tree for per-user
        # isolation (the socket lives under .../radios/).
        parent_dir = os.path.dirname(remote_dir)

        try:
            subprocess.run(
                [
                    "ssh", "-o", "ConnectTimeout=10",
                    "-o", "ClearAllForwardings=yes",
                    self.ssh_target,
                    f"mkdir -p {remote_dir} && chmod 700 {remote_dir} {parent_dir} && "
                    f"rm -f {self.remote_socket}; echo done",
                ],
                capture_output=True, text=True, timeout=15,
            )
        except (subprocess.TimeoutExpired, FileNotFoundError):
            warn("Could not prepare remote socket directory — continuing anyway.")

    def _verify_tunnel(self) -> bool:
        """Check that the remote UNIX socket exists (the -R forward bound)."""
        try:
            result = subprocess.run(
                [
                    "ssh", "-o", "ConnectTimeout=5",
                    "-o", "ClearAllForwardings=yes",
                    self.ssh_target,
                    f"test -S {self.remote_socket} && echo BOUND || echo NOTBOUND",
                ],
                capture_output=True, text=True, timeout=10,
            )

            output = result.stdout.strip()

            if "BOUND" in output and "NOTBOUND" not in output:
                ok(f"Verified: remote socket {self.remote_socket} is present.")
                return True

            warn(f"Remote socket {self.remote_socket} is NOT present — -R forward may have failed.")
            return False
        except (subprocess.TimeoutExpired, FileNotFoundError):
            warn("Could not verify tunnel — continuing anyway.")
            return True  # assume OK if we can't check

    def _run(self) -> None:
        backoff = 2

        while not self._stop.is_set():
            self._ensure_remote_dir()

            # ssh -R <remote-socket>:127.0.0.1:<local-port>
            tunnel_spec = f"{self.remote_socket}:127.0.0.1:{self.local_port}"
            cmd = [
                "ssh",
                "-N",                        # no remote command
                "-o", "ServerAliveInterval=15",
                "-o", "ServerAliveCountMax=3",
                "-o", "ConnectTimeout=10",
                "-o", "StreamLocalBindUnlink=yes",
                # Do NOT use ClearAllForwardings — it clears our -R too.
                "-o", "ExitOnForwardFailure=no",
                "-R", tunnel_spec,
                self.ssh_target,
            ]

            info(f"Opening SSH tunnel: remote {self.remote_socket} → localhost:{self.local_port}")

            try:
                self._process = subprocess.Popen(
                    cmd,
                    stdin=subprocess.DEVNULL,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                )
            except FileNotFoundError:
                die("ssh is not available on PATH.")

            time.sleep(3)

            if self._process.poll() is not None:
                rc = self._process.returncode
                stderr_out = ""

                try:
                    stderr_out = self._process.stderr.read().decode(errors="replace").strip()
                except Exception:
                    pass

                warn(f"SSH exited immediately (code {rc}){': ' + stderr_out if stderr_out else ''}")
                warn(f"Reconnecting in {backoff}s...")
                time.sleep(backoff)
                backoff = min(backoff * 2, 60)
                continue

            ok("SSH connection established.")

            if not self._verify_tunnel():
                warn("Killing SSH and retrying...")
                self._process.terminate()
                self._process.wait()
                time.sleep(2)
                continue

            backoff = 2
            self._process.wait()
            rc = self._process.returncode

            if self._stop.is_set():
                break

            stderr_out = ""

            try:
                stderr_out = self._process.stderr.read().decode(errors="replace").strip()
            except Exception:
                pass

            warn(f"SSH tunnel exited (code {rc}){': ' + stderr_out if stderr_out else ''}")
            warn(f"Reconnecting in {backoff}s...")
            time.sleep(backoff)
            backoff = min(backoff * 2, 60)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main() -> None:
    parser = argparse.ArgumentParser(
        description="Forward a local Silicon Labs radio serial port to a remote dev server.",
        epilog="See docs/REMOTE_RADIO_FOR_DEVELOPMENT.md for full setup instructions.",
    )
    parser.add_argument(
        "ssh_target",
        help="SSH destination, e.g. user@devserver.example.com",
    )
    parser.add_argument(
        "--port", "-p",
        dest="serial_port",
        default=None,
        help="Serial port of the radio (e.g. /dev/ttyACM0, COM3).  Auto-detected if omitted.",
    )
    parser.add_argument(
        "--baud", "-b",
        type=int,
        default=SERIAL_BAUD,
        help=f"Serial baud rate (default: {SERIAL_BAUD}).",
    )
    parser.add_argument(
        "--rtscts",
        dest="rtscts",
        action="store_true",
        default=None,
        help="Force RTS/CTS hardware flow control on (default: inferred from "
             "the adapter's USB vendor).",
    )
    parser.add_argument(
        "--no-rtscts",
        dest="rtscts",
        action="store_false",
        help="Force RTS/CTS hardware flow control off.",
    )
    parser.add_argument(
        "--socket", "-s",
        dest="remote_socket",
        default=None,
        help="Remote UNIX socket path served by the SSH reverse tunnel "
             "(default: /home/<remote-user>/.remote-radios/radios/silabs.sock).",
    )
    parser.add_argument(
        "--local-port",
        dest="local_port",
        type=int,
        default=BASE_PORT,
        help=f"Local TCP port for the serial relay (default: {BASE_PORT}). "
             "Loopback-only; the SSH tunnel forwards the remote socket to it.",
    )
    parser.add_argument(
        "--usbip",
        nargs="?",
        const="auto",
        default=None,
        metavar="BUSID",
        help="Also forward a usb-ip device (e.g. a Bluetooth dongle). "
             "Pass a bus ID, or omit the value to auto-select. On Windows the "
             "device is shared automatically; on Linux use remote-radios-setup.sh.",
    )
    parser.add_argument(
        "--usbip-socket",
        dest="usbip_socket",
        default=None,
        help="Remote UNIX socket for the forwarded usb-ip endpoint "
             "(default: /home/<remote-user>/.remote-radios/usbip/usbipd.sock).",
    )
    parser.add_argument(
        "--usbip-local-port",
        dest="usbip_local_port",
        type=int,
        default=USBIP_PORT,
        help=f"Local TCP port of the usb-ip daemon (default: {USBIP_PORT}).",
    )
    parser.add_argument(
        "--no-serial",
        dest="no_serial",
        action="store_true",
        help="Skip the serial radio and forward only the usb-ip device.",
    )

    args = parser.parse_args()

    # ── Resolve the remote user ───────────────────────────────────────────
    remote_user, _ = parse_ssh_target(args.ssh_target)

    if args.no_serial and args.usbip is None:
        die("--no-serial leaves nothing to forward.  Add --usbip.")

    # ── Detect radio ──────────────────────────────────────────────────────
    serial_port = None
    remote_socket = None

    if not args.no_serial:
        serial_port = args.serial_port

        if serial_port is None:
            info("Searching for Silicon Labs radio...")
            serial_port = find_radio_port()

            if serial_port is None:
                die(
                    "No Silicon Labs radio found.\n"
                    "  Looked for USB VID:PID  10C4:EA60 (CP210x)  or  1366:0105 (SEGGER J-Link)\n"
                    "  Specify the port manually with --port /dev/ttyACM0, or pass --no-serial."
                )

        ok(f"Radio serial port: {serial_port}")

        remote_socket = args.remote_socket or compute_default_socket(remote_user)
        ok(f"Remote socket: {remote_socket}")

    # ── Resolve the usb-ip device ─────────────────────────────────────────
    usbip_busid = None
    usbip_socket = None

    if args.usbip is not None:
        usbip_busid = select_usbip_busid(args.usbip)

        if usbip_busid is None:
            die("Could not resolve a usb-ip device to forward.")

        usbip_socket = args.usbip_socket or compute_usbip_socket(remote_user)
        ok(f"Remote usb-ip socket: {usbip_socket}")

    # ── Start the serial relay ────────────────────────────────────────────
    # The relay listens on loopback; the SSH reverse tunnel forwards the
    # remote UNIX socket to this local TCP port.
    local_port = args.local_port
    relay = None
    tunnel = None

    if serial_port is not None:
        rtscts, rtscts_src = resolve_rtscts(args.rtscts, port_vid_pid(serial_port))
        info(f"RTS/CTS flow control: {'on' if rtscts else 'off'} ({rtscts_src})")

        relay = SerialRelay(serial_port, local_port, args.baud, rtscts)
        relay.start()

        # ── Start the SSH tunnel ──────────────────────────────────────────
        tunnel = SSHTunnel(args.ssh_target, remote_socket, local_port)
        tunnel.start()

    # ── Start the usb-ip tunnel ───────────────────────────────────────────
    # No relay is needed here: the usb-ip daemon already listens on a local
    # TCP port, so the reverse tunnel points straight at it.
    usbip_tunnel = None

    if usbip_busid is not None:
        usbip_tunnel = SSHTunnel(
            args.ssh_target, usbip_socket, args.usbip_local_port
        )
        usbip_tunnel.start()

    # ── Print summary ─────────────────────────────────────────────────────
    print()
    print(f"{GREEN}{BOLD}======================================={NC}")
    print(f"{GREEN}{BOLD} REMOTE RADIO TUNNELS ACTIVE{NC}")
    print(f"{GREEN}{BOLD}======================================={NC}")

    if serial_port is not None:
        print(f"{GREEN} Radio:         {serial_port}{NC}")
        print(f"{GREEN} Local TCP:     127.0.0.1:{local_port}{NC}")
        print(f"{GREEN} Remote socket: {remote_socket}{NC}")

    if usbip_busid is not None:
        print(f"{GREEN} usb-ip device: {usbip_busid}{NC}")
        print(f"{GREEN} Local TCP:     127.0.0.1:{args.usbip_local_port}{NC}")
        print(f"{GREEN} Remote socket: {usbip_socket}{NC}")

    print(f"{GREEN}{BOLD}======================================={NC}")
    print()
    info("Keep this terminal open while you work (or let remote-radios-setup.sh manage it).")
    info("Press Ctrl-C to stop the tunnel.")
    print()

    # ── Wait for Ctrl-C ───────────────────────────────────────────────────
    stop_event = threading.Event()

    def handle_signal(sig, frame):
        info("Shutting down...")
        stop_event.set()

    signal.signal(signal.SIGINT, handle_signal)
    signal.signal(signal.SIGTERM, handle_signal)

    stop_event.wait()

    # ── Cleanup ───────────────────────────────────────────────────────────
    if usbip_tunnel is not None:
        usbip_tunnel.stop()

    if tunnel is not None:
        tunnel.stop()

    if relay is not None:
        relay.stop()

    print()
    print(f"{GREEN}{BOLD}======================================={NC}")
    print(f"{GREEN}{BOLD} TUNNELS STOPPED{NC}")
    print(f"{GREEN}{BOLD}======================================={NC}")


if __name__ == "__main__":
    main()
