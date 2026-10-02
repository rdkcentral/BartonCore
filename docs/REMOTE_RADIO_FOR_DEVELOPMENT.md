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
| `remote-radios` | cpcd, otbr-agent, socat, usbip attach, bluetoothd | Thread + Zigbee (CPC) and BLE (real HCI dongle) over a shared private D-Bus |
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

### Sharing the radios with other projects

One forwarder, and one `remote-radios` container, serve every project you work
on. The container is the hub: it owns the physical radios and publishes them, so
other stacks do not each attach the hardware themselves.

| Consumer | Gets Thread/Zigbee via | Gets Bluetooth via |
|---|---|---|
| `barton` devcontainer | otbr-agent over the shared private D-Bus | BlueZ over the shared private D-Bus |
| ZigbeeCore (`./run.sh -r`) | the Silabs socket directly, under a claim | the shared private D-Bus |
| zilker | BartonCore's device service | BartonCore's device service |
| HH4 (QEMU guest) | the Silabs socket directly, under a claim | attaches the dongle itself over usb-ip |

Everything that runs on the dev server shares its kernel, so it can consume the
hub concurrently over sockets and D-Bus. A **VM guest cannot**: it has its own
kernel and its own BlueZ, so it needs the devices themselves.

Both radios are therefore exclusive, and consumers take turns rather than
compete:

- **Silabs radio** — the workstation relay serves one client at a time. A
  consumer that wants it creates `~/.remote-radios/claims/silabs.claim`. The
  container watches that file, stops cpcd and its socat bridge while it exists,
  and reclaims the radio once it is removed. ZigbeeCore's `run.sh -r` and HH4's
  `hh4-run.sh` both take this claim and release it on exit, including on
  interrupt, so handover needs no manual steps.
- **Bluetooth dongle** — a usb-ip device can be attached by one host only, and
  the kernel grants exclusive access per `hciN` (a second stack binding it gets
  `-EBUSY` or `-EUSERS`). When an HH4 guest attaches the dongle the container
  loses it; the container's monitor re-attaches it when the guest shuts down.

There is no supported way to share one Bluetooth controller between two stacks.
If you need the container and a VM guest to have Bluetooth **at the same time**,
use a second dongle — the forwarder exports both, and each consumer attaches its
own.

Claims are advisory and per-user. The claims directory lives inside your own
`~/.remote-radios`, so it never affects another developer on the same server.

---

## One-time setup (on your workstation)

### Linux workstations

Run the setup on your **Ubuntu workstation** where the radios are plugged in.
You do not need a BartonCore checkout there — the workstation is not where you
do development, so the setup installs what it needs into
`~/.config/remote-radios/bin/` and runs from there:

```bash
curl -fsSL https://raw.githubusercontent.com/rdkcentral/BartonCore/main/scripts/remote-radios/remote-radios-setup.sh \
  | bash -s -- <user>@<devserver>
```

From a checkout it is the same command without the download:

```bash
scripts/remote-radios/remote-radios-setup.sh <user>@<devserver>
```

Either way it detects your radios, saves the choice under
`~/.config/remote-radios/`, and installs a per-user systemd service that keeps
both radios forwarded (one-time `sudo`).

To install from a branch other than `main`, point it at that ref — the variable
has to be set on `bash`, not on `curl`:

```bash
curl -fsSL https://raw.githubusercontent.com/rdkcentral/BartonCore/<ref>/scripts/remote-radios/remote-radios-setup.sh \
  | REMOTE_RADIOS_REF=<ref> bash -s -- <user>@<devserver>
```

The script exits once the service is running — you do not leave it open in a
terminal. Check on it with:

```bash
systemctl --user status remote-radios.service
journalctl --user -u remote-radios.service -f
```

### Updating an existing workstation

Re-run setup. It upgrades itself in place:

```bash
~/.config/remote-radios/bin/remote-radios-setup.sh
```

No arguments are needed — the dev server, the chosen radios and the dongle are
read back from `~/.config/remote-radios/config`, so this is non-interactive.

Each run refreshes `remote-radios-setup.sh` and `remote-serial.py` from the
repository, and if the setup script itself changed it re-executes the new copy
so the upgrade is carried out by the newer version rather than the one you
started. The files are replaced by rename rather than overwritten in place,
because a shell reads a script lazily as it runs and rewriting the file
underneath a running one corrupts it.

Only what actually changed is acted on: the service is restarted when the
runtime, the root helpers or the unit changed, and left alone otherwise. If the
workstation is offline, or the ref no longer exists, the run warns and keeps the
installed copy instead of failing.

The installed layout is versioned in `~/.config/remote-radios/version`, so a
workstation set up by an older release is migrated rather than just overwritten
— stopping and removing the superseded `remote-radios-usbip.service`, and
deleting the old sudo helper that used to live under `$HOME`.

The `curl | bash` one-liner above also upgrades an existing install, so either
entry point works.

The script:

1. **Installs its runtime** to `~/.config/remote-radios/bin/`
   (`remote-radios-setup.sh` and `remote-serial.py`), copying from a checkout
   when run from one and downloading them otherwise. The service always runs
   the installed copy, so the workstation never needs a checkout and nothing
   breaks if one is moved or deleted later. These run as you, never as root —
   unlike the sudo helpers below, which is why they can live under `$HOME`.
2. **Detects radios.** The Silabs serial radio is matched by USB VID:PID. A
   dedicated Bluetooth dongle is auto-selected **unless** it is the adapter
   your workstation's own bluetoothd is using. If there are multiple candidates
   for either, you are prompted to choose.
3. **Saves your choice** to `~/.config/remote-radios/config` (keyed by the
   radio's stable USB serial / busid, plus the dev server) so the service can
   run without arguments and later runs are non-interactive.
4. **Verifies it can reach the dev server** over SSH before installing anything
   that claims to maintain a connection to it.
5. **Installs the privileged helpers.** A one-time `sudo` installs two
   root-owned helpers under `/usr/local/lib/remote-radios/` and a narrow
   sudoers rule granting the `remote-radios` group passwordless access to
   **those two commands only**. See [Privileges](#privileges) below. This step
   is skipped entirely if you have no Bluetooth dongle.
6. **Installs and starts `remote-radios.service`.** One user unit owns the
   whole workstation side: it runs `usbipd` and keeps the dongle exported
   (re-binding it if anything hands it back to the kernel's own driver), runs
   both SSH tunnels, and writes `~/.remote-radios/radios.env` on the dev server
   for `docker/setupDockerEnv.sh` to source.

The service is wanted by `default.target`, so it **starts at login and stops at
logout**. Nothing is left running on the workstation when you are not logged
in, and there is no separate step after a reboot — log in and the radios are
forwarded.

While it runs it self-heals: it restarts dropped tunnels, tears everything down
cleanly when the dev server becomes unreachable and re-establishes when it
returns, and picks up radios that are **plugged in after login** rather than
requiring them to be present at start.

> The service runs the copy in `~/.config/remote-radios/bin/`, so moving or
> deleting a checkout does not break it. Re-run the setup to pick up an updated
> version.

Re-run the script any time to change the dev server, re-detect radios after a
hardware change, or pick up an updated checkout; it restarts the service only
when something actually changed.

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
| `RADIO_CLAIM_DIR` | Container path of the claims directory (default `/run/remote-radios/claims`) |
| `RADIO_CLAIM_DIR_HOST` | Host path of that directory (bind-mount source, read-only in the container) |
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

- **Workstation:** `systemctl --user stop remote-radios.service`. That removes
  the remote sockets and env hint, stops both tunnels, and returns the dongle
  to the normal kernel driver so your workstation's own Bluetooth can use it
  again. To stop it coming back at the next login, add
  `systemctl --user disable remote-radios.service`.
- **Dev server:** stop the container with
  `docker compose -f docker/compose.yaml -f docker/compose.remote-radios.yaml down`.

Teardown also happens automatically when the dev server becomes unreachable,
and the service stops at logout.

---

## Troubleshooting

- **Nothing is forwarded after logging in** — check the service first:
  `systemctl --user status remote-radios.service`, then
  `journalctl --user -u remote-radios.service -b`. If the unit does not exist,
  the setup script has not been run on this workstation.
- **The service exits with "No radios configured"** — the saved config is
  missing or empty. Re-run `remote-radios-setup.sh <user>@<devserver>`.
- **The service logs `sudo: a password is required`** — your login session does
  not have the `remote-radios` group yet. Log out and back in; the service
  picks it up on the next login.
- **A radio plugged in after login is not picked up** — it should be within
  ~10 seconds; the service polls for it. Check the journal for
  `Silabs radio appeared` or `Bluetooth dongle ... appeared`. If the radio is a
  different unit than the one saved, re-run the setup script to re-detect.
- **"Silabs tunnel socket did not appear"** — ensure `remote-radios.service` is
  active on your workstation and that your VPN/SSH is up.
- **"usb-ip service not reachable"** — the reverse tunnel or `usbipd` on the
  workstation is down; check the service journal, or re-run the setup script.
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
  window, pass `--rtscts` or `--no-rtscts` explicitly.
- **cpcd keeps stopping, or the log says "Silabs radio claimed via ..."** — this
  is the claim protocol working. Another project (an `hh4-run.sh` guest, or
  ZigbeeCore's `./run.sh -r`) is using the radio, so the container released it
  and is waiting. It reclaims the radio automatically once that run exits. If a
  stale claim is left behind after a crash, delete
  `~/.remote-radios/claims/silabs.claim`.
- **`No default controller available`, or the validator reports "Controller
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
- **BLE stops working while an HH4 guest is running** — expected. A usb-ip
  dongle can be attached by one host at a time, so the guest took it. The
  container's monitor re-attaches it when the guest shuts down. For simultaneous
  use, add a second dongle.
- **`sudo: a password is required` when the usb-ip service starts** — you are
  not yet in the `remote-radios` group in this session. Log out and back in (or
  `newgrp remote-radios`) and re-run the setup script.
- **Stopping the service does not release the dongle** — verify the unit has an
  `ExecStopPost=` line invoking `usbipd-release.sh`. A user systemd manager
  cannot signal the root-owned bind helper directly, so the release helper is
  what actually reclaims the device.
