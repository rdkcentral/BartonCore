---
name: reference-app
description: Run and interact with the BartonCore reference application. Use when the user wants to start the reference app, commission devices, execute/read/write resources, or troubleshoot runtime issues. Covers CLI flags, interactive commands, port management, storage cleanup, and known pitfalls.
license: Apache-2.0
compatibility: Requires the BartonCore Docker development container and a completed build.
metadata:
  author: rdkcentral
  version: "1.0"
---

# Reference App

The reference app (`build/reference/barton-core-reference`) is an interactive CLI application for exercising BartonCore device drivers at runtime.

## CLI Flags

| Flag | Short | Description |
|------|-------|-------------|
| `--sbmd-dirs` | `-b` | Semicolon-delimited SBMD spec directories |
| `--noZigbee` | `-z` | Disable Zigbee subsystem |
| `--noThread` | `-t` | Disable Thread subsystem |
| `--noMatter` | `-m` | Disable Matter subsystem |
| `--novt100` | `-1` | Disable interactive linenoise prompt |
| `--storage-dir` | `-d` | Persisted storage directory |
| `--wifi-ssid` | `-s` | Wi-Fi SSID for commissioning |
| `--wifi-password` | `-p` | Wi-Fi password for commissioning |

## Starting the Reference App

Always supply at minimum the storage directory and SBMD spec directory. Disable subsystems you are not using to avoid DBus/daemon dependency errors:

```bash
cd /path/to/worktree
build/reference/barton-core-reference -z -t -d /tmp/barton-ref-cam -b /tmp/sbmd-only-v4
```

Wait for these log lines before sending commands:
- `Server Listening...`
- `Subsystem manager is ready for devices`

## Interactive Commands

Type `help` at the `barton-core>` prompt to see all available commands with their aliases and arguments. Commands are defined in `reference/src/coreCategory.c` and `reference/src/matterCategory.c`.

### Resource URI Format

Resource operations (`rr`, `wr`, `er`) use URIs of the form:

```
/<device-uuid>/ep/<endpoint-name>/r/<resource-name>
```

Example:
```
er /36c8c685ed5dcc8e/ep/camera/r/createSession
er /36c8c685ed5dcc8e/ep/camera/r/stream 1
```

---

## Known Pitfalls

### Matter: TCP Port 5540 Conflict

The reference app uses TCP port 5540 for its Matter controller. If a Matter sample app (e.g. `chip-camera-app`, `chip-lighting-app`) is also running on the default port, the reference app will fail with "Incorrect state" or "port already in use."

**Solution:** Start sample apps on a different port:

```bash
chip-camera-app --discriminator 3841 --KVS /tmp/chip-camera-kvs --secured-device-port 5542
```

The reference app always uses 5540 — there is no flag to change it.

### Matter: Stale KVS Causes Init Failures

The Matter SDK persists fabric/commissioning state in `~/.brtn-ds/matter/`. If this gets corrupted or was left from a previous session, the controller factory fails to reinitialize with errors like "Device Controller Factory already initialized" or "Incorrect state."

**Solution:** Delete stale state before a fresh start:

```bash
rm -rf ~/.brtn-ds/matter/ /tmp/barton-ref-cam /tmp/chip-camera-kvs
```

Always clean all three locations (reference app storage, hidden Matter KVS, and sample app KVS) together.

### Matter: Missing CLI Flags

The reference app will fail or misbehave without the proper flags:

| Missing Flag | Symptom |
|-------------|---------|
| `-b <sbmd-dir>` | Devices commission but are never claimed — no endpoints or resources appear |
| `-z` (when no Zigbee daemon) | Startup hangs or errors connecting to Zigbee DBus |
| `-t` (when no Thread daemon) | Startup hangs or errors connecting to Thread DBus |
| `-d <dir>` | Writes to default storage which may conflict with other sessions |

**Minimum viable invocation for Matter-only testing:**

```bash
build/reference/barton-core-reference -z -t -d /tmp/barton-ref-storage -b <path-to-sbmd-specs>
```

---

## Clean Start Procedure

When things go wrong, use this sequence to reset completely:

```bash
# 1. Kill running processes
pkill -9 barton-core-reference 2>/dev/null
pkill -9 chip-camera-app 2>/dev/null  # or whatever sample app

# 2. Remove all persisted state
rm -rf /tmp/barton-ref-cam /tmp/chip-camera-kvs ~/.brtn-ds/matter/

# 3. Start sample app on non-conflicting port
chip-camera-app --discriminator 3841 --KVS /tmp/chip-camera-kvs --secured-device-port 5542

# 4. Start reference app
build/reference/barton-core-reference -z -t -d /tmp/barton-ref-cam -b core/deviceDrivers/matter/sbmd/specs
```

## Default Pairing Codes

| Sample App | Discriminator | Passcode | Setup Code |
|-----------|---------------|----------|------------|
| chip-camera-app | 3841 | 20202021 | `MT:-24J0CEK01KA0648G00` |
| chip-lighting-app | 3840 | 20202021 | `MT:-24J0AFN00KA0648G00` |
