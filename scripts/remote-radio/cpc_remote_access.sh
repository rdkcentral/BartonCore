#!/bin/bash
#
# CPC Remote Access - Start all socket proxies for remote libcpc access
#
# This script starts proxy processes to tunnel all cpcd Unix sockets over TCP,
# allowing libcpc clients to run on remote machines.
#
# Usage:
#   On cpcd host:     ./cpc_remote_access.sh server [instance_name] [base_port]
#   On remote client: ./cpc_remote_access.sh client <cpcd_host> [instance_name] [base_port]
#
# Port assignments (from base_port):
#   base_port + 0: ctrl.cpcd.sock
#   base_port + 1: reset.cpcd.sock
#   base_port + 100-355: ep0-ep255.cpcd.sock
#   base_port + 400-655: ep0-ep255.event.cpcd.sock
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROXY_SCRIPT="$SCRIPT_DIR/cpc_socket_proxy.py"

MODE="${1:-}"
INSTANCE="${3:-cpcd_0}"
BASE_PORT="${4:-15000}"
SOCKET_DIR="${CPC_SOCKET_DIR:-/dev/shm}"

usage() {
    echo "Usage:"
    echo "  On cpcd host:     $0 server [instance_name] [base_port]"
    echo "  On remote client: $0 client <cpcd_host> [instance_name] [base_port]"
    echo ""
    echo "Options:"
    echo "  instance_name  CPC daemon instance name (default: cpcd_0)"
    echo "  base_port      Starting port number (default: 15000)"
    echo ""
    echo "Environment:"
    echo "  CPC_SOCKET_DIR  Socket directory (default: /dev/shm)"
    exit 1
}

cleanup() {
    echo "Stopping all proxy processes..."
    jobs -p | xargs -r kill 2>/dev/null || true
    wait 2>/dev/null || true
    echo "Cleanup complete"
}

trap cleanup EXIT INT TERM

start_server() {
    local socket_base="$SOCKET_DIR/cpcd/$INSTANCE"

    if [[ ! -d "$socket_base" ]]; then
        echo "Error: Socket directory $socket_base does not exist"
        echo "Is cpcd running with instance name '$INSTANCE'?"
        exit 1
    fi

    echo "Starting CPC remote access server for instance '$INSTANCE'"
    echo "Socket directory: $socket_base"
    echo "Base port: $BASE_PORT"
    echo ""

    # Track socket_name -> PID so we can detect and restart dead proxies.
    declare -A PROXIED_PIDS

    start_proxy() {
        local sock="$1" port="$2"
        local filename
        filename=$(basename "$sock")
        python3 "$PROXY_SCRIPT" server -s "$sock" -p "$port" &
        PROXIED_PIDS["$filename"]=$!
        echo "  $filename -> port $port (PID $!)"
    }

    # Control socket
    echo "Starting core socket proxies:"
    start_proxy "$socket_base/ctrl.cpcd.sock" "$BASE_PORT"

    # Reset socket
    start_proxy "$socket_base/reset.cpcd.sock" "$((BASE_PORT + 1))"

    # proxy_endpoint_sockets scans the socket directory and starts a proxy for
    # any endpoint socket that doesn't have one yet.  It also restarts proxies
    # that have died.  cpcd creates endpoint sockets on-demand when clients
    # open endpoints, so we must poll for new ones.
    proxy_endpoint_sockets() {
        for sock in "$socket_base"/ep*.cpcd.sock; do
            [[ -S "$sock" ]] || continue
            local filename
            filename=$(basename "$sock")

            # If we already started a proxy, check if it's still alive.
            if [[ -n "${PROXIED_PIDS[$filename]+x}" ]]; then
                local pid="${PROXIED_PIDS[$filename]}"

                if kill -0 "$pid" 2>/dev/null; then
                    continue
                fi

                echo "Proxy for $filename (PID $pid) died — restarting"
                unset 'PROXIED_PIDS[$filename]'
            fi

            if [[ "$filename" =~ ^ep([0-9]+)\.event\.cpcd\.sock$ ]]; then
                local ep_num="${BASH_REMATCH[1]}"
                local event_port=$((BASE_PORT + 400 + ep_num))
                start_proxy "$sock" "$event_port"
            elif [[ "$filename" =~ ^ep([0-9]+)\.cpcd\.sock$ ]]; then
                local ep_num="${BASH_REMATCH[1]}"
                local ep_port=$((BASE_PORT + 100 + ep_num))
                start_proxy "$sock" "$ep_port"
            fi
        done
    }

    # Also monitor the core proxies (ctrl, reset) and restart them if dead.
    check_core_proxies() {
        for filename in ctrl.cpcd.sock reset.cpcd.sock; do
            [[ -n "${PROXIED_PIDS[$filename]+x}" ]] || continue
            local pid="${PROXIED_PIDS[$filename]}"

            if ! kill -0 "$pid" 2>/dev/null; then
                echo "Core proxy for $filename (PID $pid) died — restarting"
                unset 'PROXIED_PIDS[$filename]'
                local port
                case "$filename" in
                    ctrl.cpcd.sock)  port="$BASE_PORT" ;;
                    reset.cpcd.sock) port="$((BASE_PORT + 1))" ;;
                esac
                start_proxy "$socket_base/$filename" "$port"
            fi
        done
    }

    # Pre-start TCP listeners for well-known CPC endpoints that remote
    # clients need.  cpcd creates endpoint sockets on-demand when a client
    # opens them, but the proxy TCP listener must be ready BEFORE the
    # remote client tries to connect.  The proxy server retries the local
    # unix socket connection, giving cpcd time to create it after the
    # remote client opens the endpoint via the control socket.
    #
    # Silicon Labs CPC v4.4.x well-known endpoints:
    #   12  SL_CPC_ENDPOINT_15_4       (802.15.4 / Spinel / OpenThread)
    #   14  SL_CPC_ENDPOINT_BLUETOOTH   (Bluetooth RCP / HCI bridge)
    WELL_KNOWN_ENDPOINTS="12 14"
    echo ""
    echo "Starting well-known endpoint proxies:"

    for ep in ${WELL_KNOWN_ENDPOINTS}; do
        start_proxy "$socket_base/ep${ep}.cpcd.sock" "$((BASE_PORT + 100 + ep))"
        start_proxy "$socket_base/ep${ep}.event.cpcd.sock" "$((BASE_PORT + 400 + ep))"
    done

    # Proxy any other endpoint sockets that already exist.
    proxy_endpoint_sockets

    echo ""
    echo "Server started. Watching for new/dead endpoint sockets..."
    echo ""
    echo "On the remote client, run:"
    echo "  $0 client <this-host-ip> $INSTANCE $BASE_PORT"

    # check_wellknown_proxies — restart pre-started endpoint proxies if dead.
    # Unlike proxy_endpoint_sockets (which only checks existing socket files),
    # this always checks the well-known endpoints regardless of whether the
    # socket file exists on disk.
    check_wellknown_proxies() {
        for ep in ${WELL_KNOWN_ENDPOINTS}; do

            for suffix in ".cpcd.sock" ".event.cpcd.sock"; do
                local filename="ep${ep}${suffix}"
                [[ -n "${PROXIED_PIDS[$filename]+x}" ]] || continue
                local pid="${PROXIED_PIDS[$filename]}"

                if ! kill -0 "$pid" 2>/dev/null; then
                    echo "Well-known proxy for $filename (PID $pid) died — restarting"
                    unset 'PROXIED_PIDS[$filename]'
                    local port

                    if [[ "$suffix" == ".event.cpcd.sock" ]]; then
                        port=$((BASE_PORT + 400 + ep))
                    else
                        port=$((BASE_PORT + 100 + ep))
                    fi
                    start_proxy "$socket_base/$filename" "$port"
                fi
            done
        done
    }

    # Poll for new endpoint sockets and restart dead proxies.
    while true; do
        sleep 2
        proxy_endpoint_sockets
        check_core_proxies
        check_wellknown_proxies
    done
}

start_client() {
    local remote_host="$2"

    if [[ -z "$remote_host" ]]; then
        echo "Error: Remote host not specified"
        usage
    fi

    local socket_base="$SOCKET_DIR/cpcd/$INSTANCE"

    echo "Starting CPC remote access client for instance '$INSTANCE'"
    echo "Remote host: $remote_host"
    echo "Socket directory: $socket_base"
    echo "Base port: $BASE_PORT"
    echo ""

    # Create socket directory
    mkdir -p "$socket_base"

    # Control socket
    echo "Starting proxy for ctrl.cpcd.sock from port $BASE_PORT"
    python3 "$PROXY_SCRIPT" client -s "$socket_base/ctrl.cpcd.sock" -H "$remote_host" -p "$BASE_PORT" &

    # Reset socket
    local reset_port=$((BASE_PORT + 1))
    echo "Starting proxy for reset.cpcd.sock from port $reset_port"
    python3 "$PROXY_SCRIPT" client -s "$socket_base/reset.cpcd.sock" -H "$remote_host" -p "$reset_port" &

    echo ""
    echo "Client started with core sockets (ctrl, reset)."
    echo "Endpoint sockets will be created on-demand."
    echo ""
    echo "To add endpoint proxies manually:"
    echo "  # For endpoint N data socket:"
    echo "  python3 $PROXY_SCRIPT client -s $socket_base/epN.cpcd.sock -H $remote_host -p \$((BASE_PORT + 100 + N))"
    echo ""
    echo "  # For endpoint N event socket:"
    echo "  python3 $PROXY_SCRIPT client -s $socket_base/epN.event.cpcd.sock -H $remote_host -p \$((BASE_PORT + 400 + N))"
    echo ""
    echo "Press Ctrl+C to stop."

    wait
}

case "$MODE" in
    server)
        start_server "$@"
        ;;
    client)
        start_client "$@"
        ;;
    *)
        usage
        ;;
esac
