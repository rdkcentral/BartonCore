#!/usr/bin/env python3
"""
CPC Socket Proxy - Tunnels SEQPACKET Unix sockets over TCP with message framing.

Self-healing proxy that automatically recovers from:
  - TCP connection drops (client or server container restart)
  - Unix socket disconnections (cpcd or application restart)
  - Stale sessions from previous connections

Each message is framed with a 4-byte network-order length prefix to preserve
SEQPACKET message boundaries over TCP.

Server mode (on cpcd host):
  - Listens on a TCP port and relays to/from a local CPC Unix socket
  - Only one active session per endpoint (CPC limitation); a new TCP client
    automatically terminates the previous session
  - TCP keepalive detects dead clients within ~25 seconds

Client mode (on remote host):
  - Creates a local Unix SEQPACKET socket mimicking cpcd
  - Each local connection gets a fresh TCP session to the server
  - Retries TCP connections (30 attempts) to handle server-side startup races

Usage:
  Server:  ./cpc_socket_proxy.py server -s /dev/shm/cpcd/cpcd_0/ctrl.cpcd.sock -p 5000
  Client:  ./cpc_socket_proxy.py client -s /dev/shm/cpcd/cpcd_0/ctrl.cpcd.sock -H <host> -p 5000
"""

import argparse
import os
import socket
import struct
import sys
import threading
import time
from pathlib import Path


def enable_tcp_keepalive(sock: socket.socket):
    """Enable aggressive TCP keepalive for fast stale-connection detection."""
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPIDLE, 10)
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPINTVL, 5)
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPCNT, 3)


def close_sockets(*sockets):
    """Shut down and close sockets, ignoring errors."""
    for s in sockets:
        try:
            s.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass

        try:
            s.close()
        except OSError:
            pass


def recv_framed(tcp_sock: socket.socket) -> bytes | None:
    """Receive a length-prefixed message from TCP socket."""
    header = b''

    while len(header) < 4:
        chunk = tcp_sock.recv(4 - len(header))

        if not chunk:
            return None

        header += chunk

    length = struct.unpack('!I', header)[0]

    if length == 0:
        return b''

    data = b''

    while len(data) < length:
        chunk = tcp_sock.recv(min(length - len(data), 65536))

        if not chunk:
            return None

        data += chunk

    return data


def send_framed(tcp_sock: socket.socket, data: bytes) -> bool:
    """Send a length-prefixed message to TCP socket."""
    try:
        header = struct.pack('!I', len(data))
        tcp_sock.sendall(header + data)
        return True
    except (BrokenPipeError, ConnectionResetError, OSError):
        return False


def relay_unix_to_tcp(unix_sock, tcp_sock, name, done):
    """Forward Unix -> TCP.  Closes both sockets on exit so the peer exits."""
    try:
        while True:
            data = unix_sock.recv(65536)

            if not data:
                break

            if not send_framed(tcp_sock, data):
                break
    except (ConnectionResetError, BrokenPipeError, OSError):
        pass
    finally:
        close_sockets(unix_sock, tcp_sock)
        done.set()

    print(f"[{name}] Unix->TCP stopped", flush=True)


def relay_tcp_to_unix(tcp_sock, unix_sock, name, done):
    """Forward TCP -> Unix.  Closes both sockets on exit so the peer exits."""
    try:
        while True:
            data = recv_framed(tcp_sock)

            if data is None:
                break

            unix_sock.send(data)
    except (ConnectionResetError, BrokenPipeError, OSError):
        pass
    finally:
        close_sockets(unix_sock, tcp_sock)
        done.set()

    print(f"[{name}] TCP->Unix stopped", flush=True)


def run_relay_pair(unix_sock, tcp_sock, name):
    """Start bidirectional relay threads and block until the session ends."""
    done = threading.Event()

    t1 = threading.Thread(
        target=relay_unix_to_tcp,
        args=(unix_sock, tcp_sock, name, done),
        daemon=True,
    )
    t2 = threading.Thread(
        target=relay_tcp_to_unix,
        args=(tcp_sock, unix_sock, name, done),
        daemon=True,
    )
    t1.start()
    t2.start()

    # Block until either direction fails.
    done.wait()

    # Ensure both sockets are closed so the surviving thread also exits.
    close_sockets(unix_sock, tcp_sock)
    t1.join(timeout=5)
    t2.join(timeout=5)

    print(f"[{name}] Session ended", flush=True)


def run_server(socket_path: str, port: int, bind_addr: str = '0.0.0.0'):
    """
    Server mode: accept TCP clients and relay to the local CPC Unix socket.

    Multiple clients may connect concurrently (e.g. both the HCI bridge and
    otbr-agent need their own ctrl session).  Each TCP client gets its own
    independent Unix socket connection to cpcd and a dedicated relay pair.
    """

    name = os.path.basename(socket_path)
    print(f"[{name}] Server: TCP :{port} <-> {socket_path}", flush=True)

    tcp_server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    tcp_server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    tcp_server.bind((bind_addr, port))
    tcp_server.listen(5)

    while True:
        tcp_client, addr = tcp_server.accept()
        enable_tcp_keepalive(tcp_client)
        print(f"[{name}] TCP connection from {addr}", flush=True)

        # cpcd creates endpoint sockets on-demand when a client opens
        # them via the control socket.  In the remote-access scenario the
        # open-endpoint command arrives over the proxied ctrl socket, so
        # the endpoint socket may not exist yet when the TCP client for
        # that endpoint connects moments later.  Retry with backoff to
        # give cpcd time to create the socket (the firmware can take
        # several seconds to make an endpoint available after a reset).
        unix_sock = None
        max_attempts = 20
        retry_delay = 0.5

        for attempt in range(max_attempts):

            try:
                s = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
                s.connect(socket_path)
                unix_sock = s
                break
            except Exception as e:
                s.close()

                if attempt < max_attempts - 1:
                    time.sleep(retry_delay)
                else:
                    print(
                        f"[{name}] Unix connect to {socket_path} "
                        f"failed after {attempt + 1} attempts: {e}",
                        flush=True,
                    )

        if unix_sock is None:
            tcp_client.close()
            continue

        def session(us=unix_sock, ts=tcp_client, n=name):
            run_relay_pair(us, ts, n)

        threading.Thread(target=session, daemon=True).start()


def run_client(socket_path: str, host: str, port: int):
    """
    Client mode: create a local CPC-compatible Unix socket and relay to TCP.

    When a relay session dies (TCP drop, Unix disconnect, container restart),
    the proxy goes back to accept() and waits for the next local client.
    The CPC library inside the application will reconnect automatically.
    """
    name = os.path.basename(socket_path)
    print(f"[{name}] Client: {socket_path} <-> TCP {host}:{port}", flush=True)

    socket_dir = os.path.dirname(socket_path)
    Path(socket_dir).mkdir(parents=True, exist_ok=True)

    try:
        os.unlink(socket_path)
    except FileNotFoundError:
        pass

    unix_server = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    unix_server.bind(socket_path)
    unix_server.listen(5)
    os.chmod(socket_path, 0o777)

    while True:
        unix_client, _ = unix_server.accept()
        print(f"[{name}] Local client connected", flush=True)

        tcp_sock = None

        for attempt in range(30):

            try:
                tcp_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                tcp_sock.connect((host, port))
                enable_tcp_keepalive(tcp_sock)
                break
            except Exception as e:
                tcp_sock.close()
                tcp_sock = None

                if attempt < 29:
                    time.sleep(1)
                else:
                    print(f"[{name}] TCP connect to {host}:{port} "
                          f"failed after 30 attempts: {e}", flush=True)

        if tcp_sock is None:
            unix_client.close()
            continue

        print(f"[{name}] Connected to {host}:{port}", flush=True)

        def session(us=unix_client, ts=tcp_sock, n=name):
            run_relay_pair(us, ts, n)

        threading.Thread(target=session, daemon=True).start()


def main():
    parser = argparse.ArgumentParser(description='CPC Socket Proxy')
    subparsers = parser.add_subparsers(dest='mode', required=True)

    server_parser = subparsers.add_parser('server', help='Run as server (on cpcd host)')
    server_parser.add_argument('--socket', '-s', required=True, help='Unix socket path')
    server_parser.add_argument('--port', '-p', type=int, required=True, help='TCP port')
    server_parser.add_argument('--bind', '-b', default='0.0.0.0', help='Bind address')

    client_parser = subparsers.add_parser('client', help='Run as client (on remote host)')
    client_parser.add_argument('--socket', '-s', required=True, help='Unix socket path to create')
    client_parser.add_argument('--host', '-H', required=True, help='Remote cpcd host')
    client_parser.add_argument('--port', '-p', type=int, required=True, help='Remote TCP port')

    args = parser.parse_args()

    if args.mode == 'server':
        run_server(args.socket, args.port, args.bind)
    else:
        run_client(args.socket, args.host, args.port)


if __name__ == '__main__':
    main()
