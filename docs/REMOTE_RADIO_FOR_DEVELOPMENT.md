# Remote Radios for Development

This document explains how a developer working against a **remote dev server**
can use the Zigbee/Thread and Bluetooth radios physically attached to their
**local workstation** — so end-devices next to the developer can be
commissioned even though the devcontainer runs on a distant server.

> **No radios? Nothing to do.** Forwarding radios is an optional, opt-in
> feature. If you do not configure it, the devcontainer runs with simulated
> Thread/Zigbee and no Bluetooth, and no extra setup is required of you.

---

## Overview

Two **independent** physical radios are forwarded from your workstation to the
remote-radios container on the dev server:

| Radio | Purpose | Transport |
|---|---|---|
| Silicon Labs Zigbee/Thread radio (e.g. BRD2703 xG24) | Thread (otbr-agent) and Zigbee (zigbeed/ZigbeeCore) | serial-over-SSH tunnel to a per-user UNIX socket |
| Dedicated Bluetooth USB dongle (e.g. TP-Link UB500) | BLE for Matter commissioning (Matter SDK ↔ BlueZ) | usb-ip, reverse-tunnelled over SSH |

```
  WORKSTATION (your desk)                       DEV SERVER "it"                  remote-radios container (privileged)
  ─────────────────────                         ───────────────                  ───────────────────────────────────
  Silabs radio ──serial──▶ remote-serial.py ──ssh -R──▶ ~/.remote-radios/radios/silabs.sock ──(bind mount)──▶ socat → /dev/ttyRadio → cpcd ─┬─▶ otbr-agent (Thread, D-Bus)
                                                                                                                                       └─▶ zigbeed → ZigbeeCore (Zigbee)
  BT dongle ───usb-ip────▶ usbipd/usbip bind ─ssh -R (port base+UID)─▶ usbip attach (in container, host netns) ─▶ hciX ─▶ bluetoothd (private D-Bus) ─▶ org.bluez ─▶ Matter SDK
```

Key properties:

- **Your workstation's own Bluetooth is never touched.** Only a dedicated USB
  dongle (not the adapter your desktop bluetoothd is using) is forwarded.
- **Private per-developer D-Bus.** The container runs its own dbus-daemon; both
  otbr-agent and BlueZ register on it, never the host/system bus.
- **Per-user isolation.** The Silabs tunnel uses a per-user UNIX socket (0700
  dir); the Bluetooth usb-ip tunnel uses a per-user port (base + your remote
  UID). Developers can't collide with each other.

### Container architecture

| Container | Runs | Provides |
|---|---|---|
| `remote-radios` | cpcd, otbr-agent, socat, usbip attach, btattach, bluetoothd | Thread + Zigbee (CPC) and BLE (real HCI dongle) over a shared private D-Bus |
| `barton` (devcontainer) | Barton application code, Matter SDK | Consumes Thread over D-Bus and BLE via BlueZ |

A named Docker volume shares the private D-Bus socket directory
(`/var/run/remote-radios-dbus`) between the two containers. This is **not** the
host's system D-Bus.

### How BLE adapter selection works

The `remote-radios` entrypoint attaches the dongle over usb-ip, identifies the
newly-created HCI index (e.g. `hci1`, distinct from any host built-in `hci0`),
and writes it to `/var/run/remote-radios-dbus/ble_adapter_id`. Barton reads
this (or the `device.matter.bleAdapterId` property) to configure the Matter
SDK's BLE adapter.

---

## One-time setup (on your workstation)

Run the setup script on your **Ubuntu workstation** where the radios are
plugged in. It detects your radios, saves the choice under
`~/.config/remote-radios/`, installs a per-user systemd service for usb-ip
(one-time `sudo`), and brings up both tunnels:

```bash
# From a BartonCore checkout:
scripts/remote-radios/remote-radios-setup.sh <user>@<devserver>

# Or curl it directly:
curl -fsSL <raw-url>/scripts/remote-radios/remote-radios-setup.sh \
  | bash -s -- <user>@<devserver>
```

The script:

1. **Detects radios.** The Silabs serial radio is matched by USB VID:PID. A
   dedicated Bluetooth dongle is auto-selected **unless** it is the adapter
   your workstation's own bluetoothd is using. If there are multiple candidates
   for either, you are prompted to choose.
2. **Saves your choice** to `~/.config/remote-radios/config` (keyed by the
   radio's stable USB serial / busid) so subsequent runs are non-interactive.
3. **Installs a per-user systemd unit** (`remote-radios-usbip.service`) that
   runs `usbipd` and binds the dongle. A one-time `sudo` installs a narrow
   sudoers rule so later runs need no password.
4. **Establishes the tunnels** and writes `~/.remote-radios/radios.env` on the
   dev server, which `docker/setupDockerEnv.sh` sources automatically.
5. **Monitors and self-heals.** It restarts dropped tunnels and tears
   everything down cleanly when the VPN drops or you log out — then
   re-establishes when connectivity returns. Safe to re-run any time.

### Prerequisites

**Workstation:** Ubuntu, connected to the corporate VPN, with passwordless SSH
(key auth) to the dev server. Required packages:

```bash
sudo apt-get install -y openssh-client usbip python3 socat
pip install --user pyserial
```

(`usbip` is provided by the `usbip` or `linux-tools-generic` package.)

**Dev server:** Docker with your user in the `docker` group. No host `sudo` is
required for usb-ip on the dev server — the dongle is attached **inside** the
privileged container.

### Command-line options

`remote-radios-setup.sh <user>@<devserver>` is normally all you need. It stores
state per-user and is idempotent. Delete `~/.config/remote-radios/config` to
force re-detection.

Under the hood it invokes `scripts/remote-radios/remote-serial.py`, which can
also be run standalone:

```bash
python3 scripts/remote-radios/remote-serial.py <user>@<devserver> \
    [--port /dev/ttyACM0] [--socket ~/.remote-radios/radios/silabs.sock]
```

---

## Starting the devcontainer with radios

Once the workstation setup is running, start the radio overlay on the dev
server. The radio parameters flow in automatically from
`~/.remote-radios/radios.env`.

### CLI (`dockerw`)

```bash
./dockerw -T bash
```

`-T` layers in `docker/compose.remote-radios.yaml` and starts the
`remote-radios` container before opening the Barton shell. Barton is
automatically pointed at the private D-Bus.

### Devcontainer (VS Code)

The overlay is intentionally **not** in the default devcontainer stack (it
would force every developer to build a privileged container and repoint D-Bus).
Use the `dockerw -T` CLI path from within the devcontainer for forwarded
radios.

---

## Local radio (radio on the dev server itself)

If a Silabs radio is physically attached to the dev server, set `SILABS_DEVICE`
instead of using the tunnel:

```bash
SILABS_DEVICE=/dev/ttyACM0 ./dockerw -T bash
```

A dev-server-attached Bluetooth dongle can likewise be bound with `usbip` on the
dev server and pointed at via `BT_USBIP_SOCKET` (a bind-mounted usbipd socket).

---

## Environment variables

These are populated automatically by `remote-radios-setup.sh` →
`~/.remote-radios/radios.env` → `docker/setupDockerEnv.sh`. Override by
exporting before `dockerw`.

| Variable | Meaning |
|---|---|
| `SILABS_SOCKET` | Path of the Silabs tunnel socket **inside** the container (default `/run/remote-radios/radios/silabs.sock`) |
| `SILABS_SOCKET_HOST` | Host path of that socket on the dev server (bind-mount source) |
| `SILABS_DEVICE` | Host path of a locally-attached Silabs USB radio (alternative to the tunnel) |
| `BT_USBIP_SOCKET` | Container path of the bind-mounted usbipd socket for the dongle (empty ⇒ no BLE) |
| `BT_USBIP_BUSID` | Remote busid to attach (default: auto-detect) |
| `BACKBONE_IF` | Thread backbone interface (default: host default route) |

---

## Verifying the full stack

Run the validator from the Barton devcontainer or the `remote-radios`
container; it re-execs into the radio container automatically:

```bash
scripts/remote-radios/validate.sh          # human-readable
scripts/remote-radios/validate.sh --json   # machine-readable
```

It checks the private D-Bus, the Silabs socket/socat/`/dev/ttyRadio`/cpcd
chain, otbr-agent, and (when a dongle is configured) usb-ip reachability, the
imported device, the dongle's HCI adapter, `bluetoothd`, and
`ble_adapter_id`.

### Manual BLE verification

```bash
# From the remote-radios container:
cat /var/run/remote-radios-dbus/ble_adapter_id
nsenter --net=/run/host-netns hciconfig
nsenter --net=/run/host-netns bluetoothctl list
```

---

## Teardown

- **Workstation:** press `Ctrl-C` in the `remote-radios-setup.sh` terminal (it
  removes the remote socket and env hint and stops the tunnels). The usb-ip
  systemd service unbinds the dongle when stopped:
  `systemctl --user stop remote-radios-usbip.service`.
- **Dev server:** stop the container with
  `docker compose -f docker/compose.yaml -f docker/compose.remote-radios.yaml down`.

Teardown also happens automatically when the VPN drops or you log out.

---

## Troubleshooting

- **"Silabs tunnel socket did not appear"** — ensure `remote-radios-setup.sh`
  (remote-serial.py) is running on your workstation and that your VPN/SSH is up.
- **"usb-ip service not reachable"** — the reverse tunnel or `usbipd` on the
  workstation is down; re-run the setup script.
- **"no new HCI device appeared after usb-ip attach"** — confirm the dongle is
  bound on the workstation (`usbip list -l`) and that the `vhci-hcd`/`usbip`
  kernel modules are available on the dev server.
- **BLE picks the wrong adapter** — check `ble_adapter_id`; set
  `device.matter.bleAdapterId` explicitly if needed.
