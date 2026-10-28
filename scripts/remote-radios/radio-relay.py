#!/usr/bin/env python3
"""Relay a remote radio's host links between two machines.

Three kinds of link are relayed:

  cpcd's client sockets (modes agent and proxy), described below;
  zigbeed's EZSP serial link (mode ezsp), described at run_ezsp(); and
  changes to zigbeed's tokens file (mode notify), described at run_notify().


cpcd talks to its clients (otbr-agent, zigbeed) over local AF_UNIX
SOCK_SEQPACKET sockets in /dev/shm/cpcd/<instance>/, and has no remote
client support.  This relays those sockets over a byte stream, so cpcd can
run next to the radio while a client runs elsewhere:

  agent  runs next to cpcd.  Listens on a TCP port (normally 127.0.0.1,
         reached through an `ssh -R` tunnel).  Each incoming connection
         names one cpcd socket, which the agent connects to and relays.

  proxy  runs next to the client.  Recreates cpcd's sockets in a socket
         directory and relays every client connection to the agent over a
         new stream to UPSTREAM (a UNIX socket or host:port).

Stream protocol, per connection:
  client -> agent   "CONNECT <name>\\n"   <name> is ctrl, reset or ep<N>
  agent -> client   "OK\\n" or "ERR <reason>\\n"
  then both ways    4-byte big-endian length, then that many bytes; one
                    frame per SEQPACKET message.  A zero-length frame is a
                    zero-length message (cpcd's reset notification).
                    Closing the stream closes the socket at the other end.

cpcd creates an endpoint's socket only when a client asks to open it, and
before answering, so the proxy watches for OPEN_ENDPOINT requests on the
control socket and creates the matching ep<N> socket before passing the
request on.
"""

import argparse
import os
import re
import select
import signal
import socket
import struct
import subprocess
import sys
import threading
import time

MAX_MESSAGE = 65536          # well above cpcd's largest message (4087)
OPEN_ENDPOINT = 1            # cpcd control message type
NAME_RE = re.compile(r"^(ctrl|reset|ep([0-9]{1,3}))$")


def log(msg):
    print(f"[radio-relay] {msg}", file=sys.stderr, flush=True)


def recv_exact(sock, n):
    buf = b""
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            return None
        buf += chunk
    return buf


def recv_line(sock, limit=128):
    buf = b""
    while not buf.endswith(b"\n"):
        if len(buf) >= limit:
            return None
        c = sock.recv(1)
        if not c:
            return None
        buf += c
    return buf[:-1].decode("ascii", "replace")


def read_frame(stream):
    """Next message from the stream: bytes (possibly empty), or None at EOF."""
    hdr = recv_exact(stream, 4)
    if hdr is None:
        return None
    (length,) = struct.unpack(">I", hdr)
    if length > MAX_MESSAGE:
        raise ValueError(f"frame of {length} bytes")
    return recv_exact(stream, length) if length else b""


def write_frame(stream, data):
    stream.sendall(struct.pack(">I", len(data)) + data)


def pump(seq, stream, on_seq_message=None):
    """Relay between a SEQPACKET socket and a framed stream until either
    side closes, then close both."""
    try:
        while True:
            ready, _, _ = select.select([seq, stream], [], [])
            if seq in ready:
                # recv returns b"" both for a zero-length message (cpcd's
                # reset notification) and when the peer has closed; only
                # the latter shows as a hang-up.
                hup = _hung_up(seq)
                data = seq.recv(MAX_MESSAGE)
                if not data and hup:
                    break
                if on_seq_message is not None:
                    on_seq_message(data)
                write_frame(stream, data)
            if stream in ready:
                data = read_frame(stream)
                if data is None:
                    break
                seq.send(data)
    except (OSError, ValueError):
        pass
    finally:
        for s in (seq, stream):
            try:
                s.close()
            except OSError:
                pass


def _hung_up(seq):
    p = select.poll()
    p.register(seq, select.POLLRDHUP | select.POLLHUP)
    return bool(p.poll(0))


# --------------------------------------------------------------------- agent

def agent_connection(stream, socket_dir):
    try:
        line = recv_line(stream) or ""
        verb, _, name = line.partition(" ")
        if verb != "CONNECT" or not NAME_RE.match(name):
            stream.sendall(b"ERR bad request\n")
            stream.close()
            return
        path = os.path.join(socket_dir, f"{name}.cpcd.sock")
        seq = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        try:
            seq.connect(path)
        except OSError as e:
            seq.close()
            stream.sendall(f"ERR {e.strerror or e}\n".encode())
            stream.close()
            return
        stream.sendall(b"OK\n")
    except OSError:
        stream.close()
        return
    pump(seq, stream)


def run_agent(args):
    host, _, port = args.listen.rpartition(":")
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind((host or "127.0.0.1", int(port)))
    srv.listen(16)
    log(f"agent: relaying {args.socket_dir} on {args.listen}")
    while True:
        stream, _ = srv.accept()
        stream.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        threading.Thread(target=agent_connection,
                         args=(stream, args.socket_dir), daemon=True).start()


# --------------------------------------------------------------------- proxy

class Proxy:
    def __init__(self, socket_dir, upstream):
        self.socket_dir = socket_dir
        self.upstream = upstream
        self.listening = set()
        self.lock = threading.Lock()

    def open_upstream(self, name):
        if self.upstream.startswith("/"):
            s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            s.connect(self.upstream)
        else:
            host, _, port = self.upstream.rpartition(":")
            s = socket.create_connection((host, int(port)))
            s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        s.sendall(f"CONNECT {name}\n".encode())
        reply = recv_line(s)
        if reply != "OK":
            s.close()
            raise OSError(reply or "no reply from agent")
        return s

    def listen(self, name):
        with self.lock:
            if name in self.listening:
                return
            path = os.path.join(self.socket_dir, f"{name}.cpcd.sock")
            try:
                os.unlink(path)
            except FileNotFoundError:
                pass
            srv = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
            srv.bind(path)
            srv.listen(16)
            self.listening.add(name)
        threading.Thread(target=self.accept_loop, args=(srv, name),
                         daemon=True).start()

    def accept_loop(self, srv, name):
        while True:
            client, _ = srv.accept()
            threading.Thread(target=self.serve, args=(client, name),
                             daemon=True).start()

    def on_ctrl_message(self, data):
        # Endpoint sockets must exist before the client is told it may
        # connect to them.
        if len(data) >= 2 and data[0] == OPEN_ENDPOINT:
            self.listen(f"ep{data[1]}")

    def serve(self, client, name):
        try:
            stream = self.open_upstream(name)
        except OSError as e:
            log(f"proxy: {name}: cannot reach agent: {e}")
            client.close()
            return
        pump(client, stream,
             self.on_ctrl_message if name == "ctrl" else None)


def run_proxy(args):
    os.makedirs(args.socket_dir, mode=0o770, exist_ok=True)
    for f in os.listdir(args.socket_dir):
        if f.endswith(".cpcd.sock"):
            os.unlink(os.path.join(args.socket_dir, f))
    proxy = Proxy(args.socket_dir, args.upstream)
    proxy.listen("reset")
    proxy.listen("ctrl")       # last: clients wait for ctrl to appear
    log(f"proxy: serving {args.socket_dir} via {args.upstream}")
    threading.Event().wait()


# ---------------------------------------------------------------------- ezsp

def run_ezsp(args):
    """Give zigbeed a serial port whose far end is a network connection.

    zigbeed speaks EZSP over a tty.  This holds a PTY open for the whole run
    and links it at LINK, so zigbeed can open it before anyone connects and
    keeps it across connections: a dropped tunnel must not take the tty away
    from zigbeed.  One connection at a time is relayed to the PTY; a new one
    replaces the old.  While nothing is connected, zigbeed's output is
    discarded, and the EZSP host resynchronises (ASH reset) on connecting.
    """
    import termios
    import tty

    master, slave = os.openpty()
    tty.setraw(slave)
    attrs = termios.tcgetattr(slave)
    attrs[3] &= ~termios.ECHO
    termios.tcsetattr(slave, termios.TCSANOW, attrs)
    # Keeping the slave open stops the master reporting EIO whenever zigbeed
    # closes and reopens the port.
    tmp = args.link + ".tmp"
    try:
        os.unlink(tmp)
    except FileNotFoundError:
        pass
    os.symlink(os.ttyname(slave), tmp)
    os.replace(tmp, args.link)

    host, _, port = args.listen.rpartition(":")
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind((host or "127.0.0.1", int(port)))
    srv.listen(4)
    log(f"ezsp: {args.link} on {args.listen}")

    conn = None
    try:
        while True:
            watch = [master, srv] + ([conn] if conn else [])
            ready, _, _ = select.select(watch, [], [])
            if srv in ready:
                new, _ = srv.accept()
                new.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                if conn:
                    conn.close()
                    log("ezsp: replaced the previous connection")
                conn = new
            if master in ready:
                data = os.read(master, 4096)
                if conn:
                    try:
                        conn.sendall(data)
                    except OSError:
                        conn.close()
                        conn = None
            if conn and conn in ready:
                data = conn.recv(4096)
                if data:
                    os.write(master, data)
                else:
                    conn.close()
                    conn = None
    finally:
        try:
            if os.readlink(args.link) == os.ttyname(slave):
                os.unlink(args.link)
        except OSError:
            pass


def run_notify(args):
    """Report changes to zigbeed's tokens file by running a command.

    ZigbeeCore learns that the Zigbee network's configuration changed by
    watching zigbeed's tokens file, then fetches the new configuration from
    zigbeed over EZSP.  With zigbeed on another machine the file is out of its
    reach, so this watches it here and runs COMMAND (which touches ZigbeeCore's
    copy) after each change.  Changes within --settle seconds of each other are
    reported once.  A report that fails is not retried: this only mirrors a
    convenience signal, and a missed one does no harm.
    """
    path = os.path.expanduser(args.file)
    def mtime():
        try:
            return os.stat(path).st_mtime_ns
        except FileNotFoundError:
            return None

    last = mtime()
    pending_since = None
    log(f"watching {path}")
    while True:
        time.sleep(args.interval)
        now = mtime()
        if now != last:
            last = now
            pending_since = time.monotonic()
        elif pending_since is not None and time.monotonic() - pending_since >= args.settle:
            pending_since = None
            rc = subprocess.call(args.command)
            if rc == 0:
                log("reported a tokens change")
            else:
                log(f"could not report a tokens change (exit {rc})")


def main():
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = p.add_subparsers(dest="mode", required=True)
    a = sub.add_parser("agent", help="run next to cpcd")
    a.add_argument("--socket-dir", required=True,
                   help="cpcd's socket directory, e.g. /dev/shm/cpcd/cpcd_rr")
    a.add_argument("--listen", default="127.0.0.1:20001",
                   help="host:port to listen on (default 127.0.0.1:20001)")
    x = sub.add_parser("proxy", help="run next to the cpcd clients")
    x.add_argument("--socket-dir", required=True,
                   help="where to create cpcd's sockets, e.g. /dev/shm/cpcd/cpcd_0")
    x.add_argument("--upstream", required=True,
                   help="the agent: a UNIX socket path or host:port")
    e = sub.add_parser("ezsp", help="give zigbeed a tty relayed over TCP")
    e.add_argument("--link", required=True,
                   help="path of the tty for zigbeed, e.g. ~/.local/state/ttyZigbeeNCP")
    e.add_argument("--listen", default="127.0.0.1:20002",
                   help="host:port to listen on (default 127.0.0.1:20002)")
    n = sub.add_parser("notify", help="run a command when zigbeed's tokens change")
    n.add_argument("--file", required=True, help="zigbeed's tokens file")
    n.add_argument("--interval", type=float, default=1.0,
                   help="seconds between checks (default 1)")
    n.add_argument("--settle", type=float, default=2.0,
                   help="seconds without change before reporting (default 2)")
    n.add_argument("command", nargs=argparse.REMAINDER,
                   help="command to run after a change (after --)")
    args = p.parse_args()
    if args.mode == "notify":
        if args.command[:1] == ["--"]:
            args.command = args.command[1:]
        if not args.command:
            p.error("notify needs a command, after --")
    # Run cleanup (finally blocks) when stopped by a service manager too.
    signal.signal(signal.SIGTERM, lambda *_: sys.exit(0))
    try:
        {"agent": run_agent, "proxy": run_proxy, "ezsp": run_ezsp,
         "notify": run_notify}[args.mode](args)
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
