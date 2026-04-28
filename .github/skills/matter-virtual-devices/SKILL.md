---
name: matter-virtual-devices
description: Work with Matter test devices in BartonCore. Use when the user wants to start Matter sample apps, use chip-tool to commission or control devices, work with matter.js virtual devices for integration testing, or create new custom virtual device types. Covers pre-built sample apps, chip-tool commands, and the matter.js sideband API.
license: Apache-2.0
compatibility: Requires the BartonCore Docker development container with Matter sample apps and chip-tool pre-installed at /usr/local/bin/.
metadata:
  author: rdkcentral
  version: "1.0"
---

# Matter Virtual Devices

BartonCore has two categories of Matter test devices:

1. **Pre-built CHIP SDK sample apps** — standalone binaries in the Docker container for manual testing
2. **matter.js virtual devices** — Node.js-based devices with a Python wrapper and sideband API for integration tests

## Pre-Built Sample Apps

Available in the Docker container at `/usr/local/bin/`:

| Binary | Device Type |
|--------|-------------|
| `chip-lighting-app` | Light (OnOff, LevelControl, ColorControl) |
| `chip-lock-app` | Door Lock |
| `thermostat-app` | Thermostat |
| `contact-sensor-app` | Contact Sensor |

### Starting a Sample App

Each app supports common Matter arguments:

```bash
chip-lighting-app --discriminator 3840 --KVS /tmp/chip-light-kvs
```

Common flags:
- `--discriminator <value>` — Set the BLE/DNS-SD discriminator (default varies)
- `--KVS <path>` — Key-value store file path (use unique paths to avoid conflicts)
- `--secured-device-port <port>` — Operational port (default 5540)

Start in background for automated use:

```bash
chip-lighting-app --discriminator 3840 --KVS /tmp/chip-light-kvs &
```

### Default Pairing Codes

The sample apps use default commissioning data. The default setup pin is `20202021` and the default pairing code is `MT:-24J0AFN00KA0648G00`.

## chip-tool

`chip-tool` is the Matter SDK's CLI controller. Available at `/usr/local/bin/chip-tool`.

### Commission a Device

```bash
chip-tool pairing code 0x1234 MT:-24J0AFN00KA0648G00
```

- `0x1234` is the node ID you assign to the device
- The pairing code comes from the device's output or its default

### Cluster Commands

```bash
# Toggle a light
chip-tool onoff toggle 0x1234 1

# Turn on
chip-tool onoff on 0x1234 1

# Turn off
chip-tool onoff off 0x1234 1

# Lock a door
chip-tool doorlock lock-door 0x1234 1 --timedInteractionTimeoutMs 1000

# Unlock a door
chip-tool doorlock unlock-door 0x1234 1 --timedInteractionTimeoutMs 1000
```

The last argument (`1`) is the endpoint ID.

### Read Attributes

```bash
chip-tool onoff read on-off 0x1234 1
chip-tool levelcontrol read current-level 0x1234 1
chip-tool doorlock read lock-state 0x1234 1
```

## matter.js Virtual Devices

The integration test framework uses matter.js virtual devices backed by Node.js. These offer programmatic control via a sideband API.

### Available Device Types

| Python Class | JS Entry Point | Location |
|---|---|---|
| `MatterLight` | `LightDevice.js` | `testing/mocks/devices/matter/matter_light.py` |
| `MatterDoorLock` | `DoorLockDevice.js` | `testing/mocks/devices/matter/matter_door_lock.py` |

### How They Work

1. Python wrapper class (`MatterDevice` subclass) spawns a Node.js subprocess
2. The Node.js process starts a matter.js device and prints a JSON ready signal to stdout
3. Python parses the ready signal and configures a `SidebandClient` on the reported port
4. Tests interact with the device via `device.sideband`

### Sideband API

```python
# Send a command to the device (simulates device-side actions)
device.sideband.send("toggle")

# Get device state
state = device.sideband.get_state()
is_on = state["onOff"]
```

### Using in Tests (via pytest fixtures)

```python
@pytest.mark.requires_matterjs
def test_light_toggle(matter_light, default_environment):
    client = default_environment.get_client()
    client.commission_device(matter_light.get_commissioning_code(), timeout=30)
    # ... test interactions
```

The `matter_light` and `matter_door_lock` fixtures are available as pytest fixtures.

### Prerequisites for matter.js Devices

```bash
npm --prefix testing/mocks/devices/matterjs ci
```

Tests using matter.js devices should be marked with `@pytest.mark.requires_matterjs`.

## Creating a Custom Virtual Device

To create a new matter.js device type:

1. **Create the JavaScript entry point** at `testing/mocks/devices/matterjs/src/MyDevice.js` — extend `VirtualDevice` from `VirtualDevice.js`
2. **Create the Python wrapper** at `testing/mocks/devices/matter/matter_my_device.py` — extend `MatterDevice` and set `matterjs_entry_point="MyDevice.js"`
3. **Add a pytest fixture** in the Python file for test discovery
4. **Write integration tests** in `testing/test/` that use the new fixture

The human decides whether custom devices should be committed to the test harness. If they are useful for ongoing testing, commit them with accompanying integration tests.

## Error Recovery

If sample app binaries, `chip-tool`, or `node` are not found:

1. Check if `/.dockerenv` exists
2. If it does NOT exist, stop and tell the user: **"Matter tools are not available. Please run inside the BartonCore development container."**
3. If it does exist but binaries are missing, the Docker image may need rebuilding
