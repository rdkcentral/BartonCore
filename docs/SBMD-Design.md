# Specification-Based Matter Drivers (SBMD) Design Document

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
│                           Barton Device Service                          │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌──────────────────┐    ┌──────────────────┐    ┌──────────────────┐   │
│  │  SBMD Spec File  │    │   SbmdParser     │    │   SbmdSpec       │   │
│  │  (YAML .sbmd)    │───▶│                  │───▶│   (C++ structs)  │   │
│  └──────────────────┘    └──────────────────┘    └────────┬─────────┘   │
│                                                           │              │
│                                                           ▼              │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │                SpecBasedMatterDeviceDriver                        │   │
│  │  ┌─────────────────┐    ┌─────────────────┐                       │   │
│  │  │  MatterDevice   │    │  QuickJsScript  │                       │   │
│  │  │  (per device)   │◀──▶│  (JS runtime)   │                       │   │
│  │  └────────┬────────┘    └────────┬────────┘                       │   │
│  └───────────┼──────────────────────┼───────────────────────────────┘   │
│              │                      │                                    │
│              ▼                      ▼                                    │
│  ┌──────────────────┐    ┌──────────────────────────────────────────┐   │
│  │ DeviceDataCache  │    │  JavaScript Mapper Scripts               │   │
│  │ (attribute cache)│    │  - Read: Matter TLV → Barton string      │   │
│  └──────────────────┘    │  - Write: Barton string → Matter TLV     │   │
│                          │  - Execute: Barton args → Command TLV    │   │
│                          │  - Execute Response: Response TLV →      │   │
│                          │                       Barton string      │   │
│                          └──────────────────────────────────────────┘   │
│                                                                          │
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

SBMD specifications are YAML files with the `.sbmd` extension.

### 3.1 Top-Level Structure

```yaml
schemaVersion: "1.0"          # SBMD schema version (required)
driverVersion: "1.0"          # Driver version (required)
name: "Driver Name"           # Human-readable name (required)
scriptType: "JavaScript"      # Script type (currently only "JavaScript")
bartonMeta:                   # Barton-specific metadata (required)
  deviceClass: "doorLock"     # Barton device class
  deviceClassVersion: 2       # Device class version
matterMeta:                   # Matter-specific metadata (required)
  deviceTypes:                # List of supported Matter device type IDs
    - 0x000a
  revision: 1                 # Matter device type revision
reporting:                    # Subscription parameters (optional)
  minSecs: 1                  # Minimum reporting interval
  maxSecs: 3600               # Maximum reporting interval
resources: []                 # Top-level (device) resources (optional)
endpoints: []                 # Endpoint definitions (required)
```

### 3.2 Barton Metadata

```yaml
bartonMeta:
  deviceClass: "doorLock"     # Barton device class identifier
  deviceClassVersion: 2       # Version of the device class schema
```

### 3.3 Matter Metadata

The Matter metadata is used to determine which SBMD specification should be used
for a particular device. When a Matter device is commissioned, its device type is
matched against the `deviceTypes` list in each registered SBMD spec to find the
appropriate driver.

```yaml
matterMeta:
  deviceTypes:                # Matter device type IDs (hex or decimal)
    - 0x000a                  # Door Lock device type
    - 0x000b                  # Alternative device type
  revision: 1                 # Matter device type revision number
```

### 3.4 Reporting Configuration

```yaml
reporting:
  minSecs: 1                  # Minimum subscription reporting interval (seconds)
  maxSecs: 3600               # Maximum subscription reporting interval (seconds)
```

### 3.5 Endpoints

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

### 3.6 Resources

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
| `execute` | Resource can be executed (for function types) |
| `dynamic` | Value can change without direct write |
| `emitEvents` | Changes generate events to subscribers |
| `lazySaveNext` | Defer persistence to next save cycle |
| `sensitive` | Value contains sensitive data |

## 4. Mapper Configuration

Mappers define the transformation between Barton resources and Matter attributes or
commands. Each mapper type (read, write, execute) must specify either an `attribute`
or a `command`, plus a JavaScript `script` for the transformation.

### 4.0 Conversion Overview

Mappers bridge two different data representations:

- **Barton side**: Resource values are represented as **strings**. All Barton resource
  reads return strings, writes accept strings, and function arguments/responses are strings.

- **Matter side**: Data is encoded as **TLV** (Tag-Length-Value) binary format for
  over-the-air communication with devices.

Since JavaScript cannot directly manipulate TLV binary data, the SBMD runtime converts
TLV to/from **JSON** as an intermediate format that scripts can work with:

```
Read Flow:
  Matter Device → TLV → [TlvToJson] → JSON → Script → Barton String

Write Flow:
  Barton String → Script → JSON → [JsonToTlv] → TLV → Matter Device

Execute Flow:
  Barton Args (strings) → Script → JSON → [JsonToTlv] → TLV → Matter Device

Execute Response Flow:
  Matter Device → TLV → [TlvToJson] → JSON → Script → Barton String
```

The Matter SDK's `TlvToJson` and `JsonToTlv` utilities handle the TLV↔JSON conversion
automatically. Script authors work exclusively with JSON values and Barton strings.

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
      return {output: sbmdReadArgs.input === 1};
```

#### Write Mapper with Attribute

Maps a Barton resource write to a Matter attribute:

```yaml
mapper:
  write:
    attribute:
      clusterId: "0x0003"
      attributeId: "0x0000"
      name: "IdentifyTime"
      type: "uint16"
    script: |
      const secs = parseInt(sbmdWriteArgs.input, 10);
      return {output: secs};
```

### 4.2 Command Mapping

#### Execute Mapper with Command

Maps a Barton function execution to a Matter command. The `script` converts Barton
string arguments into the JSON format needed for the command's TLV encoding:

```yaml
mapper:
  execute:
    command:
      clusterId: "0x0101"           # Matter cluster ID
      commandId: "0x00"             # Matter command ID
      name: "LockDoor"              # Command name
      timedInvokeTimeoutMs: 10000   # Timed invoke timeout (optional)
      args:                         # Command arguments
        - name: "PINCode"
          type: "octstr"
    script: |
      result = null;
      if (((sbmdCommandArgs.featureMap & 0x81) === 0x81) &&
          sbmdCommandArgs.input.length > 0) {
        result = [];
        for (let i = 0; i < sbmdCommandArgs.input[0].length; i++) {
          result.push(sbmdCommandArgs.input[0].charCodeAt(i));
        }
      }
      return {output: result};
```

#### Execute Response Mapper (scriptResponse)

Some Matter commands return response data. The optional `scriptResponse` field defines
a script that converts the command response TLV (provided as JSON) back to a Barton
string that can be returned to the caller:

```yaml
mapper:
  execute:
    command:
      clusterId: "0x0101"
      commandId: "0x03"             # GetUser command
      name: "GetUser"
      args:
        - name: "userIndex"
          type: "uint16"
    script: |
      // Convert Barton user index string to number
      return {output: parseInt(sbmdCommandArgs.input[0], 10)};
    scriptResponse: |
      // Convert GetUserResponse to Barton string
      var user = sbmdCommandResponseArgs.input;
      if (user.userName) {
        return {output: user.userName};
      }
      return {output: ""};
```

The `scriptResponse` receives the command response in `sbmdCommandResponseArgs.input`
(already converted from TLV to JSON) and must return a Barton string in the `output` field.
```

### 4.3 Combined Mappers

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
      return {output: sbmdReadArgs.input};
  write:
    attribute:
      clusterId: "0x0003"
      attributeId: "0x0000"
      name: "IdentifyTime"
      type: "uint16"
    script: |
      const secs = parseInt(sbmdWriteArgs.input, 10);
      return {output: secs};
```

## 5. JavaScript Script Interfaces

Scripts are executed in a QuickJS JavaScript runtime. Each mapper type provides a
specific input object and expects a specific output format.

### 5.1 Read Mapper Script Interface

#### Input Object: `sbmdReadArgs`

```javascript
sbmdReadArgs = {
    input: <value>,           // Attribute value (JSON-converted from TLV)
    deviceUuid: "uuid-string",// Device UUID
    clusterId: 6,             // Cluster ID (number)
    featureMap: 0,            // Cluster feature map (number)
    endpointId: "1",          // Endpoint ID (string, may be empty for device resources)
    attributeId: 0,           // Attribute ID (number)
    attributeName: "OnOff",   // Attribute name from spec
    attributeType: "bool"     // Attribute type from spec
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
// Input: sbmdReadArgs.input = true
return {output: sbmdReadArgs.input};
// Output: "true"
```

**Enum to boolean conversion (Door Lock state):**
```javascript
// LockState enum: 0=NotFullyLocked, 1=Locked, 2=Unlocked, 3=Unlatched
// Input: sbmdReadArgs.input = 1
return {output: sbmdReadArgs.input === 1};
// Output: "true" (locked)
```

**Percentage conversion (Level Control):**
```javascript
// Input: sbmdReadArgs.input = 127 (Matter level 0-254)
var level = sbmdReadArgs.input;
var percent = Math.round(level / 254 * 100);
return {output: percent.toString()};
// Output: "50"
```

### 5.2 Write Mapper Script Interface

#### Input Object: `sbmdWriteArgs`

```javascript
sbmdWriteArgs = {
    input: "value",           // Barton string value to write
    deviceUuid: "uuid-string",// Device UUID
    clusterId: 6,             // Cluster ID (number)
    featureMap: 0,            // Cluster feature map (number)
    endpointId: "1",          // Endpoint ID (string)
    attributeId: 0,           // Attribute ID (number)
    attributeName: "OnOff",   // Attribute name from spec
    attributeType: "bool"     // Attribute type from spec
}
```

#### Expected Output

```javascript
return {
    output: <matter_value>    // Value to write to Matter attribute (JSON format)
};
```

#### Examples

**Boolean string to boolean:**
```javascript
// Input: sbmdWriteArgs.input = "true"
return {output: sbmdWriteArgs.input === 'true'};
// Output: true (boolean for Matter)
```

**String to integer:**
```javascript
// Input: sbmdWriteArgs.input = "30"
const secs = parseInt(sbmdWriteArgs.input, 10);
return {output: secs};
// Output: 30 (integer for Matter)
```

**Percentage to level:**
```javascript
// Input: sbmdWriteArgs.input = "50" (50%)
var percent = parseInt(sbmdWriteArgs.input, 10);
var level = Math.round(percent / 100 * 254);
return {output: level};
// Output: 127 (Matter level)
```

### 5.3 Execute Mapper Script Interface

#### Input Object: `sbmdCommandArgs`

```javascript
sbmdCommandArgs = {
    input: ["arg1", "arg2"],  // Array of Barton argument strings
    deviceUuid: "uuid-string",// Device UUID
    clusterId: 257,           // Cluster ID (number)
    featureMap: 129,          // Cluster feature map (number)
    endpointId: "1",          // Endpoint ID (string)
    commandId: 0,             // Command ID (number)
    commandName: "LockDoor"   // Command name from spec
}
```

#### Expected Output

```javascript
return {
    output: <command_args>    // Command arguments (JSON format matching arg definitions)
};
```

#### Command Arguments Format

For commands with arguments, the output can be:

1. **Single value** - For commands with one argument:
   ```javascript
   return {output: 30};  // Single uint16 argument
   ```

2. **Array of bytes** - For octet string arguments:
   ```javascript
   // Convert string "1234" to byte array for PINCode
   result = [];
   for (let i = 0; i < sbmdCommandArgs.input[0].length; i++) {
     result.push(sbmdCommandArgs.input[0].charCodeAt(i));
   }
   return {output: result};
   ```

3. **null** - For optional arguments not being sent:
   ```javascript
   return {output: null};
   ```

4. **Object** - For commands with multiple named arguments:
   ```javascript
   return {output: {level: 127, transitionTime: 10}};
   ```

#### Examples

**Simple command with no arguments:**
```javascript
return {output: null};
```

**Lock/Unlock with optional PIN:**
```javascript
result = null;
// Check if COTA (0x80) and PIN (0x01) features are both enabled
if (((sbmdCommandArgs.featureMap & 0x81) === 0x81) &&
    sbmdCommandArgs.input.length > 0) {
  // Convert PIN string to byte array
  result = [];
  for (let i = 0; i < sbmdCommandArgs.input[0].length; i++) {
    result.push(sbmdCommandArgs.input[0].charCodeAt(i));
  }
}
return {output: result};
```

**Command with timeout argument:**
```javascript
// Input: sbmdCommandArgs.input = ["30"]
const secs = parseInt(sbmdCommandArgs.input[0], 10);
return {output: secs};
```

### 5.4 Execute Response Mapper Script Interface

For commands that return data, an optional `scriptResponse` can process the response:

#### Input Object: `sbmdCommandResponseArgs`

```javascript
sbmdCommandResponseArgs = {
    input: <response_value>,  // Command response (JSON-converted from TLV)
    deviceUuid: "uuid-string",// Device UUID
    clusterId: 257,           // Cluster ID (number)
    featureMap: 129,          // Cluster feature map (number)
    endpointId: "1",          // Endpoint ID (string)
    commandId: 0,             // Command ID (number)
    commandName: "LockDoor"   // Command name from spec
}
```

#### Expected Output

```javascript
return {
    output: <barton_response> // String response for Barton
};
```

## 6. Matter Data Types

### 6.1 Supported SBMD Types

The following Matter data types are supported in attribute and argument definitions:

| Category | Types |
|----------|-------|
| **Boolean** | `bool`, `boolean` |
| **Unsigned Integer** | `uint8`, `uint16`, `uint32`, `uint64` |
| **Signed Integer** | `int8`, `int16`, `int24`, `int32`, `int40`, `int48`, `int56`, `int64` |
| **Enum/Bitmap** | `enum8`, `enum16`, `bitmap8`, `bitmap16`, `bitmap32`, `bitmap64` |
| **Floating Point** | `single`, `float`, `double` |
| **String** | `string`, `char_string`, `long_char_string` |
| **Byte String** | `octstr`, `octet_string`, `long_octet_string` |
| **Derived Types** | `percent`, `percent100ths`, `epoch-s`, `epoch-us`, `posix-ms`, `temperature`, `amperage-ma`, `voltage-mv`, `power-mw`, `energy-mwh` |
| **Network Types** | `ipadr`, `ipv4adr`, `ipv6adr`, `ipv6pre`, `hwadr` |
| **Matter Identifiers** | `fabric-idx`, `fabric-id`, `node-id`, `vendor-id`, `devtype-id`, `group-id`, `endpoint-no`, `cluster-id`, `attrib-id`, `event-id`, `command-id` |
| **Complex** | `struct`, `list`, `array` |

### 6.2 Type Conversion

The QuickJS script engine automatically converts between Matter TLV and JSON:

- **TLV → JSON** (read operations): Matter SDK's `TlvToJson` converts attribute data
- **JSON → TLV** (write/execute operations): Matter SDK's `JsonToTlv` converts script output

Scripts work with native JavaScript types:
- Booleans: `true`/`false`
- Numbers: JavaScript numbers (automatic integer/float handling)
- Strings: JavaScript strings
- Byte arrays: JavaScript arrays of integers (0-255)
- Structs: JavaScript objects
- Arrays/Lists: JavaScript arrays

### 6.3 Type-Directed TLV Encoding

When converting script output (JSON) back to binary TLV for transmission to the device,
the **`type` field specified in the SBMD attribute or argument definition** determines
how the value is encoded. This is critical because JavaScript is loosely typed—a
JavaScript number could be encoded as various TLV types (uint8, int16, enum8, etc.).

For example, given this attribute definition:

```yaml
attribute:
  clusterId: "0x0008"
  attributeId: "0x0000"
  name: "CurrentLevel"
  type: "uint8"          # <-- This type is used for TLV encoding
```

If the script returns `{output: 127}`, the SBMD runtime:

1. Reads the `type` field from the attribute definition (`uint8`)
2. Maps `uint8` to the appropriate TLV type (`UINT`)
3. Encodes the JSON value `127` as an unsigned 8-bit integer in TLV format

The same applies to command arguments:

```yaml
command:
  clusterId: "0x0101"
  commandId: "0x00"
  name: "LockDoor"
  args:
    - name: "PINCode"
      type: "octstr"     # <-- This type directs TLV encoding for this argument
```

If the script returns `{output: [49, 50, 51, 52]}` (byte array), the runtime encodes
it as an octet string in the final TLV because the argument's `type` is `octstr`.

**Important**: The script's JSON output must be compatible with the declared type.
For example:
- `uint8`/`uint16`/etc.: Script should return a number
- `bool`: Script should return `true` or `false`
- `string`: Script should return a string
- `octstr`: Script should return an array of byte values (0-255)

## 7. Complete Examples

### 7.1 Door Lock Driver

```yaml
schemaVersion: "1.0"
driverVersion: "1.0"
name: "Door Lock"
scriptType: "JavaScript"
bartonMeta:
  deviceClass: "doorLock"
  deviceClassVersion: 2
matterMeta:
  deviceTypes:
    - 0x000a
  revision: 1
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
          return {output: sbmdReadArgs.input};
      write:
        attribute:
          clusterId: "0x0003"
          attributeId: "0x0000"
          name: "IdentifyTime"
          type: "uint16"
        script: |
          const secs = parseInt(sbmdWriteArgs.input, 10);
          return {output: secs};
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
          read:
            attribute:
              clusterId: "0x0101"
              attributeId: "0x0000"
              name: "LockState"
              type: "enum8"
            script: |
              return {output: sbmdReadArgs.input === 1};
      - id: "lock"
        type: "function"
        mapper:
          execute:
            command:
              clusterId: "0x0101"
              commandId: "0x00"
              name: "LockDoor"
              timedInvokeTimeoutMs: 10000
              args:
                - name: "PINCode"
                  type: "octstr"
            script: |
              result = null;
              if (((sbmdCommandArgs.featureMap & 0x81) === 0x81) &&
                  sbmdCommandArgs.input.length > 0) {
                result = [];
                for (let i = 0; i < sbmdCommandArgs.input[0].length; i++) {
                  result.push(sbmdCommandArgs.input[0].charCodeAt(i));
                }
              }
              return {output: result};
      - id: "unlock"
        type: "function"
        mapper:
          execute:
            command:
              clusterId: "0x0101"
              commandId: "0x01"
              name: "UnlockDoor"
              timedInvokeTimeoutMs: 10000
              args:
                - name: "PINCode"
                  type: "octstr"
            script: |
              result = null;
              if (((sbmdCommandArgs.featureMap & 0x81) === 0x81) &&
                  sbmdCommandArgs.input.length > 0) {
                result = [];
                for (let i = 0; i < sbmdCommandArgs.input[0].length; i++) {
                  result.push(sbmdCommandArgs.input[0].charCodeAt(i));
                }
              }
              return {output: result};
```

### 7.2 Water Leak Detector

```yaml
schemaVersion: "1.0"
driverVersion: "1.0"
name: "Water Leak Detector"
scriptType: "JavaScript"
bartonMeta:
  deviceClass: "sensor"
  deviceClassVersion: 2
matterMeta:
  deviceTypes:
    - 0x0043
  revision: 1
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
          return {output: sbmdReadArgs.input};
      write:
        attribute:
          clusterId: "0x0003"
          attributeId: "0x0000"
          name: "IdentifyTime"
          type: "uint16"
        script: |
          const secs = parseInt(sbmdWriteArgs.input, 10);
          return {output: secs};
endpoints:
  - id: "1"
    profile: "sensor"
    profileVersion: 3
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
              return {output: sbmdReadArgs.input};
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
return {output: sbmdReadArgs.input};
```

**Boolean enum conversion:**
```javascript
return {output: sbmdReadArgs.input === <expected_value>};
```

**Numeric scaling:**
```javascript
var scaled = Math.round(sbmdReadArgs.input * <scale_factor>);
return {output: scaled.toString()};
```

**Feature-conditional logic:**
```javascript
if ((sbmdCommandArgs.featureMap & <feature_bit>) !== 0) {
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
