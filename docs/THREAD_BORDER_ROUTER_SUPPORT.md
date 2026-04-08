# Thread Border Router Support

Barton communicates with `otbr-agent` over D-Bus when Thread support is enabled
(`BCORE_THREAD=ON`). Two modes are supported:

| Mode | When to use | How to start |
|------|-------------|-------------|
| **Simulated** (default) | Unit/integration tests, no hardware needed | `scripts/start-simulated-otbr.sh` |
| **Real USB radio** | Testing with real Thread devices | See below |

> **Note**: Be sure to enable Thread and Border Router support in the Matter SDK
> (`-DOT_THREAD_VERSION=1.4 -DOTBR_BORDER_ROUTING=ON`) if Matter is also enabled.

---

## Real USB Radio: BRD2703 xG24 Explorer Kit

The real-radio mode uses a **Silicon Labs BRD2703 xG24 Explorer Kit** connected over USB.
An optional Docker container (`otbr-radio`) runs two daemons:

- **`cpcd`** — Silicon Labs CPC daemon. Manages the USB serial link to the radio
  using the CPC multiplexing protocol.
- **`otbr-agent`** — OpenThread Border Router agent. Connects to `cpcd` via
  `spinel+cpc://cpcd_0` and exposes the Thread network on D-Bus.

A named Docker volume (`dbus-socket`) shares `/var/run/dbus` between the
`otbr-radio` container and the Barton devcontainer so that Barton can reach
`otbr-agent` over D-Bus with no code changes.

```
BRD2703 USB Radio
    │  /dev/ttyACM0
    ▼
  cpcd           (CPC daemon — hardware abstraction)
    │  spinel+cpc://cpcd_0
    ▼
  otbr-agent     (Thread network stack)
    │  D-Bus  io.openthread.BorderRouter.wpan0
    ▼
  Barton         (your application code)
```

---

## Flashing RCP Firmware onto the BRD2703

Before use, the BRD2703 must be flashed with a CPC-capable **RCP (Radio Co-Processor)**
firmware image.

If the board is new (out of the box), its debug adapter firmware must be updated first.
Connect the board to **Simplicity Studio** and accept any firmware upgrade prompts before
proceeding.

Download the firmware image appropriate for the BRD2703 (EFR32MG24) in either
`.s37` or `.hex` format, then flash it using
[Simplicity Commander](https://www.silabs.com/developers/simplicity-studio).

After flashing the RCP image, lock the security bits:

```bash
sudo commander security lock
```

---

## USB-IP: Forwarding the Radio from Laptop to Remote Dev Server

If your devcontainer runs on a **remote server** (common with VS Code Remote-SSH)
but the BRD2703 is plugged into your **local laptop**, use
[USB-IP](https://www.kernel.org/doc/html/latest/usb/usbip_protocol.html) to
forward the USB device over the network.

### On the laptop (USB-IP server — where the radio is plugged in)

```bash
# Update package lists
sudo apt update

# Install usbip tools
sudo apt install linux-tools-generic linux-cloud-tools-generic

# Load the kernel module
sudo modprobe usbip_host

# Find the bus ID of the BRD2703 (look for "Silicon Labs" or "CP210x")
usbip list -l
# Example output:
#   - busid 1-2 (10c4:ea60)
#     Silicon Labs : CP210x UART Bridge (10c4:ea60)

# Bind the device to usbip
sudo usbip bind -b <bus-id>

# If the bind fails, check the correct bus ID and load the kernel modules, then retry
sudo modprobe usbip-core
sudo modprobe usbip-host
sudo modprobe vhci-hcd

# Start the USB-IP daemon (listens on TCP port 3240)
sudo usbipd -D

# Confirm that port 3240 is listening
netstat -an | grep 3240

# Due to firewall restrictions on the laptop, the remote server cannot connect
# to port 3240 directly. Use an SSH reverse tunnel to work around this.
# Keep this terminal open while working with the device.
ssh -R 3240:localhost:3240 <username>@<remote_ip>
```

### On the remote dev server (USB-IP client — where Docker runs)

```bash
# Install usbip tools
sudo apt install linux-tools-generic hwdata

# Load the kernel module
sudo modprobe vhci-hcd

# List the available devices via the tunnel and confirm the BRD2703 is visible
usbip list -r 127.0.0.1

# Attach the remote device (replace the bus ID as appropriate)
sudo usbip attach -r 127.0.0.1 -b <bus-id>

# Confirm the device appeared
ls -la /dev/ttyACM*
# Expected: /dev/ttyACM0
```

> **Tip**: If the device appears at `/dev/ttyACM1` or another path, set
> `RADIO_DEVICE` before starting:
> ```bash
> export RADIO_DEVICE=/dev/ttyACM1
> ```

---

## Starting the Real-Radio Container

### CLI (`dockerw`)

Use the `-T` flag. This adds `docker/compose.otbr-radio.yaml` to the compose
stack and starts the `otbr-radio` container alongside the main Barton container:

```bash
./dockerw -T
```

To override the radio device or backbone interface:

```bash
RADIO_DEVICE=/dev/ttyACM1 BACKBONE_IF=eth1 ./dockerw -T
```

This creates two containers: the `barton` container and the `otbr-radio` container.

To check the logs and confirm all services started correctly:

```bash
docker compose -f docker/compose.yaml -f docker/compose.otbr-radio.yaml logs -f otbr-radio
```

Expected log progression:

- `Using USB radio device: /dev/ttyACM1` (or your selected device)
- `Connected to Secondary`
- `Secondary CPC v4.4.6`
- `Daemon startup was successful. Waiting for client connections`
- `Starting otbr-agent`

To enter the `otbr-radio` container, find its container ID and run:

```bash
docker exec -it <container-id> /bin/bash
```

Verify that `otbr-agent` is running:

```bash
pgrep -a otbr-agent
```

Check the runtime cpcd configuration:

```bash
cat /usr/local/etc/cpcd.conf
```

Check the Thread stack state inside otbr-radio container:

```bash
ot-ctl state
```

If `ot-ctl state` returns `disabled`, the radio and `otbr-agent` are healthy but no thread network has been formed yet. Run the following commands to bring up the interface and form a new network:

```bash
ot-ctl dataset init new
ot-ctl dataset commit active
ot-ctl ifconfig up
ot-ctl thread start
# After few seconds check the state
ot-ctl state
```

Expected result:

- `leader` — this node formed a new Thread network
- `router` — this node joined an existing Thread network

The `otbr-radio` container continues running after you exit the shell. To stop and
remove it explicitly:

```bash
docker compose -f docker/compose.yaml -f docker/compose.otbr-radio.yaml rm -sf otbr-radio
```

### Devcontainer (VS Code)

Add `compose.otbr-radio.yaml` to the `dockerComposeFile` array in
`.devcontainer/devcontainer.json`:

```jsonc
"dockerComposeFile": [
    "../docker/compose.devcontainer.yaml",
    "../docker/compose.otbr-radio.yaml"   // add this line
],
```

Then rebuild the devcontainer (**Dev Containers: Rebuild Container**).

To set a non-default radio device, export `RADIO_DEVICE` in your shell before
opening VS Code, or set it in `docker/.env` after running
`docker/setupDockerEnv.sh`.

---

## Verifying the D-Bus Interface

### From inside the `otbr-radio` container

`gdbus` is available and can be used to query the OTBR D-Bus service:

```bash
gdbus introspect --system \
    --dest io.openthread.BorderRouter.wpan0 \
    --object-path /io/openthread/BorderRouter/wpan0
```

To read the current Thread device role:

```bash
gdbus call --system \
    --dest io.openthread.BorderRouter.wpan0 \
    --object-path /io/openthread/BorderRouter/wpan0 \
    --method org.freedesktop.DBus.Properties.Get \
    io.openthread.BorderRouter DeviceRole
```

### From inside the Barton container

The `dbus-socket` volume shares `/var/run/dbus` between the two containers, so
the Barton container can also reach the OTBR D-Bus service:

```bash
gdbus introspect --system \
    --dest io.openthread.BorderRouter.wpan0 \
    --object-path /io/openthread/BorderRouter/wpan0
```
If Barton is built with `BCORE_THREAD=ON`, it will automatically discover and
communicate with the border router over D-Bus on startup.
