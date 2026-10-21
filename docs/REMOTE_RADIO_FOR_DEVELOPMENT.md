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
  WORKSTATION (your desk)                       DEV SERVER                       remote-radios container (privileged)
  ─────────────────────                         ───────────────                  ───────────────────────────────────
  Silabs radio ──serial──▶ remote-serial.py ──ssh -R──▶ ~/.remote-radios/radios/silabs.sock ──(bind mount)──▶ socat → /dev/ttyRadio → cpcd ─┬─▶ otbr-agent (Thread)
                                                                                                                                       └─▶ remote-zigbee container: zigbeed → ZigbeeCore (Zigbee)
  BT dongle ───usb-ip────▶ usbipd/usbip bind ─ssh -R─▶ ~/.remote-radios/usbip/usbipd.sock ──▶ usbip attach (host netns) ─▶ hciX ─▶ dev server's bluetoothd ─▶ org.bluez
```

Key properties:

- **Your workstation's own Bluetooth is never touched.** Only a dedicated USB
  dongle (not the adapter your desktop bluetoothd is using) is forwarded.
- **One D-Bus: the dev server's own.** Your border router (`otbr-agent`) and
  BlueZ (`org.bluez`) both live on the dev server's system bus, because Matter
  requires both on the same bus. Your border router registers there under a
  name unique to you, `io.openthread.BorderRouter.wpan<uid>`.
- **Per-user isolation.** Your radio sockets are in your own `~/.remote-radios`
  (mode `0700`), your Thread interface is named after your uid, and your radios
  container sits on a Docker network that only your containers join. Developers
  cannot collide with each other.

### Container architecture

| Container | Runs | Provides |
|---|---|---|
| `remote-radios` (one per user) | cpcd, otbr-agent, mdnsd, socat, usbip attach | Thread + CPC, and your dongle attached as an HCI adapter |
| `remote-zigbee` (one per user, from ZigbeeCore) | zigbeed, ZigbeeCore, socat | Zigbee, using the radios container's cpcd |
| `barton` (devcontainer, one per workspace) | Barton application code, Matter SDK | Consumes Thread and BLE over the dev server's D-Bus, and Zigbee from `remote-zigbee` |

The radios stack belongs to **you**, not to a workspace: the radio sockets in
`~/.remote-radios` are per user, so there is exactly one stack to share. It runs
in the Compose project `<user>-remote-radios` on the network `<user>-remote-radios`.
Every one of your BartonCore workspaces joins that network in addition to its
own, and `./dockerw -T` in a second workspace reuses the running stack rather
than starting another. `dockerw` never recreates a running radios stack, because
another workspace may be using it; to pick up changes to its compose files or
entrypoint, recreate it explicitly:

```bash
cd docker
docker compose -p "$USER-remote-radios" -f compose.yaml -f compose.remote-radios.yaml \
    up -d --force-recreate remote-radios
```

Everything this creates on the dev server is named after you:

| Kind | Name | Scope |
|---|---|---|
| Container, image and Compose project | `<user>-remote-radios` | One per user |
| Zigbee container and Compose project | `<user>-remote-zigbee` | One per user, started from ZigbeeCore |
| Network | `<user>-remote-radios` | One per user, joined by each of your workspaces |
| Volume | `<user>-remote-radios-state` | One per user |
| Workspace network | `<user>-<workspace>` | One per BartonCore workspace |

The volume is mounted in parts. Its `ble/` directory appears at
`/var/run/remote-radios-dbus` and carries `ble_adapter_id` from the radios
container to each of your barton containers. Its `cpcd/` directory appears at
`/dev/shm/cpcd` and holds cpcd's sockets. These mounts need Docker Engine 26 and
Compose 2.23 or later.

### How Thread and Matter find your border router

otbr-agent derives its D-Bus name from its Thread interface, and
`docker/setupDockerEnv.sh` names that interface `wpan<uid>` (for example
`wpan1000`), unique on the server. The remote-radios overlay passes the same
name to Barton as `BARTON_THREAD_INTERFACE`, which Barton's Thread client and
the Matter SDK both use to select the border router. Without the overlay it
defaults to `wpan0`, so nothing changes for anyone not using remote radios.

The Matter SDK hardcodes `wpan0` upstream. BartonCore carries a patch,
`third_party/matter/barton-library/patches/0004-*`, that adds
`ThreadStackManagerImpl::SetThreadInterfaceName()`.

otbr-agent publishes its services with mDNSResponder (`mdnsd`) rather than
avahi. Avahi is reached over D-Bus, and only one `org.freedesktop.Avahi` can
exist on the shared bus; mDNSResponder uses a local socket instead.

### How Matter discovery stays private

Thread devices do not use mDNS on the mesh. They register their services with
your border router over SRP, and otbr republishes them as mDNS on its backbone,
the `<user>-remote-radios` Docker network. Matter's queries go out on that
network and your `mdnsd` answers. mDNS is link-local multicast, which never
leaves the network it was sent on, and Docker drops traffic between networks,
so you never discover another developer's devices. Once a device is found,
Matter talks to it by unicast IPv6: `./dockerw -T` installs a route for
`fd00::/8` via your border router, which forwards onto your Thread mesh.

This is why `./dockerw -T` refuses `-H` (host networking). On the host network
your container would be on every developer's Docker networks at once.

### How BLE adapter selection works

The dev server runs a single `bluetoothd` for everyone. The kernel only allows
Bluetooth sockets in the host's network namespace, and `bluetoothd` cannot be
limited to one adapter, so a second daemon would fight the first over every
dongle. One daemon managing many adapters is what BlueZ is designed for.

The `remote-radios` entrypoint attaches your dongle over usb-ip, identifies the
newly-created HCI index (e.g. `hci1`), and writes it to
`/var/run/remote-radios-dbus/ble_adapter_id`. `./dockerw -T` exports it as
`BARTON_BLE_ADAPTER_ID` in the barton container, and the reference app passes it
to Barton as the `barton.ble.adapterId` property, which chooses your adapter from
the ones the shared daemon manages. Barton itself does not read the file.

### Sharing the radios with other projects

One forwarder, and one `remote-radios` container, serve every project you work
on. The container is the hub: it owns the physical radios and publishes them, so
other stacks do not each attach the hardware themselves.

| Consumer | Gets Thread/Zigbee via | Gets Bluetooth via |
|---|---|---|
| `barton` devcontainer | your otbr-agent, over the dev server's D-Bus | BlueZ, over the dev server's D-Bus |
| ZigbeeCore (`remote-zigbee` container) | the radios container's cpcd, shared with Thread | BlueZ, over the dev server's D-Bus |
| zilker | BartonCore's device service | BartonCore's device service |
| HH4 (QEMU guest) | the Silabs socket directly, under a claim | attaches the dongle itself over usb-ip |

Everything that runs on the dev server shares its kernel, so it can consume the
radios container concurrently over sockets and D-Bus. A **VM guest cannot**: it has its own
kernel and its own BlueZ, so it needs the devices themselves.

Both radios are therefore exclusive, and consumers take turns rather than
compete:

- **Silabs radio** — the workstation relay serves one client at a time. A
  consumer that wants it creates `~/.remote-radios/claims/silabs.claim`. The
  container watches that file, stops cpcd and its socat bridge while it exists,
  and reclaims the radio once it is removed. HH4's `hh4-run.sh` takes this
  claim and releases it on exit, including on interrupt, so handover needs no
  manual steps. Zigbee does not need it: it shares the radios container's cpcd.
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

The ref a workstation was installed from is remembered as `SETUP_REF` in
`~/.config/remote-radios/config`, and re-runs track that same ref. A workstation
installed from a branch or a commit therefore stays on it, instead of silently
replacing itself with whatever is currently on `main`. Set `REMOTE_RADIOS_REF`
explicitly to move it to a different ref.

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

### More than one workstation

You can set up several workstations, an office machine and a laptop say, but
only one of them forwards at a time: they all forward to the same sockets in
your `~/.remote-radios` on the dev server. Whichever is forwarding holds a
lease, `~/.remote-radios/owner` on the dev server, and refreshes it every 30
seconds. Your other workstations **stand by**: they open no tunnels, and their
dongles stay with their own Bluetooth. Their journal says who has the radios:

```text
Radios for you@devserver are being forwarded from other-workstation (forwarding since …); standing by.
```

A standing-by workstation takes over by itself when the lease is released,
because the forwarding workstation stopped its service or logged out, or when
the lease goes stale, because that workstation vanished without stopping (a
closed laptop, a lost VPN connection) and has not refreshed it for two minutes.

To switch immediately instead, run this on the workstation you want to use:

```bash
~/.config/remote-radios/bin/remote-radios-setup.sh --take-over
```

It claims the lease, waits about 45 seconds for the other workstation to
notice and stand down, then restarts the local service. The other workstation
stops forwarding without touching the sockets on the dev server, and its
dongle returns to its own Bluetooth. The radios container on the dev server
switches to the new radios by itself.

Isolation is per dev-server **user account**. Two developers with different
accounts never contend; this lease only arbitrates between your own
workstations.

Every one of your workstations must run a version of the setup that includes
the lease: an older one ignores it and will fight the others for the sockets.
Re-run the setup on each to update it.

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

**Dev server:** Docker with your user in the `docker` group. No `sudo` is
needed on the dev server for usb-ip — the dongle is attached **inside** the
privileged container. The dev server needs two things set up once, by an
administrator, for all developers:

1. **Its own Bluetooth daemon must be running.** Every developer's dongle is
   handed to it:

   ```bash
   sudo systemctl unmask bluetooth.service
   sudo systemctl enable --now bluetooth.service
   ```

   (An older version of this guide said to mask `bluetooth.service`; unmask it.)

2. **A D-Bus policy for the border routers.** Each developer's otbr-agent
   registers on the system bus as `io.openthread.BorderRouter.wpan<uid>`, and
   the default system-bus policy denies it:

   ```bash
   sudo install -m 0644 docker/remote-radios-host-dbus.conf \
       /etc/dbus-1/system.d/remote-radios.conf
   ```

   The policy only covers `io.openthread.BorderRouter.*` names: otbr-agent
   (root in each container) may own them and local users may call them. Any
   developer on the server can therefore call any border router, not only
   their own. `dbus-daemon` picks the file up automatically.

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
`remote-radios` container if it is not already running, then opens the Barton
shell pointed at the dev server's D-Bus and your border router, with a route to
your Thread network installed. `-T` cannot be combined with `-H`.

### Zigbee

Zigbee comes from ZigbeeCore's `remote-zigbee` container, which runs zigbeed and
ZigbeeCore against the radios container's cpcd. The multi-PAN radio serves both
networks at once: otbr-agent uses spinel interface 2 and zigbeed interface 1,
on the same channel. Set it up once from a ZigbeeCore checkout (see "Remote
radios" in its README):

```bash
./dockerw ./build.sh             # in ZigbeeCore
scripts/remote-zigbee.sh up      # zigbeed and ZigbeeCore
```

That records the checkout in `~/.remote-radios/zigbee.env`. From then on
`./dockerw -T` starts the container whenever it is not running, and points
Barton at it by setting `ZIGBEE_CORE_IP` to the container's IPv4 address on your
radios network (Barton accepts only a numeric address, because it trusts
ZigbeeCore's events by their source address). If the `remote-zigbee` container is
recreated and gets a new address, restart the barton container. ZigbeeCore is reached
over your private radios network, and sends its events back to the barton
container at its alias `barton` there. If two of your workspaces run at once,
both answer to that alias; only one can usefully use Zigbee anyway.

Without it, `./dockerw -T` says so, and Barton must be run with Zigbee disabled
(`-z` for the reference app); otherwise it waits for a ZigbeeCore that never
answers.

### Devcontainer (VS Code)

The overlay is intentionally **not** in the default devcontainer stack (it
would force every developer to build a privileged container and repoint D-Bus).
Use the `dockerw -T` CLI path for forwarded radios. Run it from a shell on the
dev server host (for example over SSH, or VS Code's integrated terminal
connected to the host rather than to the container); `dockerw` refuses to run
inside a container.

---

## Local radio (radio on the dev server itself)

If a Silabs radio is physically attached to the dev server, set `SILABS_DEVICE`
instead of using the tunnel:

```bash
SILABS_DEVICE=/dev/ttyACM0 ./dockerw -T bash
```

The machine with the radio needs the same one-time host setup as a dev server
(the Bluetooth daemon and the D-Bus policy above); without the policy,
otbr-agent cannot register on the system bus. To return to the tunnel later,
clear the saved device with `SILABS_DEVICE= ./dockerw -T bash`.

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
| `REMOTE_RADIOS_THREAD_INTERFACE` | Your Thread interface, `wpan<uid>`. Passed to otbr-agent as `-I` and to Barton as `BARTON_THREAD_INTERFACE` |
| `REMOTE_RADIOS_READY_WAIT_SECONDS` | How long `./dockerw -T` waits for otbr-agent to register `io.openthread.BorderRouter.<interface>` on D-Bus before opening the barton shell (default `60`). A timeout is not fatal: Barton reconnects and retries until the border router appears |

The reference app maps these variables, set in the barton container, onto Barton
properties. Barton validates each value when it reads the property; an invalid
value is logged and the default is used.

| Variable | Property | Meaning |
|---|---|---|
| `BARTON_BLE_ADAPTER_ID` | `barton.ble.adapterId` | BlueZ HCI index for BLE (0-9999, default `0`). `./dockerw -T` sets it from `ble_adapter_id` |
| `BARTON_THREAD_INTERFACE` | `barton.thread.interfaceName` | Thread interface, which selects the border router `io.openthread.BorderRouter.<name>` (default `wpan0`). Set from `REMOTE_RADIOS_THREAD_INTERFACE` |
| `BARTON_THREAD_ATTACH_WAIT_SECONDS` | `barton.thread.attachWaitSeconds` | Longest wait for the border router to form a new network (1-600, default `25`; the remote-radios overlay sets `120`, because CPC-based radios can need 35 s or more to attach and longer to publish the dataset) |
| `ZIGBEE_CORE_IP` | `barton.zigbee.address` | ZigbeeCore's IPv4 address, numeric only (default `127.0.0.1`). A stored `ZIGBEE_CORE_IP` system property takes precedence. `./dockerw -T` sets it when Zigbee is set up |

---

## Verifying the full stack

Run the validator from the dev server host, where it finds the `remote-radios`
container and re-execs into it automatically:

```bash
scripts/remote-radios/validate.sh          # human-readable
scripts/remote-radios/validate.sh --json   # machine-readable
```

It also runs directly inside the `remote-radios` container. It does **not** run
from the Barton devcontainer: that container has neither the Docker CLI nor the
Docker socket, so it cannot reach the radio container to re-exec into it.

It checks the dev server's D-Bus, the Silabs socket/socat/`/dev/ttyRadio`/cpcd
chain, otbr-agent and its registration on the shared bus, `mdnsd` and the
border router's MeshCoP advertisement, and (when a dongle is configured) usb-ip
reachability, the imported device, the dongle's HCI adapter, the dev server's
`bluetoothd`, `ble_adapter_id`, and a BLE scan that must see at least one
device. When the `remote-zigbee` container is running it also checks that
zigbeed is attached to the radio and that otbr-agent is on the other spinel
interface.

### Manual BLE verification

```bash
# From the remote-radios container:
cat /var/run/remote-radios-dbus/ble_adapter_id
nsenter --net=/run/host-netns hciconfig
nsenter --net=/run/host-netns bluetoothctl list
```

---

## Teardown

- **Workstation:** `systemctl --user stop remote-radios.service`. That releases
  the lease, removes the remote sockets and env hint, stops both tunnels, and
  returns the dongle to the normal kernel driver so your workstation's own
  Bluetooth can use it again. To stop it coming back at the next login, add
  `systemctl --user disable remote-radios.service`.
- **Dev server:** stop your radios container with

  ```bash
  cd docker
  docker compose -p "$USER-remote-radios" -f compose.yaml -f compose.remote-radios.yaml down
  ```

Teardown also happens automatically when the dev server becomes unreachable,
and the service stops at logout.

The radios container does **not** need stopping when your workstation goes
away. It keeps running without the radios, and connects them again as soon as
a workstation forwards them, whether that is the same one or another of yours.

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
- **The container logs "Silabs tunnel socket … is not present yet — starting
  without it"** — no workstation is forwarding the radio. Check that
  `remote-radios.service` is active on the workstation you are using and that
  your VPN is up. The container connects the radio by itself once it arrives;
  it does not need restarting.
- **Your workstation's journal says it is "standing by"** — another of your
  workstations holds the radios. Stop the service there, or run
  `remote-radios-setup.sh --take-over` here; see
  [More than one workstation](#more-than-one-workstation).
- **"usb-ip service not reachable"** — the reverse tunnel or `usbipd` on the
  workstation is down; check the service journal, or re-run the setup script.
- **"no new HCI device appeared after usb-ip attach"** — confirm the dongle is
  bound on the workstation (`usbip list -l`) and that the `vhci-hcd`/`usbip`
  kernel modules are available on the dev server.
- **BLE picks the wrong adapter** — check `ble_adapter_id`, and Barton's log
  line naming the adapter it uses; set `BARTON_BLE_ADAPTER_ID` explicitly if
  needed.
- **The Silabs radio is connected but silent** — cpcd sends, nothing comes
  back. This is usually the wrong RTS/CTS mode: some USB-serial adapters do not
  drive the hardware flow-control lines, so requesting flow control leaves RTS
  deasserted and the radio never transmits. `remote-serial.py` detects the
  signature (bytes flowing one way only) and flips the mode once automatically,
  logging `flipping RTS/CTS flow control`. To skip the six-second detection
  window, pass `--rtscts` or `--no-rtscts` explicitly.
- **cpcd keeps stopping, or the log says "Silabs radio claimed via ..."** — this
  is the claim protocol working. An `hh4-run.sh` guest is using the radio, so
  the container released it
  and is waiting. It reclaims the radio automatically once that run exits. If a
  stale claim is left behind after a crash, delete
  `~/.remote-radios/claims/silabs.claim`.
- **The reference app stops after "Using defaults" and prints nothing more** —
  Barton is waiting for ZigbeeCore. Set up the `remote-zigbee` container (see
  "Zigbee" above), or disable Zigbee (`-z`). If the container is running, check
  `docker logs $USER-remote-zigbee` and the validator's Zigbee section.
- **The validator reports "org.bluez absent" or "Controller hciN not
  available to the dev server's bluetoothd"** — the dev server's
  `bluetooth.service` is not running, or is masked. See the dev server
  prerequisites above.
- **"Border router on shared bus … not registered", or Matter logs
  `ServiceUnknown: The name io.openthread.BorderRouter.wpan<uid> was not
  provided`** — the dev server is missing the D-Bus policy
  (`/etc/dbus-1/system.d/remote-radios.conf`; see the prerequisites), or
  otbr-agent is still starting. otbr-agent registers about a minute after the
  container starts.
- **Matter finds a Thread device but cannot reach it** — check for the route
  with `ip -6 route show fd00::/8` in the Barton container. `./dockerw -T` and
  the devcontainer install it automatically; run
  `docker/setup-thread-routes.sh` to add it by hand.
- **BLE scan finds zero devices** — treated as a failure, not a warning: an
  adapter that is present but deaf is indistinguishable from a working one by
  any other check. Confirm the dongle's antenna and placement. Remember it
  scans what is near *the workstation forwarding the dongle*, not near you.
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
