# Specification-Based Matter Drivers (SBMD)

> ## ⚠️ Known Issues and Limitations
>
> This is the **first release** of SBMD support. It is considered **early access** and
> will likely receive significant schema and interface changes in the next release.
>
> - **Shared resources not yet factored out.** Some SBMD drivers define an
>   `identifySeconds` resource inline. This resource (and others common to all devices)
>   will be refactored into common/base driver code in a future release.
>
> - **No formal "skip update" mechanism for scripts.** Currently, a script block
>   indicates that the related resource should not be updated by returning a `null`
>   output or an invalid JSON object (which causes an error). This behavior will be
>   formalized and corrected in a future release.
>
> - **matter.js script support is optional and under evaluation.** Enabling
>   `JavaScript+matterjs` adds significant runtime resource overhead (~1 MB RAM).
>   Some drivers use only `SbmdUtils` for TLV operations while others use the
>   matter.js cluster library; both approaches are present intentionally to allow
>   comparison. Once more complex drivers are implemented (e.g., door lock user/PIN
>   code management), the method will be standardized. `SbmdUtils` is expected to
>   always be available going forward and will have a growing library of lower-level
>   utility functions.
>
> - **Verbose logging.** Logging output is very verbose at the moment, especially the
>   frequent dumps of the entire device data cache JSON. This will be reduced.
>
> - **No multi-instance cluster support.** Devices that expose multiple instances of
>   the same cluster on different Matter endpoints (e.g., IKEA BILRESA) are not yet
>   supported. This will be addressed in the next release.

## 1. Introduction

### 1.1 Purpose

Specification-Based Matter Drivers (SBMD) is a device driver framework that enables
Barton to support Matter devices through declarative YAML specification files rather
than compiled C/C++ code. This approach facilitates:

- **Rapid device type support**: Add new Matter device types without code changes
- **Dynamic extensibility**: Deploy new device support without firmware updates
- **Simplified maintenance**: Declarative specifications are easier to review and maintain
- **Reduced complexity**: Eliminate per-device-type native code compilation

### 1.2 Historical Context

Barton device drivers are responsible for bridging Barton's resource-based device
data model to device-specific interfaces like Matter, Zigbee, etc. Historically,
these drivers have been written in C/C++.

The idea of device drivers as specifications started around 2015 related to Zigbee
driver authoring. While complexities with proprietary message timing caused that
effort to be shelved, the concept resurfaced with OCF device support and now Matter,
where the need to add custom native code for each supported device type adds too
much friction to the goal of virtually unlimited device support.

SBMD addresses this by leveraging textual specification documents that provide the
mapping between Matter types and Barton resources, enabling dynamically extending
supported device types without requiring rebuilding and redeployment of the core
binaries through firmware updates.

## 2. High-Level Architecture

### 2.1 Overview

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           Barton Device Service                         │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌──────────────────┐    ┌──────────────────┐    ┌──────────────────┐   │
│  │  SBMD Spec File  │    │   SbmdParser     │    │   SbmdSpec       │   │
│  │  (YAML .sbmd)    │───▶│                  │───▶│   (C++ structs)  │   │
│  └──────────────────┘    └──────────────────┘    └────────┬─────────┘   │
│                                                           │             │
│                                                           ▼             │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │                SpecBasedMatterDeviceDriver                       │   │
│  │  ┌─────────────────┐    ┌─────────────────┐                      │   │
│  │  │  MatterDevice   │    │  QuickJsScript  │                      │   │
│  │  │  (per device)   │◀──▶│  (JS runtime)   │                      │   │
│  │  └────────┬────────┘    └────────┬────────┘                      │   │
│  └───────────┼──────────────────────┼───────────────────────────────┘   │
│              │                      │                                   │
│              ▼                      ▼                                   │
│  ┌──────────────────┐    ┌──────────────────────────────────────────┐   │
│  │ DeviceDataCache  │    │  JavaScript Mapper Scripts               │   │
│  │ (attribute cache)│    │  - Read: Matter TLV → Barton string      │   │
│  └──────────────────┘    │  - Write: Barton string → Matter TLV     │   │
│                          │  - Execute: Barton args → Command TLV    │   │
│                          │  - Execute Response: Response TLV →      │   │
│                          │                       Barton string      │   │
│                          └──────────────────────────────────────────┘   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
                                      │
                                      ▼
                          ┌──────────────────┐
                          │   Matter Device  │
                          │   (over fabric)  │
                          └──────────────────┘
```

### 2.2 Key Components

| Component | Description |
|-----------|-------------|
| **SbmdSpec** | C++ data structures representing a parsed SBMD specification |
| **SbmdParser** | YAML parser that converts `.sbmd` files into `SbmdSpec` objects |
| **SbmdFactory** | Auto-registers SBMD drivers from the specs directory at startup |
| **SpecBasedMatterDeviceDriver** | Device driver implementation that uses SBMD specs |
| **MatterDevice** | Per-device instance managing state, cache, and script execution |
| **QuickJsScript** | JavaScript runtime (QuickJS) for executing mapper scripts |
| **DeviceDataCache** | Cached attribute data kept up-to-date via Matter subscriptions |

### 2.3 Data Flow

1. **Startup**: `SbmdFactory` scans the specs directory and parses all `.sbmd` files
2. **Registration**: Each parsed spec creates a `SpecBasedMatterDeviceDriver` instance
3. **Device Addition**: When a Matter device is commissioned, it is matched to a driver by device type
4. **Resource Binding**: The driver binds Barton resources to Matter attributes/commands via mappers
5. **Runtime Operations**:
   - **Read**: Attribute data from cache/device → JavaScript script → Barton string
   - **Write**: Barton string → JavaScript script → TLV → Matter attribute write
   - **Execute**: Barton arguments → JavaScript script → TLV → Matter command

## 3. SBMD File Schema

SBMD specifications are YAML files with the `.sbmd` extension. The current schema
version is **1.0**, as specified in the `schemaVersion` field of each SBMD file.

> **JSON Schema**: A formal JSON Schema for validating SBMD files is available at
> [`core/deviceDrivers/matter/sbmd/sbmd-spec-schema.json`](../core/deviceDrivers/matter/sbmd/sbmd-spec-schema.json).
> All `.sbmd` files in the `specs/` directory are automatically validated against
> this schema during the build process.

### 3.1 Top-Level Structure

```yaml
schemaVersion: "1.0"          # SBMD schema version (required)
driverVersion: "1.0"          # Driver version (required)
name: "Driver Name"           # Human-readable name (required)
scriptType: "JavaScript"      # Script type (see below)
bartonMeta:                   # Barton-specific metadata (required)
  deviceClass: "doorLock"     # Barton device class
  deviceClassVersion: 3       # Device class version
matterMeta:                   # Matter-specific metadata (required)
  deviceTypes:                # List of supported Matter device type IDs
    - 0x000a
  revision: 1                 # Matter device type revision
  featureClusters: []         # Cluster IDs for featureMap access (optional)
reporting:                    # Subscription parameters (optional)
  minSecs: 1                  # Minimum reporting interval
  maxSecs: 3600               # Maximum reporting interval
resources: []                 # Top-level (device) resources (optional)
endpoints: []                 # Endpoint definitions (required)
```

### 3.2 Script Type

The `scriptType` field specifies the JavaScript runtime requirements for the driver:

| Value | Description |
|-------|-------------|
| `JavaScript` | Default. Scripts use only `SbmdUtils` helpers. |
| `JavaScript+matterjs` | Scripts require the `MatterClusters` global for TLV encoding. |

Drivers that use `JavaScript+matterjs` require the matter.js cluster bundle to be
built and available. If the bundle is not present, device initialization will fail.
See [SBMD matter.js Integration](SBMD_MATTERJS_INTEGRATION.md) for details on using
matter.js for TLV encoding.

### 3.3 Barton Metadata

```yaml
bartonMeta:
  deviceClass: "doorLock"     # Barton device class identifier
  deviceClassVersion: 3       # Version of the device class schema
```

### 3.4 Matter Metadata

The Matter metadata is used to determine which SBMD specification should be used
for a particular device. When a Matter device is commissioned, its device type is
matched against the `deviceTypes` list in each registered SBMD spec to find the
appropriate driver.

```yaml
matterMeta:
  deviceTypes:                # Matter device type IDs (hex or decimal)
    - 0x000a                  # Door Lock device type
    - 0x000b                  # Alternative device type
  revision: 1                 # Matter device type revision number from Matter Spec.
  featureClusters:            # Optional: cluster IDs whose FeatureMap to read
    - 0x0101                  # e.g., DoorLock cluster
```

The optional `featureClusters` list specifies which Matter cluster IDs the runtime
should read `FeatureMap` attributes for. At device initialization, the runtime reads
the FeatureMap attribute from each listed cluster and makes the values available to
scripts via the `clusterFeatureMaps` object (keyed by decimal cluster ID string).
If `featureClusters` is omitted, `clusterFeatureMaps` will be empty in all scripts.

### 3.5 Reporting Configuration

A single wildcarded attribute reporting configuration is maintained on the device.
These settings allow configuration on the min/max intervals.

```yaml
reporting:
  minSecs: 1                  # Minimum subscription reporting interval (seconds)
  maxSecs: 3600               # Maximum subscription reporting interval (seconds)
```

### 3.6 Endpoints

Endpoints in this context are Barton device data model concepts and should not be
confused with Matter endpoints. These represent logical groupings of resources
within Barton's device representation and do not necessarily map directly to
Matter endpoint IDs. The endpoint `id` is a Barton identifier, not a Matter
endpoint number.

```yaml
endpoints:
  - id: "1"                   # Barton endpoint identifier (string)
    profile: "doorLock"       # Barton profile name
    profileVersion: 3         # Profile version
    resources: []             # Resources on this endpoint
```

### 3.7 Resources

Resources define the Barton data model elements and their mapping to Matter:

```yaml
resources:
  - id: "locked"              # Resource identifier
    type: "boolean"           # Barton type (boolean, string, number, function, etc.)
    modes:                    # Access modes
      - "read"                # Resource is readable
      - "write"               # Resource is writable
      - "dynamic"             # Value can change asynchronously
      - "emitEvents"          # Changes emit events
    mapper:                   # Mapping configuration
      read: {...}             # Read mapper (optional)
      write: {...}            # Write mapper (optional)
      execute: {...}          # Execute mapper (optional, for function types)
```

#### Resource Modes

| Mode | Description |
|------|-------------|
| `read` | Resource value can be read |
| `write` | Resource value can be written |
| `execute` | Resource can be executed (for function types). Automatically set when an execute mapper is present. |
| `dynamic` | Value can change without direct write |
| `emitEvents` | Changes generate events to subscribers |
| `lazySaveNext` | Defer persistence to next save cycle |
| `sensitive` | Value contains sensitive data |

## 4. Mapper Configuration

Mappers define the transformation between Barton resources and Matter attributes,
commands, or events. Read mappers may specify an `attribute` or `command`, write and
execute mappers are script-only, and event mappers specify an `event`. All mapper
types include a JavaScript `script` for the transformation.

### 4.0 Conversion Overview

Mappers bridge two different data representations:

- **Barton side**: Resource values are represented as **strings**. All Barton resource
  reads return strings, writes accept strings, and function arguments/responses are strings.

- **Matter side**: Data is encoded as **TLV** (Tag-Length-Value) binary format for
  over-the-air communication with devices.

#### Read Operations

For read operations, the SBMD runtime retrieves attribute data from the device and
passes it to the script as base64-encoded TLV. The script decodes the TLV and
transforms it to a Barton string:

```
Read Flow:
  Matter Device → TLV → Base64 → Script (decode + transform) → Barton String
```

Scripts use `SbmdUtils.Tlv.decode(sbmdReadArgs.tlvBase64)` to decode the TLV data
into native JavaScript values.

#### Write and Execute Operations

For write and execute operations, scripts encode data as TLV and return it as
base64. The script returns a structured JSON object with `tlvBase64` containing
the encoded data:

```
Write/Execute Flow:
  Barton Input → Script (transform + encode) → tlvBase64 → Matter Device

Execute Response Flow:
  Matter Device → TLV → Base64 → Script (decode + transform) → Barton String
```

Write and execute mapper scripts return one of:
- `{write: {clusterId, attributeId, tlvBase64}}` - for attribute writes
- `{invoke: {clusterId, commandId, tlvBase64, ...}}` - for command invocations

Scripts use `SbmdUtils.Tlv.encode*()` helpers for TLV encoding. For complex
type-safe encoding, see the optional [matter.js integration](SBMD_MATTERJS_INTEGRATION.md).

### 4.1 Attribute Mapping

#### Read Mapper with Attribute

Maps a Matter attribute read to a Barton resource value:

```yaml
mapper:
  read:
    attribute:
      clusterId: "0x0101"     # Matter cluster ID (hex or decimal)
      attributeId: "0x0000"   # Matter attribute ID
      name: "LockState"       # Attribute name (for documentation)
      type: "enum8"           # Matter data type
    script: |
      var lockState = SbmdUtils.Tlv.decode(sbmdReadArgs.tlvBase64);
      return {output: lockState === 1 ? 'true' : 'false'};
```

#### Write Mapper

Maps a Barton resource write to a Matter operation. Write mappers are script-only and
must return the full operation details. The script can return either a `write` operation
(for attribute writes) or an `invoke` operation (for command-based writes):

```yaml
mapper:
  write:
    script: |
      // Encode the value as TLV and return a write operation
      const secs = parseInt(sbmdWriteArgs.input, 10);
      const tlvBase64 = SbmdUtils.Tlv.encode(secs, 'uint16');
      return SbmdUtils.Response.write(0x0003, 0x0000, tlvBase64);
```

Or invoke a command:

```yaml
mapper:
  write:
    script: |
      // Write to On/Off resource invokes On or Off command
      const isOn = sbmdWriteArgs.input === 'true';
      return SbmdUtils.Response.invoke(0x0006, isOn ? 0x0001 : 0x0000);
```

### 4.2 Command Mapping

#### Execute Mapper

Maps a Barton function execution to a Matter command. Execute mappers are script-only
and must return an `invoke` operation with full command details:

```yaml
mapper:
  execute:
    script: |
      // Build PINCode bytes if credential service is supported
      var args = { PINCode: null };
      const featureMap = sbmdCommandArgs.clusterFeatureMaps['257'] || 0;
      if (((featureMap & 0x81) === 0x81) &&
          sbmdCommandArgs.input.length > 0) {
        var pinBytes = [];
        for (let i = 0; i < sbmdCommandArgs.input.length; i++) {
          pinBytes.push(sbmdCommandArgs.input.charCodeAt(i));
        }
        args.PINCode = pinBytes;
      }
      const tlvBase64 = SbmdUtils.Tlv.encodeStruct(
          args, {PINCode: {tag: 0, type: 'octstr'}});
      return SbmdUtils.Response.invoke(0x0101, 0x0000, tlvBase64,
          {timedInvokeTimeoutMs: 10000});
```

#### Execute Response Mapper (scriptResponse)

Some Matter commands return response data. The optional `scriptResponse` field defines
a script that converts the command response TLV (provided as JSON) back to a Barton
string that can be returned to the caller:

```yaml
mapper:
  execute:
    script: |
      // Encode user index as TLV and invoke GetUser
      const userIndex = parseInt(sbmdCommandArgs.input, 10);
      const tlvBase64 = SbmdUtils.Tlv.encodeStruct(
          {userIndex: userIndex}, {userIndex: {tag: 0, type: 'uint16'}});
      return SbmdUtils.Response.invoke(0x0101, 0x0003, tlvBase64);
    scriptResponse: |
      // Decode GetUserResponse TLV and return userName
      var user = SbmdUtils.Tlv.decode(sbmdCommandResponseArgs.tlvBase64);
      if (user.userName) {
        return {output: user.userName};
      }
      return {output: ""};
```

The `scriptResponse` receives the command response in `sbmdCommandResponseArgs.tlvBase64`
which the script decodes using `SbmdUtils.Tlv.decode()` before returning a Barton string.

### 4.3 Event Mapping

#### Event Mapper

Maps a Matter device event to a Barton resource value update. Event mappers bind to a
specific Matter cluster event and execute a script to transform the event TLV data into
a Barton resource value:

```yaml
mapper:
  event:
    event:
      clusterId: "0x0101"      # Door Lock cluster
      eventId: "0x0002"        # LockOperation event
      name: "LockOperation"
    script: |
      // Decode event TLV struct — lockOperationType is at tag 0
      var eventData = SbmdUtils.Tlv.decode(sbmdEventArgs.tlvBase64);
      // LockOperationType: 0=Lock, 1=Unlock, 2=NonAccessUserEvent, ...
      var isLocked = (eventData.lockOperationType === 0);
      return { output: isLocked ? 'true' : 'false' };
```

Event mappers receive `sbmdEventArgs` containing the base64-encoded TLV event data.
The script decodes the data and returns a Barton resource value.

### 4.4 Combined Mappers

A single resource can have multiple mappers for different operations:

```yaml
mapper:
  read:
    attribute:
      clusterId: "0x0003"
      attributeId: "0x0000"
      name: "IdentifyTime"
      type: "uint16"
    script: |
      var secs = SbmdUtils.Tlv.decode(sbmdReadArgs.tlvBase64);
      return {output: secs.toString()};
  write:
    script: |
      const secs = parseInt(sbmdWriteArgs.input, 10);
      const tlvBase64 = SbmdUtils.Tlv.encode(secs, 'uint16');
      return {write: {clusterId: 0x0003, attributeId: 0x0000, tlvBase64: tlvBase64}};
```

> **Note:** Read mappers require `attribute:` or `command:` metadata to specify the
> Matter operation since it is performed first in order to provide data to the script.
> Write and execute mappers are script-only and return full operation details directly.

## 5. JavaScript Script Interfaces

Scripts are executed in a QuickJS JavaScript runtime. Each mapper type provides a
specific input object and expects a specific output format.

> **TypeScript Definitions**: A formal schema for all script interfaces is available in
> [`core/deviceDrivers/matter/sbmd/script/sbmd-script.d.ts`](../core/deviceDrivers/matter/sbmd/script/sbmd-script.d.ts).
> This file can be used for IDE autocompletion and type checking during script development.

### 5.1 Read Mapper Script Interface

#### Input Object: `sbmdReadArgs`

```javascript
sbmdReadArgs = {
    tlvBase64: "...",          // Base64-encoded TLV data from Matter attribute
    deviceUuid: "uuid-string", // Device UUID
    clusterId: 0x0006,          // Cluster ID (number)
    clusterFeatureMaps: {"6": 0}, // Feature maps keyed by cluster ID string (decimal)
    endpointId: "1",           // Endpoint ID (string, may be empty for device resources)
    attributeId: 0x0000,       // Attribute ID (number)
    attributeName: "OnOff",    // Attribute name from spec
    attributeType: "bool"      // Attribute type from spec
}
```

#### Expected Output

```javascript
return {
    output: <barton_value>    // String value for the Barton resource
};
```

#### Examples

**Boolean passthrough:**
```javascript
// Decode TLV boolean and return as string
var val = SbmdUtils.Tlv.decode(sbmdReadArgs.tlvBase64);
return {output: val ? 'true' : 'false'};
```

**Enum to boolean conversion (Door Lock state):**
```javascript
// LockState enum: 0=NotFullyLocked, 1=Locked, 2=Unlocked, 3=Unlatched
var lockState = SbmdUtils.Tlv.decode(sbmdReadArgs.tlvBase64);
return {output: lockState === 1 ? 'true' : 'false'};
```

**Percentage conversion (Level Control):**
```javascript
// Decode level (0-254) and convert to percentage string
var level = SbmdUtils.Tlv.decode(sbmdReadArgs.tlvBase64);
var percent = Math.round(level / 254 * 100);
return {output: percent.toString()};
```

### 5.2 Write Mapper Script Interface

Write mappers are script-only—the script determines the complete Matter operation
to perform and returns it as a structured JSON object.

#### Input Object: `sbmdWriteArgs`

```javascript
sbmdWriteArgs = {
    input: "value",            // Barton string value to write
    deviceUuid: "uuid-string", // Device UUID
    clusterFeatureMaps: {"6": 0}, // Feature maps keyed by cluster ID string (decimal)
    endpointId: "1",           // Endpoint ID (string)
    resourceId: "res-id"       // Barton resource ID
}
```

#### Expected Output

The script must return one of two operation types:

**For attribute writes:**
```javascript
return {
    write: {
        clusterId: <number>,    // Matter cluster ID
        attributeId: <number>,  // Matter attribute ID
        tlvBase64: <string>     // Base64-encoded TLV value
    }
};
```

**For command invocations:**
```javascript
return {
    invoke: {
        clusterId: <number>,           // Matter cluster ID
        commandId: <number>,           // Matter command ID
        tlvBase64: <string>,           // Base64-encoded TLV arguments (or "" for no args)
        timedInvokeTimeoutMs?: <number> // Optional timed invoke timeout
    }
};
```

#### Examples

**Attribute write - integer value:**
```javascript
// Input: sbmdWriteArgs.input = "30" (seconds)
const secs = parseInt(sbmdWriteArgs.input, 10);
const tlvBase64 = SbmdUtils.Tlv.encode(secs, 'uint16');
return {
    write: {
        clusterId: 0x0003,  // Identify cluster
        attributeId: 0x0000, // IdentifyTime attribute
        tlvBase64: tlvBase64
    }
};
```

**Command invocation - On/Off:**
```javascript
// Input: sbmdWriteArgs.input = "true" or "false"
const isOn = sbmdWriteArgs.input === 'true';
return {
    invoke: {
        clusterId: 0x0006,  // OnOff cluster
        commandId: isOn ? 0x0001 : 0x0000,  // On=1, Off=0
        tlvBase64: ""       // No arguments
    }
};
```

**Command invocation - Level Control:**
```javascript
// Input: sbmdWriteArgs.input = "50" (50%)
var percent = parseInt(sbmdWriteArgs.input, 10);
var level = Math.round(percent / 100 * 254);

// Encode MoveToLevelWithOnOff command struct
var tlvBase64 = SbmdUtils.Tlv.encodeStruct(
    { level: level, transitionTime: 0, optionsMask: 0, optionsOverride: 0 },
    {
        level: { tag: 0, type: 'uint8' },
        transitionTime: { tag: 1, type: 'uint16' },
        optionsMask: { tag: 2, type: 'bitmap8' },
        optionsOverride: { tag: 3, type: 'bitmap8' }
    }
);
return {
    invoke: {
        clusterId: 0x0008,  // LevelControl cluster
        commandId: 0x0004,  // MoveToLevelWithOnOff
        tlvBase64: tlvBase64
    }
};
```

### 5.3 Execute Mapper Script Interface

Execute mappers are script-only—the script determines the complete Matter command
to invoke and returns it as a structured JSON object.

#### Input Object: `sbmdCommandArgs`

```javascript
sbmdCommandArgs = {
    input: "value",            // Barton argument string
    deviceUuid: "uuid-string", // Device UUID
    clusterFeatureMaps: {"257": 129}, // Feature maps keyed by cluster ID string (decimal)
    endpointId: "1",           // Endpoint ID (string)
    resourceId: "res-id"       // Barton resource ID
}
```

#### Expected Output

```javascript
return {
    invoke: {
        clusterId: <number>,           // Matter cluster ID
        commandId: <number>,           // Matter command ID
        tlvBase64: <string>,           // Base64-encoded TLV arguments (or "" for no args)
        timedInvokeTimeoutMs?: <number> // Optional timed invoke timeout
    }
};
```

#### Examples

**Simple command with no arguments:**
```javascript
// Toggle command
return {
    invoke: {
        clusterId: 0x0006,  // OnOff cluster
        commandId: 0x0002,  // Toggle
        tlvBase64: ""       // No arguments
    }
};
```

**Lock/Unlock with optional PIN and timed invoke:**
```javascript
var args = { PINCode: null };
// Check if COTA (0x80) and PIN (0x01) features are both enabled
const featureMap = sbmdCommandArgs.clusterFeatureMaps['257'] || 0;
if (((featureMap & 0x81) === 0x81) &&
    sbmdCommandArgs.input.length > 0) {
  // Convert PIN string to byte array
  var pinBytes = [];
  for (let i = 0; i < sbmdCommandArgs.input.length; i++) {
    pinBytes.push(sbmdCommandArgs.input.charCodeAt(i));
  }
  args.PINCode = pinBytes;
}
// Encode struct with PINCode field at tag 0
const tlvBase64 = SbmdUtils.Tlv.encodeStruct(args, {PINCode: {tag: 0, type: 'octstr'}});
return {
    invoke: {
        clusterId: 0x0101,  // DoorLock cluster
        commandId: 0x0000,  // LockDoor
        timedInvokeTimeoutMs: 10000,
        tlvBase64: tlvBase64
    }
};
```

### 5.4 Execute Response Mapper Script Interface

For commands that return data, an optional `scriptResponse` can process the response:

#### Input Object: `sbmdCommandResponseArgs`

```javascript
sbmdCommandResponseArgs = {
    tlvBase64: "...",          // Base64-encoded TLV response data
    deviceUuid: "uuid-string", // Device UUID
    clusterId: 0x0101,         // Cluster ID (number)
    clusterFeatureMaps: {"257": 129}, // Feature maps keyed by cluster ID string (decimal)
    endpointId: "1",           // Endpoint ID (string)
    commandId: 0x0000,         // Command ID (number)
    commandName: "LockDoor"    // Command name from spec
}
```

#### Expected Output

```javascript
return {
    output: <barton_response> // String response for Barton
};
```

### 5.5 Event Mapper Script Interface

Event mappers process Matter device events (e.g., DoorLock LockOperation) and produce
a Barton resource value.

#### Input Object: `sbmdEventArgs`

```javascript
sbmdEventArgs = {
    tlvBase64: "...",          // Base64-encoded TLV data from Matter event
    deviceUuid: "uuid-string", // Device UUID
    clusterId: 0x0101,         // Cluster ID (number)
    clusterFeatureMaps: {"257": 129}, // Feature maps keyed by cluster ID string (decimal)
    endpointId: "1",           // Endpoint ID (string)
    eventId: 0x0002,           // Event ID (number)
    eventName: "LockOperation" // Event name from spec
}
```

#### Expected Output

```javascript
return {
    output: <barton_value>    // String value for the Barton resource
};
```

#### Example

**DoorLock LockOperation event:**
```javascript
// Decode LockOperation event TLV struct to determine lock state
var eventData = SbmdUtils.Tlv.decode(sbmdEventArgs.tlvBase64);
// LockOperationType: 0=Lock, 1=Unlock, 2=NonAccessUserEvent, ...
var isLocked = (eventData.lockOperationType === 0);
return { output: isLocked ? 'true' : 'false' };
```

## 6. Matter Data Types

### 6.1 Supported SBMD Types

The following Matter data types are supported in read mapper attribute definitions:

| Category | Types |
|----------|-------|
| **Boolean** | `bool`, `boolean` |
| **Unsigned Integer** | `uint8`, `uint16`, `uint32`, `uint64` |
| **Signed Integer** | `int8`, `int16`, `int24`, `int32`, `int40`, `int48`, `int56`, `int64` |
| **Enum/Bitmap** | `enum8`, `enum16`, `bitmap8`, `bitmap16`, `bitmap32`, `bitmap64` |
| **Floating Point** | `single`, `float`, `double` |
| **String** | `string`, `char_string`, `long_char_string` |
| **Byte String** | `octstr`, `octet_string`, `long_octet_string` |
| **Derived Types** | `percent`, `percent100ths`, `epoch-s`, `epoch-us`, `posix-ms`, `elapsed-s`, `utc`, `systime-ms`, `systime-us`, `temperature`, `amperage-ma`, `voltage-mv`, `power-mw`, `energy-mwh` |
| **Network Types** | `ipadr`, `ipv4adr`, `ipv6adr`, `ipv6pre`, `hwadr`, `semtag` |
| **Matter Identifiers** | `fabric-idx`, `fabric-id`, `node-id`, `vendor-id`, `devtype-id`, `group-id`, `endpoint-no`, `cluster-id`, `attrib-id`, `event-id`, `command-id`, `action-id`, `trans-id`, `data-ver`, `entry-idx` |
| **Complex** | `struct`, `list`, `array`, `null` |

### 6.2 TLV Decoding for Read Operations

For read operations, the C++ runtime passes attribute data (or command responses) as
base64-encoded TLV. Scripts use `SbmdUtils.Tlv.decode()` to convert TLV to JavaScript:

```javascript
var value = SbmdUtils.Tlv.decode(sbmdReadArgs.tlvBase64);
```

The decoder automatically handles all TLV types and returns native JavaScript values:
- Booleans: `true`/`false`
- Numbers: JavaScript numbers (automatic integer/float handling)
- Strings: JavaScript strings
- Byte arrays: JavaScript arrays of integers (0-255)
- Structs: JavaScript objects
- Arrays/Lists: JavaScript arrays

The `type` field in the mapper's `attribute:` section is for documentation purposes.

### 6.3 TLV Encoding for Write and Execute Operations

For write and execute operations, scripts encode values as TLV and return base64-encoded
data. Two encoding approaches are available:

#### SbmdUtils.Tlv Encoding

The built-in `SbmdUtils.Tlv` helpers provide simple encoding for primitive and struct types:

```javascript
// Encode primitive values
var tlv = SbmdUtils.Tlv.encode(42, 'uint16');
var tlv = SbmdUtils.Tlv.encode(true, 'bool');

// Encode structs with field schema
var args = { PINCode: [0x31, 0x32, 0x33, 0x34] };
var tlv = SbmdUtils.Tlv.encodeStruct(args, {
    PINCode: {tag: 0, type: 'octstr'}
});
```

> **matter.js Encoding**: For complex type-safe encoding with schema validation,
> scripts can use the optional matter.js cluster library. This requires
> `scriptType: "JavaScript+matterjs"` in the spec file. See
> [SBMD matter.js Integration](SBMD_MATTERJS_INTEGRATION.md) for details.

## 7. Complete Examples

### 7.1 Door Lock Driver

```yaml
schemaVersion: "1.0"
driverVersion: "1.0"
name: "Door Lock"
scriptType: "JavaScript"
bartonMeta:
  deviceClass: "doorLock"
  deviceClassVersion: 3
matterMeta:
  deviceTypes:
    - 0x000a
  revision: 1
  featureClusters:
    - 0x0101  # DoorLock cluster - for featureMap access in scripts
reporting:
  minSecs: 1
  maxSecs: 3600
resources:
  - id: "identifySeconds"
    type: "com.icontrol.seconds"
    modes:
      - "read"
      - "write"
    mapper:
      read:
        attribute:
          clusterId: "0x0003"
          attributeId: "0x0000"
          name: "IdentifyTime"
          type: "uint16"
        script: |
          var secs = SbmdUtils.Tlv.decode(sbmdReadArgs.tlvBase64);
          return {output: secs.toString()};
      write:
        script: |
          var secs = parseInt(sbmdWriteArgs.input, 10) || 0;
          var tlvBase64 = SbmdUtils.Tlv.encode(secs, 'uint16');
          return SbmdUtils.Response.write(0x0003, 0x0000, tlvBase64);
endpoints:
  - id: "1"
    profile: "doorLock"
    profileVersion: 3
    resources:
      - id: "locked"
        type: "boolean"
        modes:
          - "read"
          - "dynamic"
          - "emitEvents"
        mapper:
          event:
            event:
              clusterId: "0x0101"
              eventId: "0x0002"
              name: "LockOperation"
            script: |
              // Decode LockOperation event struct
              var eventData = SbmdUtils.Tlv.decode(sbmdEventArgs.tlvBase64);
              // LockOperationType: 0=Lock, 1=Unlock, 2=NonAccessUserEvent, ...
              var isLocked = (eventData.lockOperationType === 0);
              return { output: isLocked ? 'true' : 'false' };
      - id: "lock"
        type: "function"
        mapper:
          execute:
            script: |
              // Check if COTA (0x80) and PIN (0x01) features are both enabled
              var args = { PINCode: null };
              var featureMap = sbmdCommandArgs.clusterFeatureMaps['257'] || 0;
              if (((featureMap & 0x81) === 0x81) &&
                  sbmdCommandArgs.input.length > 0) {
                var pinBytes = [];
                for (var i = 0; i < sbmdCommandArgs.input.length; i++) {
                  pinBytes.push(sbmdCommandArgs.input.charCodeAt(i));
                }
                args.PINCode = pinBytes;
              }
              var tlvBase64 = SbmdUtils.Tlv.encodeStruct(
                  args, {PINCode: {tag: 0, type: 'octstr'}});
              return SbmdUtils.Response.invoke(0x0101, 0x0000, tlvBase64,
                  {timedInvokeTimeoutMs: 10000});
      - id: "unlock"
        type: "function"
        mapper:
          execute:
            script: |
              var args = { PINCode: null };
              var featureMap = sbmdCommandArgs.clusterFeatureMaps['257'] || 0;
              if (((featureMap & 0x81) === 0x81) &&
                  sbmdCommandArgs.input.length > 0) {
                var pinBytes = [];
                for (var i = 0; i < sbmdCommandArgs.input.length; i++) {
                  pinBytes.push(sbmdCommandArgs.input.charCodeAt(i));
                }
                args.PINCode = pinBytes;
              }
              var tlvBase64 = SbmdUtils.Tlv.encodeStruct(
                  args, {PINCode: {tag: 0, type: 'octstr'}});
              return SbmdUtils.Response.invoke(0x0101, 0x0001, tlvBase64,
                  {timedInvokeTimeoutMs: 10000});
```

### 7.2 Water Leak Detector

```yaml
schemaVersion: "1.0"
driverVersion: "1.0"
name: "Water Leak Detector"
scriptType: "JavaScript"
bartonMeta:
  deviceClass: "sensor"
  deviceClassVersion: 1
matterMeta:
  deviceTypes:
    - 0x0043
  revision: 1
reporting:
  minSecs: 1
  maxSecs: 3600
endpoints:
  - id: "1"
    profile: "sensor"
    profileVersion: 2
    resources:
      - id: "faulted"
        type: "com.icontrol.boolean"
        modes:
          - "read"
          - "dynamic"
          - "emitEvents"
        mapper:
          read:
            attribute:
              clusterId: "0x0045"
              attributeId: "0x0000"
              name: "StateValue"
              type: "bool"
            script: |
              const value = SbmdUtils.Tlv.decode(sbmdReadArgs.tlvBase64);
              return {output: (value === true) ? 'true' : 'false'};
```

## 8. Authoring Guidelines

### 8.1 Creating a New SBMD File

1. **Identify the Matter device type** - Find the device type ID from the Matter specification
2. **Map to Barton device class** - Determine which Barton device class best fits
3. **Define endpoints and resources** - The endpoints and resources defined in the SBMD file
   **must conform to the data model defined by the Barton device class**. The device class
   specifies required endpoints, profiles, and resources that devices of that class must
   provide. Refer to the Barton device class documentation for the expected structure.
4. **Map resources** - For each Barton resource, identify the corresponding Matter attribute or command
5. **Write scripts** - Create transformation scripts for non-trivial mappings
6. **Test** - Validate with actual devices

### 8.2 Best Practices

1. **Use hex notation** for cluster/attribute/command IDs for consistency with Matter spec
2. **Document transformations** in comments within scripts
3. **Check feature maps** before using optional features
4. **Handle null/undefined** values gracefully in scripts
5. **Use meaningful names** for attributes and commands (documentation purposes)
6. **Set appropriate reporting intervals** based on device type (e.g., sensors may need faster reporting)

### 8.3 Common Patterns

**Identity passthrough (no transformation):**
```javascript
var val = SbmdUtils.Tlv.decode(sbmdReadArgs.tlvBase64);
return {output: val.toString()};
```

**Boolean enum conversion:**
```javascript
var val = SbmdUtils.Tlv.decode(sbmdReadArgs.tlvBase64);
return {output: val === <expected_value> ? 'true' : 'false'};
```

**Numeric scaling:**
```javascript
var val = SbmdUtils.Tlv.decode(sbmdReadArgs.tlvBase64);
var scaled = Math.round(val * <scale_factor>);
return {output: scaled.toString()};
```

**Feature-conditional logic:**
```javascript
// Requires the cluster to be listed in matterMeta.featureClusters
const featureMap = sbmdCommandArgs.clusterFeatureMaps['<clusterId>'] || 0;
if ((featureMap & <feature_bit>) !== 0) {
  // Feature is enabled
}
```

### 8.4 Debugging Tips

1. Script errors are logged via `icLog` - check logs for "QuickJsScript" tag
2. JSON input/output is logged at debug level
3. Use `console.log()` in scripts for additional debugging (outputs to log)
4. Validate YAML syntax before deployment
5. Test scripts with unit tests before integration

## 9. File Deployment

### 9.1 Specs Directory

SBMD specification files should be placed in:
```
core/deviceDrivers/matter/sbmd/specs/
```

Files must have the `.sbmd` extension.

### 9.2 Automatic Registration

At startup, `SbmdFactory` automatically:
1. Scans the specs directory
2. Parses each `.sbmd` file
3. Creates `SpecBasedMatterDeviceDriver` instances
4. Registers drivers with `MatterDriverFactory`

### 9.3 Runtime Loading

Future versions may support:
- Dynamic loading of new specs without restart
- Remote spec distribution
- Spec versioning and updates

### 9.4 matter.js Cluster Integration

SBMD supports integration with the [matter.js](https://github.com/matter-js/matter.js)
cluster library for type-safe TLV encoding and decoding. When available, SBMD scripts
can use the `MatterClusters` global object to access Matter cluster TLV schemas.

See [SBMD_MATTERJS_INTEGRATION.md](SBMD_MATTERJS_INTEGRATION.md) for detailed documentation.

## 10. Appendix

### 10.1 Matter Cluster Reference

Common clusters used in SBMD specs:

| Cluster | ID | Description |
|---------|------|-------------|
| Identify | 0x0003 | Device identification |
| On/Off | 0x0006 | Binary switch control |
| Level Control | 0x0008 | Dimmable control |
| Door Lock | 0x0101 | Lock control |
| Window Covering | 0x0102 | Shades/blinds control |
| Boolean State | 0x0045 | Binary sensor state |
| Occupancy Sensing | 0x0406 | Motion detection |

### 10.2 Error Handling

Scripts that fail will:
1. Log an error with details
2. Return failure to the calling operation
3. Not affect other operations or devices

Common error causes:
- Syntax errors in JavaScript
- Missing `output` field in return value
- Type mismatches in TLV conversion
- Undefined variables or properties
