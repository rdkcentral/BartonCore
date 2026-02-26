# SBMD matter.js Cluster Integration

This document describes the integration of the [matter.js](https://github.com/matter-js/matter.js) cluster library into the SBMD (Specification-Based Matter Drivers) QuickJS runtime for TLV encoding/decoding.

## Overview

The matter.js project provides a complete Matter implementation in TypeScript/JavaScript. Barton integrates just the cluster library portion to enable SBMD JavaScript mappers to perform type-safe TLV encoding and decoding without relying on the CHIP SDK's JSON/TLV conversion utilities.

### Benefits

- **Type-Safe TLV Encoding**: matter.js provides strongly-typed TLV schemas for all Matter clusters
- **Schema Validation**: Encoding validates data against the Matter specification
- **Future Compatibility**: Updates to matter.js automatically include new cluster definitions
- **Simplified Scripts**: SBMD mappers can use high-level JavaScript objects instead of raw JSON/TLV manipulation

## Architecture

```
┌──────────────────────────────────────────────────────────────────────────┐
│                            SBMD Script Runtime                           │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌────────────────────────────────────────────────────────────────────┐  │
│  │                          QuickJS Engine                            │  │
│  │                                                                    │  │
│  │  ┌──────────────────────┐    ┌──────────────────────────────────┐  │  │
│  │  │  MatterClusters      │    │  SBMD Mapper Script              │  │  │
│  │  │  (Global Object)     │◀───│  (User JavaScript)               │  │  │
│  │  │                      │    │                                  │  │  │
│  │  │  - LevelControl      │    │  // Example:                     │  │  │
│  │  │  - OnOff             │    │  const { LevelControl } =        │  │  │
│  │  │  - DoorLock          │    │      MatterClusters;             │  │  │
│  │  │  - ColorControl      │    │  const cmd = LevelControl        │  │  │
│  │  │  - Thermostat        │    │      .TlvMoveToLevelRequest;     │  │  │
│  │  │  - ...               │    │  const tlv = cmd.encode({...});  │  │  │
│  │  └──────────────────────┘    └──────────────────────────────────┘  │  │
│  │                                                                    │  │
│  └────────────────────────────────────────────────────────────────────┘  │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

## Build Process

### Prerequisites

- Node.js and npm (for building the matter.js bundle)
- Python 3 (for embedding the bundle as C header)

### Building the Bundle

The matter.js cluster bundle is built automatically during the CMake build process:

```bash
# Full build includes matter.js cluster bundle
./build.sh

# Or build just the cluster bundle manually
./scripts/build-matterjs-clusters.sh -o build/matterjs-clusters
```

### Build Script Options

```
Usage: ./scripts/build-matterjs-clusters.sh [-o <output_dir>] [-v <version>] [-h]
  -o  Output directory for the bundled JavaScript (default: build/matterjs-clusters)
  -v  matter.js version/tag to use (default: main)
  -h  Display help
```

### Build Artifacts

| File | Description |
|------|-------------|
| `build/matterjs-clusters/matter-clusters.js` | Bundled JavaScript for QuickJS |
| `build/core/src/MatterJsClustersEmbedded.h` | Embedded C header with bundle |

## Usage in SBMD Scripts

### Accessing the Cluster Library

The matter.js clusters are exposed as a global `MatterClusters` object. Each cluster has a nested namespace containing the TLV types:

```javascript
// Access cluster TLV types (note the nested namespace)
const { LevelControl } = MatterClusters;
const TlvMoveToLevelRequest = LevelControl.LevelControl.TlvMoveToLevelRequest;

// Or destructure directly
const { TlvMoveToLevelRequest } = MatterClusters.LevelControl.LevelControl;
```

### TLV Encoding

Convert JavaScript objects to binary TLV:

```javascript
// Level Control - MoveToLevel command
const TlvMoveToLevelRequest = MatterClusters.LevelControl.LevelControl.TlvMoveToLevelRequest;

const commandData = {
    level: 50,
    transitionTime: null,
    optionsMask: { executeIfOff: false, coupleColorTempToLevel: false },
    optionsOverride: { executeIfOff: false, coupleColorTempToLevel: false }
};

// Encode to Uint8Array
const tlvBytes = TlvMoveToLevelRequest.encode(commandData);

// Return invoke operation with base64-encoded TLV
return {
    invoke: {
        clusterId: 0x0008,
        commandId: 0x0000,  // MoveToLevel
        tlvBase64: btoa(String.fromCharCode(...tlvBytes))
    }
};
```

### TLV Decoding

Convert binary TLV to JavaScript objects:

```javascript
// Receive TLV bytes from base64 using SbmdUtils
const tlvBytes = SbmdUtils.Base64.decode(sbmdReadArgs.tlvBase64);

// Decode to JavaScript object
const TlvMoveToLevelRequest = MatterClusters.LevelControl.LevelControl.TlvMoveToLevelRequest;
const data = TlvMoveToLevelRequest.decode(tlvBytes);

// data = { level: 50, transitionTime: null, optionsMask: {...}, optionsOverride: {...} }
return { output: data.level.toString() };
```

### Example: Complete Light Level Write Mapper

```javascript
// SBMD write mapper for light level using matter.js
const TlvMoveToLevelWithOnOffRequest = MatterClusters.LevelControl.LevelControl.TlvMoveToLevelWithOnOffRequest;

// Parse input percentage
let percent = parseInt(sbmdWriteArgs.input, 10);
if (isNaN(percent)) percent = 0;
if (percent < 0) percent = 0;
if (percent > 100) percent = 100;

// Convert to Matter level (0-254)
const level = Math.round(percent / 100 * 254);

// Create typed command data
const commandData = {
    level: level,
    transitionTime: null,
    optionsMask: { executeIfOff: false, coupleColorTempToLevel: false },
    optionsOverride: { executeIfOff: false, coupleColorTempToLevel: false }
};

// Encode using matter.js TLV schema
const tlvBytes = TlvMoveToLevelWithOnOffRequest.encode(commandData);

// Return invoke operation with base64-encoded TLV
return {
    invoke: {
        clusterId: 0x0008,  // LevelControl
        commandId: 0x0004,  // MoveToLevelWithOnOff
        tlvBase64: btoa(String.fromCharCode.apply(null, tlvBytes))
    }
};
```

## Available Clusters

The bundle includes TLV schemas for all standard Matter clusters:

| Cluster | Namespace | Common TLV Types |
|---------|-----------|------------------|
| On/Off | `OnOff` | `TlvOnOffAttribute` |
| Level Control | `LevelControl` | `TlvMoveToLevelRequest`, `TlvCurrentLevelAttribute` |
| Door Lock | `DoorLock` | `TlvLockDoorRequest`, `TlvLockStateAttribute` |
| Color Control | `ColorControl` | `TlvMoveToColorRequest`, `TlvColorCapabilitiesAttribute` |
| Thermostat | `Thermostat` | `TlvSetpointRaiseLowerRequest`, `TlvLocalTemperatureAttribute` |
| Window Covering | `WindowCovering` | `TlvGoToLiftPercentageRequest` |
| Fan Control | `FanControl` | `TlvFanModeAttribute` |
| Identify | `Identify` | `TlvIdentifyRequest` |
| Groups | `Groups` | `TlvAddGroupRequest` |
| Scenes | `ScenesManagement` | `TlvAddSceneRequest` |

See the [matter.js types package](https://github.com/matter-js/matter.js/tree/main/packages/types/src/clusters) for the complete list.

## TLV Encoding in Write/Execute Mappers

SBMD write and execute mappers must return `invoke` or `write` objects with a `tlvBase64`
field containing the pre-encoded TLV data. The matter.js cluster library provides type-safe
encoding with schema validation for this purpose.

### Write Operations

Return an `invoke` or `write` object with `tlvBase64` containing the encoded TLV:

```yaml
mapper:
  write:
    script: |
      // Use MatterClusters for type-safe TLV encoding
      const { TlvUInt8 } = MatterClusters.Tlv;

      const level = parseInt(sbmdWriteArgs.input, 10);
      const tlvBytes = TlvUInt8.encode(level);

      return {
        write: {
          clusterId: 0x0008,      // Level Control
          attributeId: 0x0000,    // CurrentLevel
          tlvBase64: btoa(String.fromCharCode(...tlvBytes))
        }
      };
```

For command invocations:

```yaml
mapper:
  write:
    script: |
      const isOn = sbmdWriteArgs.input === 'true';

      // Select command based on input
      return {
        invoke: {
          clusterId: 0x0006,    // OnOff cluster
          commandId: isOn ? 0x0001 : 0x0000,  // On or Off
          tlvBase64: ""         // No arguments (empty TLV)
        }
      };
```

### Execute Operations

```yaml
mapper:
  execute:
    script: |
      const { DoorLock } = MatterClusters;
      const TlvLockDoorRequest = DoorLock.DoorLock.TlvLockDoorRequest;

      // Build command data
      const cmdData = { pinCode: null };
      if (sbmdCommandArgs.input.length > 0) {
        cmdData.pinCode = new TextEncoder().encode(sbmdCommandArgs.input[0]);
      }

      const tlvBytes = TlvLockDoorRequest.encode(cmdData);

      // Return with tlvBase64 for pre-encoded TLV
      return {
        invoke: {
          clusterId: 0x0101,        // DoorLock cluster
          commandId: 0x0000,        // LockDoor command
          tlvBase64: btoa(String.fromCharCode(...tlvBytes)),
          timedInvokeTimeoutMs: 10000
        }
      };
```

### Return Object Format

Write and execute mappers return `invoke` or `write` objects with `tlvBase64`:

**For attribute writes:**
```javascript
return {
  write: {
    clusterId: <number>,      // Matter cluster ID
    attributeId: <number>,    // Matter attribute ID
    tlvBase64: <string>       // Base64-encoded TLV data (from matter.js)
  }
};
```

**For command invocations:**
```javascript
return {
  invoke: {
    clusterId: <number>,           // Matter cluster ID
    commandId: <number>,           // Matter command ID
    tlvBase64: <string>,           // Base64-encoded TLV data (from matter.js)
    timedInvokeTimeoutMs?: <number> // Optional timed invoke timeout
  }
};
```

> **Note:** The `tlvBase64` field is required for any command or attribute write that
> has arguments. If omitted, an empty TLV structure is used (suitable only for commands
> with no parameters, like OnOff's `On` and `Off` commands).

## Error Handling

### Schema Validation Errors

If encoding fails due to invalid data, matter.js throws a JavaScript exception:

```javascript
try {
    const tlv = LevelControl.TlvMoveToLevelRequest.encode({
        level: 300,  // Invalid: exceeds max of 254
        transitionTime: null,
        optionsMask: {},
        optionsOverride: {}
    });
} catch (e) {
    // Exception: "level" value 300 exceeds maximum 254
}
```

## Script Type Selection

SBMD drivers that use matter.js for TLV encoding must specify the `scriptType` field
in the spec file to indicate the runtime dependency:

```yaml
schemaVersion: "1.0"
driverVersion: "1.0"
name: "Level Control"
scriptType: "JavaScript+matterjs"  # Required when using MatterClusters
```

### scriptType Values

| Value | Description |
|-------|-------------|
| `JavaScript` | Default. Script uses only `SbmdUtils` for all operations. |
| `JavaScript+matterjs` | Script requires the `MatterClusters` global for TLV encoding. |

When `scriptType: "JavaScript+matterjs"` is specified, the SBMD runtime loads the
matter.js cluster bundle into the QuickJS context before executing any mapper scripts.
If the matter.js bundle is not available (e.g., Node.js was not installed during
the build or the matter.js feature was not enabled), device initialization will fail
with an error rather than silently falling back to a non-functional state.

## Development

### Updating matter.js Version

1. Edit `MATTERJS_VERSION` in CMake or pass via command line:
   ```bash
   cmake -DMATTERJS_VERSION=v1.0.0 ..
   ```

2. Rebuild:
   ```bash
   rm -rf build/matterjs-*
   ninja matterjs_clusters
   ```

### Debugging Bundle Issues

Enable debug logging to see bundle loading status:

```bash
export ICLOG_LEVEL=DEBUG
./barton-device-service
```

Look for log messages from `MatterJsClusterLoader`:

```
[DEBUG] MatterJsClusterLoader: matter.js cluster bundle loaded from embedded source
[DEBUG] MatterJsClusterLoader: MatterClusters global is available
```

### Testing Bundle Manually

Test the bundle with QuickJS directly:

```bash
qjs -e "
    load('build/matterjs-clusters/matter-clusters.js');
    const { LevelControl } = MatterClusters;
    console.log('TlvMoveToLevelRequest:', typeof LevelControl.TlvMoveToLevelRequest);
"
```

## Files

| Path | Description |
|------|-------------|
| `scripts/build-matterjs-clusters.sh` | Build script for matter.js bundle |
| `scripts/embed-js-as-header.py` | Embeds JS bundle as C header |
| `config/cmake/modules/BCoreMatterJsClusters.cmake` | CMake module for integration |
| `core/deviceDrivers/matter/sbmd/script/MatterJsClusterLoader.h` | C++ loader header |
| `core/deviceDrivers/matter/sbmd/script/MatterJsClusterLoader.cpp` | C++ loader implementation |

## License

The matter.js project is licensed under Apache-2.0, compatible with Barton's license.
