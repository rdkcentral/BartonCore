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

### Linux workstations

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
   runs `usbipd`, binds the dongle, and re-binds it if anything hands it back
   to the kernel's own driver. A one-time `sudo` installs two root-owned
   helpers under `/usr/local/lib/remote-radios/` and a narrow sudoers rule
   granting the `remote-radios` group passwordless access to **those two
   commands only**, so later runs need no password. See
   [Privileges](#privileges) below.
4. **Establishes the tunnels** and writes `~/.remote-radios/radios.env` on the
   dev server, which `docker/setupDockerEnv.sh` sources automatically.
5. **Monitors and self-heals.** It restarts dropped tunnels and tears
   everything down cleanly when the VPN drops or you log out — then
   re-establishes when connectivity returns. Safe to re-run any time.

### Windows workstations

`remote-radios-setup.sh` is a Bash script and needs systemd and the in-tree
usb-ip tools, so it does not run on Windows. Use `remote-serial.py` directly
instead — it is the same forwarder core the setup script drives, and it is
cross-platform:

```powershell
# Install the prerequisites once.
pip install pyserial
winget install usbipd

# Share the Bluetooth dongle (admin shell, once per dongle).
usbipd list                  # note the BUSID
usbipd bind --busid 2-4

# Forward both radios.
python remote-serial.py <user>@<devserver> --usbip
```

Differences from the Linux path:

- **Binding is explicit.** On Linux the setup script installs a privileged
  helper that binds the dongle and re-binds it if the kernel takes it back. On
  Windows `usbipd bind` is a one-time administrator action that persists, so
  `remote-serial.py` performs it when needed and no helper is installed.
- **No service is installed.** Keep the terminal open while you work; the
  script tears the tunnels down when you stop it.
- **Serial ports are named `COMn`.** Auto-detection handles this; override with
  `--port COM3` if several radios are attached.

`--usbip` on its own auto-selects the dongle; pass `--usbip <BUSID>` to choose
explicitly, or `--no-serial` to forward only Bluetooth.

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

The dev server must **not** run its own Bluetooth daemon. Linux does not place
the Bluetooth stack in a network namespace, so when usb-ip attaches your dongle
it becomes visible to the dev server's host `bluetoothd` as well as to the one
inside the container. BlueZ cannot share an adapter between two daemons: the
host daemon claims it first, and every BLE operation in the container then
fails with `No default controller available` — while `hciconfig` still cheerfully
reports the adapter `UP RUNNING`, which makes this failure easy to misread.

If the dev server has no Bluetooth hardware of its own (the usual case for a
build server), disable the daemon there once:

```bash
sudo systemctl mask --now bluetooth.service
```

Masking rather than disabling matters: `bluetooth.service` is a D-Bus-activated
unit, so anything that touches `org.bluez` on the system bus can otherwise
start it again. This does not affect the container, which runs its own
`bluetoothd` on a private D-Bus.

### Privileges

`usbipd` and `usbip bind` require root on the workstation. Rather than asking
for a password on every run, the setup script installs, on first use:

| What | Where | Ownership |
|---|---|---|
| `usbipd-bind.sh` | `/usr/local/lib/remote-radios/` | `root:root`, mode `0755` |
| `usbipd-release.sh` | `/usr/local/lib/remote-radios/` | `root:root`, mode `0755` |
| sudoers rule | `/etc/sudoers.d/remote-radios` | `root:root`, mode `0440` |

```text
%remote-radios ALL=(root) NOPASSWD: /usr/local/lib/remote-radios/usbipd-bind.sh, \
                                    /usr/local/lib/remote-radios/usbipd-release.sh
```

The grant is deliberately given to a group rather than a named user, so one
install serves every developer on a shared workstation. The helpers live
outside `$HOME` because a root-executed script in a directory the invoking user
can write to is equivalent to handing that user passwordless root — they can
simply rewrite the script. Being root-owned and not group-writable is what
keeps the grant as narrow as it looks. The helpers also validate their busid
argument, since a sudoers entry listing a command with no argument list permits
any arguments.

Group membership is only applied to new login sessions. On first install the
script adds you to `remote-radios` and then stops, asking you to log out and
back in (or run `newgrp remote-radios`) before re-running it.

To grant another developer access on the same workstation:

```bash
sudo usermod -aG remote-radios <user>
```

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
  systemd service returns the dongle to your workstation when stopped:
  `systemctl --user stop remote-radios-usbip.service`. After that the dongle is
  back on the normal kernel driver and your workstation's own Bluetooth can use
  it again.
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
- **The Silabs radio is connected but silent** — cpcd sends, nothing comes
  back. This is usually the wrong RTS/CTS mode: some USB-serial adapters do not
  drive the hardware flow-control lines, so requesting flow control leaves RTS
  deasserted and the radio never transmits. `remote-serial.py` detects the
  signature (bytes flowing one way only) and flips the mode once automatically,
  logging `flipping RTS/CTS flow control`. To skip the six-second detection
  window, pass `--rtscts` or `--no-rtscts` explicitly.- **`No default controller available`, or the validator reports "Controller
  hciN not available to bluetoothd"** — another `bluetoothd` has claimed the
  dongle. Almost always this is the dev server's own `bluetooth.service`; run
  `sudo systemctl mask --now bluetooth.service` there and restart the
  remote-radios container. Note that `hciconfig` will report the adapter
  `UP RUNNING` throughout, because the kernel side is genuinely fine — only
  bluetoothd ownership is missing.
- **BLE scan finds zero devices** — treated as a failure, not a warning: an
  adapter that is present but deaf is indistinguishable from a working one by
  any other check. Confirm the dongle's antenna/placement and that no second
  `bluetoothd` is competing for it.
- **`sudo: a password is required` when the usb-ip service starts** — you are
  not yet in the `remote-radios` group in this session. Log out and back in (or
  `newgrp remote-radios`) and re-run the setup script.
- **Stopping the service does not release the dongle** — verify the unit has an
  `ExecStopPost=` line invoking `usbipd-release.sh`. A user systemd manager
  cannot signal the root-owned bind helper directly, so the release helper is
  what actually reclaims the device.
