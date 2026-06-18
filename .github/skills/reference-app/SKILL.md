---
name: reference-app
description: Run and interact with the BartonCore reference application. Use when the user wants to start the reference app, commission devices, execute/read/write resources, or troubleshoot runtime issues. Covers CLI flags, interactive commands, isolated session management, and known pitfalls.
license: Apache-2.0
compatibility: Requires the BartonCore Docker development container and a completed build.
metadata:
  author: rdkcentral
  version: "2.0"
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

### Isolation Strategy (IMPORTANT)

Each session MUST use its own **unique storage directory** and **unique sample-app KVS path**. This ensures complete isolation from any other running or previously-run instances — no need to kill previous processes or clear existing state.

The dev build is compiled with `BCORE_MATTER_USE_RANDOM_PORT=ON`, which means the reference app's Matter controller binds to a random OS-assigned port (not a fixed port). This eliminates port conflicts between concurrent reference app instances.

Choose a unique session name (e.g., based on the feature or test you're running) and derive all paths from it:

```bash
SESSION="my-session-name"
STORAGE_DIR="/tmp/barton-ref-${SESSION}"
SAMPLE_KVS="/tmp/chip-${SESSION}-kvs"
```

Using a fresh `STORAGE_DIR` guarantees a clean Matter KVS (stored at `<STORAGE_DIR>/matter/`), clean general storage, and no interaction with other sessions. You do NOT need to:
- Kill previous processes
- Remove `~/.brtn-ds/matter/` or any other directories
- Check whether ports are free

### Launch sequence

**Step 1 — Start the sample app first**, with the session-specific KVS and a unique port:

```bash
<sample-app> --discriminator <disc> --KVS "$SAMPLE_KVS" --secured-device-port <unique-port>
```

Pick a `--secured-device-port` that differs from any other running sample app (e.g., 5542, 5544, 5546). Scrape the sample app's startup log for the setup code (look for `SetupQRCode` or `Manual pairing code`).

**Step 2 — Start the reference app.** Always supply at minimum the storage directory and SBMD spec directory. Disable subsystems you are not using to avoid DBus/daemon dependency errors:

```bash
cd /path/to/worktree
build/reference/barton-core-reference -z -t -d "$STORAGE_DIR" -b <path-to-sbmd-specs>
```

**Step 3 — Wait** for these log lines before sending commands:
- `Server Listening...`
- `Subsystem manager is ready for devices`

### Cleaning up a session (optional)

If you want to start a session completely fresh (e.g., to re-commission a device), delete only that session's state:

```bash
rm -rf "$STORAGE_DIR" "$SAMPLE_KVS"
```

This does not affect any other sessions or the default `~/.brtn-ds/` directory.

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

### Matter: Sample App Port Conflicts

If multiple sample apps are running, each must use a distinct `--secured-device-port`. If two sample apps bind the same port, the second will fail with "Address already in use."

**Solution:** Assign unique ports per sample app instance (e.g., 5542, 5544, 5546).

### Matter: Stale KVS Causes Init Failures

If you reuse a `--storage-dir` from a previous session that ended badly, the Matter controller factory may fail with "Device Controller Factory already initialized" or "Incorrect state."

**Solution:** Use a fresh storage directory, or delete the old one:

```bash
rm -rf "$STORAGE_DIR"
```

The Matter KVS lives at `<storage-dir>/matter/`, so deleting the storage directory removes it.

### Matter: Shared `.ini` Config Files

The Matter SDK's PosixConfig storage objects (`chip_factory.ini`, `chip_config.ini`, `chip_counters.ini`) are `static` globals that always write to the compile-time path `~/.brtn-ds/matter/`. These files are **shared** across all concurrent sessions and cannot be redirected at runtime without Matter SDK changes. The main KVS (`matterkv`) IS properly isolated per session.

In practice, the `.ini` files rarely cause conflicts between concurrent sessions. If you encounter issues, stop all sessions, delete `~/.brtn-ds/matter/chip_*.ini`, and restart.

### Matter: Missing CLI Flags

The reference app will fail or misbehave without the proper flags:

| Missing Flag | Symptom |
|-------------|---------|
| `-b <sbmd-dir>` | Devices commission but are never claimed — no endpoints or resources appear |
| `-z` (when no Zigbee daemon) | Startup hangs or errors connecting to Zigbee DBus |
| `-t` (when no Thread daemon) | Startup hangs or errors connecting to Thread DBus |
| `-d <dir>` | Writes to default `~/.brtn-ds/` which may conflict with other sessions |

**Minimum viable invocation for Matter-only testing:**

```bash
SESSION="test1"
build/reference/barton-core-reference -z -t -d "/tmp/barton-ref-${SESSION}" -b <path-to-sbmd-specs>
```


