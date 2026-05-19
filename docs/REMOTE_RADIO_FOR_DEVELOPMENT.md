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
restarts it if the USB connection drops (e.g. USB-IP reconnect), updating the
`ble_adapter_id` file accordingly.

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

## USB-IP: Forwarding the Radio to a Remote Dev Server

If your devcontainer runs on a **remote server** (common with VS Code
Remote-SSH) but the BRD2703 is plugged into your **local laptop**, use USB-IP
to forward the USB device over the network.

Automation scripts are provided in `scripts/remote-radio/`. They handle
package installation, kernel module loading, device detection, per-user port
isolation on shared servers, and cleanup.

### Quick Start (Scripted)

**Step 1 — On your laptop** (where the radio is plugged in):

```bash
scripts/remote-radio/usbip-attach-local.sh user@devserver.example.com
```

This auto-detects the radio, binds it, starts the USB-IP daemon, and opens an
SSH reverse tunnel. Leave the terminal open.

**Step 2 — On the remote server** (where Docker runs):

```bash
scripts/remote-radio/usbip-attach-remote.sh
```

This auto-detects the radio from the remote USB listing, attaches it, waits
for `/dev/ttyACM*` to appear, and prints the `RADIO_DEVICE` path to use.

**Step 3 — Validate** (inside the Barton container, after containers are up):

```bash
scripts/remote-radio/usbip-validate.sh
```

Checks the entire chain: D-Bus socket, container processes (cpcd, otbr-agent,
bt_host_cpc_hci_bridge, btattach, bluetoothd), Thread network state, BLE HCI
device, and `DBUS_SYSTEM_BUS_ADDRESS`.

**Teardown** — when done:

```bash
# On the remote server:
scripts/remote-radio/usbip-detach-remote.sh

# On your laptop:
scripts/remote-radio/usbip-detach-local.sh
```

### Port Isolation on Shared Servers

The scripts use per-user loopback addresses to avoid conflicts when multiple
developers share the same remote server. Each user's SSH tunnel binds to a
unique address in the `127.0.0.0/8` range derived from their UID:

```
loopback_addr = 127.0.<UID_hi>.<UID_lo>
```

For example, UID 1000 → `127.0.3.232`. All tunnels use the standard USB-IP
port 3240, so both `usbip list` and `usbip attach` work without any
non-standard flags.

> **Prerequisite**: The remote server's sshd must have `GatewayPorts clientspecified`
> enabled so that SSH can bind the reverse tunnel to the per-user loopback address
> instead of defaulting to `127.0.0.1`. Add this to `/etc/ssh/sshd_config` and
> restart sshd:
> ```
> GatewayPorts clientspecified
> ```

Each user's attach and detach scripts only touch their own state files and
vhci ports. There is no cross-user interference.

### Manual USB-IP Setup

If you prefer to run the commands manually:

<details>
<summary>Click to expand manual instructions</summary>

#### On the laptop (USB-IP server)

```bash
sudo apt install linux-tools-generic linux-cloud-tools-generic
sudo modprobe usbip_core usbip_host vhci_hcd

# Find and bind the radio
# Look for Silicon Labs CP210x (10c4:ea60) or SEGGER J-Link (1366:0105)
usbip list -l
sudo usbip bind -b <bus-id>

# Start the daemon
sudo usbipd -D

# Compute your per-user loopback: 127.0.<UID/256>.<UID%256>
# Example for remote UID 1000: 127.0.3.232
ssh -R 127.0.3.232:3240:localhost:3240 <username>@<remote_ip>
```

#### On the remote server (USB-IP client)

```bash
sudo apt install linux-tools-generic hwdata
sudo modprobe vhci_hcd

# List and attach using your per-user loopback address
usbip list -r 127.0.3.232
sudo usbip attach -r 127.0.3.232 -b <bus-id>

# Confirm
ls -la /dev/ttyACM*
```

</details>

> **Tip**: Always set `RADIO_DEVICE` explicitly, even if the device is at
> `/dev/ttyACM0`. On shared servers, multiple radios may be attached at
> different paths.

---

## Starting the Real-Radio Container

### CLI (`dockerw`)

Use the `-T` flag to add `docker/compose.otbr-radio.yaml` to the compose stack.
Set `RADIO_DEVICE` explicitly:

```bash
RADIO_DEVICE=/dev/ttyACM0 ./dockerw -T bash
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

1. `Using USB radio device: /dev/ttyACM0`
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
If `RADIO_DEVICE` is not set it exits with a clear error in its own logs, but
**the `barton` devcontainer is unaffected and starts normally**.

To enable real radio support, set `RADIO_DEVICE` in `docker/.env` after running
`docker/setupDockerEnv.sh`, or export it in your host shell before opening
VS Code:

```bash
export RADIO_DEVICE=/dev/ttyACM0
```

Then rebuild the devcontainer (**Dev Containers: Rebuild Container**).

---

## Verifying the Full Stack

The fastest way to verify everything is working is to run the validation script
from inside the Barton container:

```bash
scripts/remote-radio/usbip-validate.sh
```

This checks:

- D-Bus socket exists and is reachable
- otbr-radio container processes (cpcd, otbr-agent, bt_host_cpc_hci_bridge,
  btattach, bluetoothd)
- Thread network state via D-Bus
- BLE adapter ID file and HCI device presence
- `DBUS_SYSTEM_BUS_ADDRESS` is set correctly

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

### Detaching the USB-IP device

```bash
# On the remote server:
scripts/remote-radio/usbip-detach-remote.sh

# On your laptop (also close the SSH tunnel terminal):
scripts/remote-radio/usbip-detach-local.sh
```

---

## Troubleshooting

| Symptom | Cause | Fix |
|---------|-------|-----|
| `InitCommissioner` fails with `CHIP_ERROR_INCORRECT_STATE` | Default `CertifierOperationalCredentialsIssuer` requires an auth token | Build with dev platform: `cmake -C config/cmake/platforms/dev/linux.cmake ..` to use `SelfSignedCertifierOperationalCredentialsIssuer` |
| Only `hci0` visible, no `hci1` | `btattach` failed or USB-IP disconnected | Run `scripts/remote-radio/usbip-validate.sh` to diagnose; the entrypoint's BLE monitor should auto-restart `btattach` |
| `ble_adapter_id` contains `0` | Radio BLE bridge didn't create a new HCI device | Check otbr-radio logs for `bt_host_cpc_hci_bridge` errors |
| OpenSSL link errors during build | Stale OpenSSL 1.1.1 in `/usr/local/openssl/` | Remove it: `sudo rm -rf /usr/local/openssl /usr/local/lib/libcurl*` and reconfigure |
| `DBUS_SYSTEM_BUS_ADDRESS` not set | Barton connects to system D-Bus instead of otbr-radio's private bus | Export it or use `dockerw -T` which injects it automatically |
