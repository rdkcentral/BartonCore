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
Remote Serial Tunnel — forward a local Silicon Labs radio's serial port to a
remote dev server via SSH port forwarding.

Run this on the WORKSTATION (Windows or Linux) where the BRD2703 xG24 radio is
physically connected via USB.  It:

  1. Auto-detects the Silicon Labs radio serial port.
  2. Computes a per-user TCP port from the remote UID (base 20000 + UID) and
     a per-user loopback address (127.0.<hi>.<lo>) to avoid conflicts on
     shared servers.
  3. Starts a local TCP server that relays bytes between the serial port and
     TCP clients.
  4. Opens an SSH reverse tunnel so the remote dev server can reach the
     local TCP server.  The otbr-radio container's entrypoint connects
     socat to this port, creating a virtual serial device for cpcd.
  5. Self-heals: reconnects serial and SSH on failure with backoff.

Requirements (workstation):
  - Python 3.8+
  - pyserial  (pip install pyserial)
  - ssh client on PATH

Usage:
  python remote-serial.py user@devserver.example.com
  python remote-serial.py user@devserver.example.com --port /dev/ttyACM0
  python remote-serial.py user@devserver.example.com --port COM3

The script keeps running until Ctrl-C.  The SSH tunnel and serial relay
are restarted automatically if either side disconnects.

See docs/REMOTE_RADIO_FOR_DEVELOPMENT.md for full setup instructions.
"""

import argparse
import datetime
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
SERIAL_BAUD = 115200
BASE_PORT = 20000
RELAY_BUF_SIZE = 4096

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


def compute_tunnel_addr(uid: int) -> tuple[str, int]:
    """Derive per-user tunnel port from UID.

    Port: 20000 + UID  (unique per user on shared servers)

    The SSH reverse tunnel binds to 0.0.0.0 on the remote so it is
    reachable from Docker containers (which cannot access the host's
    loopback directly).  Per-user port isolation prevents conflicts.
    """
    port = BASE_PORT + uid

    return "0.0.0.0", port


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

    def __init__(self, serial_port: str, listen_port: int, baud: int = SERIAL_BAUD):
        self.serial_port = serial_port
        self.listen_port = listen_port
        self.baud = baud
        self._stop = threading.Event()
        self._server_sock: socket.socket | None = None
        # Guarded by _client_lock.  Set to the active TCP socket or None.
        self._client: socket.socket | None = None
        self._client_lock = threading.Lock()

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
                    rtscts=True,
                    timeout=0.1,
                )
                ok(f"Serial port {self.serial_port} opened ({self.baud} baud, hw flow control)")
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

            try:
                ser.write(data)
            except serial_mod.SerialException as e:
                info(f"tcp→serial: serial write error: {e}")
                break


# ---------------------------------------------------------------------------
# SSH tunnel
# ---------------------------------------------------------------------------
class SSHTunnel:
    """Manage an SSH reverse tunnel with auto-reconnect.

    Before opening the tunnel, kills any stale SSH processes on the remote
    that are still holding the port.  After opening, verifies the -R forward
    actually bound on the remote side.
    """

    def __init__(self, ssh_target: str, remote_addr: str, remote_port: int, local_port: int):
        self.ssh_target = ssh_target
        self.remote_addr = remote_addr
        self.remote_port = remote_port
        self.local_port = local_port
        self._stop = threading.Event()
        self._process: subprocess.Popen | None = None

    def start(self) -> None:
        """Start the tunnel in a background thread with auto-reconnect."""
        t = threading.Thread(target=self._run, daemon=True, name="ssh-tunnel")
        t.start()

    def stop(self) -> None:
        """Signal the tunnel to stop."""
        self._stop.set()

        if self._process:
            try:
                self._process.terminate()
            except OSError:
                pass

    def _kill_stale_listeners(self) -> None:
        """Kill any process on the remote that is holding our tunnel port."""
        info(f"Checking for stale listeners on remote port {self.remote_port}...")

        try:
            result = subprocess.run(
                [
                    "ssh", "-o", "ConnectTimeout=10",
                    "-o", "ClearAllForwardings=yes",
                    self.ssh_target,
                    f"fuser -k {self.remote_port}/tcp 2>/dev/null; echo done",
                ],
                capture_output=True,
                text=True,
                timeout=15,
            )

            if result.returncode == 0:
                info("Cleared stale listeners (if any).")
        except (subprocess.TimeoutExpired, FileNotFoundError):
            warn("Could not check for stale listeners — continuing anyway.")

    def _verify_tunnel(self) -> bool:
        """Check that the -R forward is actually listening on the remote."""
        try:
            result = subprocess.run(
                [
                    "ssh", "-o", "ConnectTimeout=5",
                    "-o", "ClearAllForwardings=yes",
                    self.ssh_target,
                    f"ss -tlnp 2>/dev/null | grep -q ':{self.remote_port} ' && echo BOUND || echo NOTBOUND",
                ],
                capture_output=True,
                text=True,
                timeout=10,
            )

            output = result.stdout.strip()

            if "BOUND" in output and "NOTBOUND" not in output:
                ok(f"Verified: remote port {self.remote_port} is bound.")
                return True
            else:
                warn(f"Remote port {self.remote_port} is NOT bound — tunnel -R forward may have failed.")
                return False
        except (subprocess.TimeoutExpired, FileNotFoundError):
            warn("Could not verify tunnel — continuing anyway.")
            return True  # assume OK if we can't check

    def _run(self) -> None:
        backoff = 2

        while not self._stop.is_set():
            # Kill any stale SSH tunnels holding our port on the remote.
            self._kill_stale_listeners()

            # Brief pause to let the port be released.
            time.sleep(1)

            tunnel_spec = f"{self.remote_addr}:{self.remote_port}:127.0.0.1:{self.local_port}"
            cmd = [
                "ssh",
                "-N",                        # no remote command
                "-o", "ServerAliveInterval=15",
                "-o", "ServerAliveCountMax=3",
                "-o", "ConnectTimeout=10",
                # Do NOT use ClearAllForwardings — it also clears command-line
                # -R forwards.  Instead, tolerate local forward failures from
                # ~/.ssh/config with ExitOnForwardFailure=no.  Our reverse
                # forward is verified separately after startup.
                "-o", "ExitOnForwardFailure=no",
                "-R", tunnel_spec,
                self.ssh_target,
            ]

            info(f"Opening SSH tunnel: {self.remote_addr}:{self.remote_port} → localhost:{self.local_port}")

            try:
                self._process = subprocess.Popen(
                    cmd,
                    stdin=subprocess.DEVNULL,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                )
            except FileNotFoundError:
                die("ssh is not available on PATH.")

            # Give SSH a moment to establish the connection and set up forwards.
            time.sleep(3)

            if self._process.poll() is not None:
                # SSH already exited.
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

            # Verify the -R forward actually bound on the remote.
            if not self._verify_tunnel():
                warn("Killing SSH and retrying with stale listener cleanup...")
                self._process.terminate()
                self._process.wait()
                time.sleep(2)
                continue

            backoff = 2

            # Wait for the SSH process to exit.
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

    args = parser.parse_args()

    # ── Detect radio ──────────────────────────────────────────────────────
    serial_port = args.serial_port

    if serial_port is None:
        info("Searching for Silicon Labs radio...")
        serial_port = find_radio_port()

        if serial_port is None:
            die(
                "No Silicon Labs radio found.\n"
                "  Looked for USB VID:PID  10C4:EA60 (CP210x)  or  1366:0105 (SEGGER J-Link)\n"
                "  Specify the port manually with --port /dev/ttyACM0  or  --port COM3"
            )

    ok(f"Radio serial port: {serial_port}")

    # ── Resolve remote UID and compute tunnel parameters ──────────────────
    remote_uid = get_remote_uid(args.ssh_target)
    tunnel_addr, tunnel_port = compute_tunnel_addr(remote_uid)
    ok(f"Remote UID: {remote_uid}")
    ok(f"Tunnel port: {tunnel_port}")

    # ── Start the serial relay ────────────────────────────────────────────
    # Use an ephemeral local port for the TCP server.  The SSH tunnel
    # forwards from the remote side to this local port.
    local_port = tunnel_port  # keep it simple — same port locally
    relay = SerialRelay(serial_port, local_port, args.baud)
    relay.start()

    # ── Start the SSH tunnel ──────────────────────────────────────────────
    tunnel = SSHTunnel(args.ssh_target, tunnel_addr, tunnel_port, local_port)
    tunnel.start()

    # ── Print summary ─────────────────────────────────────────────────────
    print()
    print(f"{GREEN}{BOLD}======================================={NC}")
    print(f"{GREEN}{BOLD} REMOTE SERIAL TUNNEL ACTIVE{NC}")
    print(f"{GREEN}{BOLD}======================================={NC}")
    print(f"{GREEN} Radio:       {serial_port}{NC}")
    print(f"{GREEN} Local TCP:   127.0.0.1:{local_port}{NC}")
    print(f"{GREEN} Tunnel port: {tunnel_port} on remote (all interfaces){NC}")
    print(f"{GREEN} Remote UID:  {remote_uid}{NC}")
    print(f"{GREEN}{BOLD}======================================={NC}")
    print()
    info("Keep this terminal open while you work.")
    info("Press Ctrl-C to stop the tunnel.")
    print()
    info("On the remote dev server, set in docker/.env or export:")
    print()
    print(f"    {BOLD}RADIO_PORT={tunnel_port}{NC}")
    print()
    info("Then rebuild the devcontainer or run:")
    print()
    print(f"    {BOLD}RADIO_PORT={tunnel_port} ./dockerw -T bash{NC}")
    print()

    # ── Wait for Ctrl-C ───────────────────────────────────────────────────
    stop_event = threading.Event()

    def handle_signal(sig, frame):
        info("Shutting down...")
        stop_event.set()

    signal.signal(signal.SIGINT, handle_signal)

    if platform.system() != "Windows":
        signal.signal(signal.SIGTERM, handle_signal)

    stop_event.wait()

    # ── Cleanup ───────────────────────────────────────────────────────────
    tunnel.stop()
    relay.stop()

    print()
    print(f"{GREEN}{BOLD}======================================={NC}")
    print(f"{GREEN}{BOLD} TUNNEL STOPPED{NC}")
    print(f"{GREEN}{BOLD}======================================={NC}")


if __name__ == "__main__":
    main()
