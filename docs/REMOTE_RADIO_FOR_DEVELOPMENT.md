# Thread and BLE Support for Remote Development

This document covers how Barton integrates with a **Thread Border Router** and
**Bluetooth Low Energy (BLE)** for Matter device commissioning. Both capabilities
are provided by the same physical radio — a Silicon Labs BRD2703 xG24 — and
managed by the `otbr-radio` Docker container.

---

## Overview

The BRD2703 xG24 is a multi-protocol radio that provides two wireless interfaces
through a single USB connection:

- **Thread 802.15.4** — used by `otbr-agent` for the Thread mesh network
- **Bluetooth LE** — used by the Matter SDK for BLE-based device commissioning

Both interfaces are multiplexed over the **CPC (Co-Processor Communication)**
protocol. The `cpcd` daemon manages the USB serial link and exposes separate
CPC endpoints for each protocol.

```
  BRD2703 USB Radio (/dev/ttyACM0)
    │
    ▼
  cpcd                         (CPC daemon — multiplexes radio protocols)
    ├── spinel+cpc://cpcd_0 ──▶ otbr-agent     (Thread 802.15.4)
    │                            │  D-Bus: io.openthread.BorderRouter.wpan0
    │                            ▼
    │                          Barton           (Thread network management)
    │
    └── BLE CPC endpoint ────▶ bt_host_cpc_hci_bridge
                                 │  creates virtual serial device (pts_hci)
                                 ▼
                               btattach         (creates HCI device, e.g. hci1)
                                 │  host network namespace
                                 ▼
                               bluetoothd       (BlueZ daemon, private D-Bus)
                                 │  D-Bus: org.bluez
                                 ▼
                               Barton / Matter SDK  (BLE scanning & commissioning)
```

### Container Architecture

| Container | Runs | Provides |
|-----------|------|----------|
| `otbr-radio` | cpcd, otbr-agent, bt_host_cpc_hci_bridge, btattach, bluetoothd | Thread + BLE via shared D-Bus and host network namespace |
| `barton` (devcontainer) | Barton application code, Matter SDK | Consumes Thread D-Bus API and BLE via BlueZ |

A named Docker volume (`dbus-socket`) at `/var/run/otbr-dbus` shares a private
D-Bus bus between the two containers. This is **not** the host's system D-Bus.

The BLE stack requires the **host network namespace** because Linux
`AF_BLUETOOTH` sockets only work in the initial (host) network namespace.
`btattach` and `bluetoothd` run via `nsenter --net=/run/host-netns` inside the
`otbr-radio` container.

### How BLE Adapter Selection Works

The `otbr-radio` entrypoint detects the HCI device index created by `btattach`
(e.g. `hci1`) and writes it to `/var/run/otbr-dbus/ble_adapter_id`. Barton
reads this file at startup to configure the Matter SDK's BLE adapter via
`BLEMgrImpl().ConfigureBle(adapterId, true)`.

The host machine may have its own built-in Bluetooth adapter (e.g. `hci0`).
The radio's BLE adapter appears as a separate HCI device with a distinct index.
A background monitor in the entrypoint watches `btattach` and automatically
restarts it if the connection drops, updating the `ble_adapter_id` file
accordingly.

---

## Thread Modes

| Mode | When to use | How to start |
|------|-------------|-------------|
| **Simulated** (default) | Unit/integration tests, no hardware needed | `scripts/start-simulated-otbr.sh` |
| **Real USB radio** | Testing with real Thread + BLE devices | See below |

> **Note**: Enable Thread and Border Router support in the Matter SDK
> (`-DOT_THREAD_VERSION=1.4 -DOTBR_BORDER_ROUTING=ON`) if Matter is also enabled.

---

## Hardware: BRD2703 xG24 Explorer Kit

### Flashing RCP Firmware

Before use, the BRD2703 must be flashed with a CPC-capable **RCP (Radio
Co-Processor)** firmware image that includes the BLE HCI endpoint.

If the board is new (out of the box), its debug adapter firmware must be updated
first. Connect the board to **Simplicity Studio** and accept any firmware
upgrade prompts before proceeding.

Download the firmware image appropriate for the BRD2703 (EFR32MG24) in either
`.s37` or `.hex` format, then flash it using
[Simplicity Commander](https://www.silabs.com/developers/simplicity-studio).

---

## Radio Connection Methods

Two methods are supported for connecting the radio to the otbr-radio container:

| Method | When to use | Environment variable |
|--------|-------------|---------------------|
| **Local USB** | Radio is plugged directly into the Docker host | `RADIO_DEVICE=/dev/ttyACM0` |
| **Remote serial tunnel** | Radio is on your laptop, Docker runs on a remote server | `RADIO_PORT=21234` |

### Local USB Radio

If the radio is plugged directly into the machine running Docker:

```bash
RADIO_DEVICE=/dev/ttyACM0 ./dockerw -T bash
```

### Remote Serial Tunnel

If your devcontainer runs on a **remote server** (common with VS Code
Remote-SSH) but the BRD2703 is plugged into your **local workstation**, use
`remote-serial.py` to forward the serial port over an SSH tunnel.

```
  Workstation                          Dev Server (Docker host)
  ┌──────────────────┐                ┌───────────────────────────────┐
  │ BRD2703 radio    │                │  otbr-radio container         │
  │ /dev/ttyACM0     │                │  ┌─────────────────────────┐  │
  │       │          │                │  │ socat (TCP→PTY bridge)  │  │
  │       ▼          │                │  │   /dev/ttyRadio         │  │
  │ remote-serial.py │   SSH tunnel   │  │       │                 │  │
  │ (serial→TCP      │───────────────▶│  │       ▼                 │  │
  │  relay + SSH -R) │  port 21234    │  │     cpcd                │  │
  │                  │                │  │   (same as local USB)   │  │
  └──────────────────┘                │  └─────────────────────────┘  │
                                      └───────────────────────────────┘
```

The tunnel is self-healing — if the SSH connection drops, `remote-serial.py`
automatically reconnects. The socat bridge inside the container also reconnects
automatically.

#### Prerequisites

1. **Python 3.7+** and **pyserial** on the workstation:

   ```bash
   pip install pyserial
   ```

2. **SSH key authentication** from your workstation to the dev server (no
   password prompts).

3. **`GatewayPorts clientspecified`** in the dev server's sshd config:

   ```
   # /etc/ssh/sshd_config on the dev server
   GatewayPorts clientspecified
   ```

   Then restart sshd: `sudo systemctl restart sshd`

#### Quick Start

**Step 1 — On your workstation** (where the radio is plugged in):

```bash
python3 scripts/remote-radio/remote-serial.py <user>@<devserver>
```

The script auto-detects the radio, starts a TCP relay, and opens an SSH reverse
tunnel. Leave this terminal open.

Example output:

```
[remote-serial] OK: Auto-detected radio: /dev/ttyACM0 (Silicon Labs CP210x)
[remote-serial] OK: Remote UID: 1234
[remote-serial] OK: Tunnel port: 21234
[remote-serial] OK: TCP server listening on 127.0.0.1:21234

=======================================
 REMOTE SERIAL TUNNEL ACTIVE
=======================================
 Radio:       /dev/ttyACM0
 Local TCP:   127.0.0.1:21234
 Tunnel port: 21234 on remote (all interfaces)
=======================================

    RADIO_PORT=21234
```

**Step 2 — On the dev server** (or in your devcontainer):

```bash
RADIO_PORT=21234 ./dockerw -T bash
```

Or add `RADIO_PORT=21234` to `docker/.env`.

**Step 3 — Validate** (inside the Barton or otbr-radio container):

```bash
scripts/remote-radio/validate.sh
```

#### Port Isolation on Shared Servers

The tunnel port is computed as `20000 + UID` on the remote server. Each
developer on a shared server gets a unique port automatically:

| UID | Port |
|-----|------|
| 1234 | 21234 |
| 1235 | 21235 |
| 1236 | 21236 |

#### Command-Line Options

```
usage: remote-serial.py [-h] [--port PORT] [--baud BAUD] ssh_target

positional arguments:
  ssh_target            SSH target (e.g. user@hostname)

options:
  --port PORT           Serial port path (auto-detected if omitted)
  --baud BAUD           Baud rate (default: 115200)
```

---

## Starting the Real-Radio Container

### CLI (`dockerw`)

Use the `-T` flag to add `docker/compose.otbr-radio.yaml` to the compose stack:

```bash
# Local USB radio:
RADIO_DEVICE=/dev/ttyACM0 ./dockerw -T bash

# Remote serial tunnel:
RADIO_PORT=21234 ./dockerw -T bash
```

To override the backbone interface:

```bash
RADIO_DEVICE=/dev/ttyACM1 BACKBONE_IF=eth1 ./dockerw -T bash
```

This creates two containers: `barton` and `otbr-radio`.

To check the logs:

```bash
docker compose -f docker/compose.yaml -f docker/compose.otbr-radio.yaml logs -f otbr-radio
```

Expected log progression:

1. `Using USB radio device: /dev/ttyACM0` (or `Remote serial mode: connecting to ...`)
2. `Connected to Secondary` / `Secondary CPC v4.4.6`
3. `Daemon startup was successful`
4. `bt_host_cpc_hci_bridge ready`
5. `Radio HCI device: hci1 (index 1)`
6. `bluetoothd started`
7. `Starting otbr-agent`

### Devcontainer (VS Code)

`compose.otbr-radio.yaml` is included in the devcontainer by default — no
manual edit to `.devcontainer/devcontainer.json` is required.

The `otbr-radio` container starts automatically when the devcontainer launches.
If neither `RADIO_DEVICE` nor `RADIO_PORT` is set, it exits with a clear error
in its own logs, but **the `barton` devcontainer is unaffected and starts
normally**.

To enable real radio support, set the appropriate variable in `docker/.env`
after running `docker/setupDockerEnv.sh`, or export it in your host shell
before opening VS Code:

```bash
# Local radio:
export RADIO_DEVICE=/dev/ttyACM0

# Remote tunnel:
export RADIO_PORT=21234
```

Then rebuild the devcontainer (**Dev Containers: Rebuild Container**).

---

## Verifying the Full Stack

Run the validation script from inside the Barton container:

```bash
scripts/remote-radio/validate.sh
```

This checks:

- Container environment (privileged mode, required binaries)
- D-Bus socket and daemon
- Radio connection (local USB or remote serial tunnel)
- CPC daemon and sockets
- BLE chain (bridge → btattach → bluetoothd)
- HCI adapter and transport health
- BLE scanning capability
- otbr-agent and Thread interface
- Known pitfalls

Options: `--json` for machine-readable output, `--fix` for auto-remediation.

### Manual D-Bus Verification

From inside either container, query the OTBR D-Bus interface:

```bash
gdbus introspect \
    --address unix:path=/var/run/otbr-dbus/system_bus_socket \
    --dest io.openthread.BorderRouter.wpan0 \
    --object-path /io/openthread/BorderRouter/wpan0
```

Read the current Thread device role:

```bash
gdbus call \
    --address unix:path=/var/run/otbr-dbus/system_bus_socket \
    --dest io.openthread.BorderRouter.wpan0 \
    --object-path /io/openthread/BorderRouter/wpan0 \
    --method org.freedesktop.DBus.Properties.Get \
    io.openthread.BorderRouter DeviceRole
```

### D-Bus Configuration

The `dbus-socket` volume shares `/var/run/otbr-dbus` between containers. To
reach the private D-Bus daemon, `DBUS_SYSTEM_BUS_ADDRESS` must point to it:

- **`dockerw -T`**: Injected automatically.
- **Devcontainer**: Set in `docker/.env` or export before rebuilding:

  ```bash
  export DBUS_SYSTEM_BUS_ADDRESS=unix:path=/var/run/otbr-dbus/system_bus_socket
  ```

### BLE Verification

Check the BLE adapter from inside the Barton container:

```bash
# Verify the adapter ID file
cat /var/run/otbr-dbus/ble_adapter_id

# Check HCI devices in the host network namespace
sudo nsenter --net=/run/host-netns hciconfig

# List Bluetooth controllers
sudo nsenter --net=/run/host-netns bluetoothctl list
```

Expected: two HCI devices — the host's built-in adapter (e.g. `hci0`) and the
radio's BLE adapter (e.g. `hci1`). The `ble_adapter_id` file should contain
the index of the radio's adapter.

---

## Teardown

### Stopping the otbr-radio container

```bash
docker compose -f docker/compose.yaml -f docker/compose.otbr-radio.yaml rm -sf otbr-radio
```

### Stopping the remote serial tunnel

Press `Ctrl-C` in the terminal running `remote-serial.py` on your workstation.

---

## Troubleshooting

| Symptom | Cause | Fix |
|---------|-------|-----|
| `InitCommissioner` fails with `CHIP_ERROR_INCORRECT_STATE` | Default `CertifierOperationalCredentialsIssuer` requires an auth token | Build with dev platform: `cmake -C config/cmake/platforms/dev/linux.cmake ..` to use `SelfSignedCertifierOperationalCredentialsIssuer` |
| Only `hci0` visible, no `hci1` | `btattach` failed or radio disconnected | Run `scripts/remote-radio/validate.sh` to diagnose; the entrypoint's BLE monitor should auto-restart `btattach` |
| `ble_adapter_id` contains `0` | Radio BLE bridge didn't create a new HCI device | Check otbr-radio logs for `bt_host_cpc_hci_bridge` errors |
| OpenSSL link errors during build | Stale OpenSSL 1.1.1 in `/usr/local/openssl/` | Remove it: `sudo rm -rf /usr/local/openssl /usr/local/lib/libcurl*` and reconfigure |
| `DBUS_SYSTEM_BUS_ADDRESS` not set | Barton connects to system D-Bus instead of otbr-radio's private bus | Export it or use `dockerw -T` which injects it automatically |
| Remote tunnel not reachable | SSH tunnel not binding or IPv6/IPv4 mismatch | Check `remote-serial.py` output; ensure `GatewayPorts clientspecified` in sshd_config |
| socat/ttyRadio not created | Remote serial tunnel not yet running | Start `remote-serial.py` on workstation before starting containers |
