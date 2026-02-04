# Specification-Based Matter Drivers (SBMD)

## Historical Overview of the Goal

SBMD is a method to bring the long desired goal of being able to dynamically update the supported
device types through dynamically downloadable specifications which are not required to be packaged
with a device's firmware.

The idea of device drivers as specifications started about 10 years ago (at time of this writing)
and related to how Zigbee drivers were authored and managed. Complexities related to tricky timing
on some proprietary messages for some devices caused enough problems that the idea was shelved.
Downloadable native shared libraries that could provide the desired flexibility and performance
were also considered, however managing potentially large collections of compiled code for many
architectures was deemed too complex. The specification-based approach briefly resurfaced when OCF
device support was added since the need to add custom native code for each supported device type was
once again seen as adding too much friction to the goal of virtually unlimited device support in our
product.

## SBMD at a High Level

The SBMD architecture provides a mechanism by which a complete Barton device driver is provided as
a text file which can, at a minimum, perform the mapping between Matter types and Barton resources.

Barton's Zigbee architecture was designed to be mostly stateless, with device state stored in core
icDevice structures within deviceService. Matter devices under SBMD require state to be maintained
mostly by the DeviceDataCache instances. These are backed by the Matter SDK's ClusterStateCache
which is automatically kept up-to-date for attributes and events through full-device wildcard
subscriptions. Any event or attribute a device supports that is 'reportable' will be considered
up-to-date with what is in the DeviceDataCache. Attributes that are not reportable will have to be
read over-the-air on demand. Commands obviously do not use the DeviceDataCache.

The full set of conceivable functionality that could be provided by an SBMD driver for a device is:

- Process startup state

  Load from cache, do other stuff...

- Read an attribute
  - In DeviceDataCache
  - Over-the-air
- Write an attribute
- Send a command
- Receive a command
- Handle an event
- Schedule a timer to do some work

# Dynamic/Scripted Behavior

During the initial SBMD design, the idea was that the native SBMD code would basically provide TLV
data to a small bit of JavaScript that would provide the mapping to our Barton resource string
value. (Native TLV data can be converted to and from JavaScript using the Matter SDK today). It
would also perform the reverse mapping from a Barton string to a TLV that the native SBMD
code would send over-the-air to the device on a write or a command execution. The goal of this
approach was to do as much work as possible in common native code with only the minimum translation
done in very small and focused JavaScript. Subsequently, another approach emerged through Matter.js.

# Matter.js (https://github.com/matter-js/matter.js/)

While at the 2025 CSA Member Meeting in Barcelona, a new project called Matter.js was announced
which is a complete implementation of the Matter specification in TypeScript/JavaScript. Within
this project are full cluster implementations that are directly generated from the Matter
specification which yields a complete and correct library which we could use that is too tempting
to ignore. Thomas met with the author/owner of the project and discussed its use in SBMD where the
idea was met with excitement and an eagerness to help.

Matter.js expects a Node.js environment for its full-functionality, however the author has
considered non-Node.js usage and stated that it is possible to provide polyfill for the dependent
functionality that is non-standard and missing from QuickJS (like UDP, etc.). We might consider
https://github.com/saghul/txiki.js as it appears to be QuickJS + libuv and shows active development.

There are several ways we could use Matter.js:

1. As a simple TLV transcoder. This would not require full Matter.js functionality and would fill
   the initial requirement to convert TLV data as provided by the native Matter SDK into concrete
   types that JavaScript could act upon and perform the conversion to/from Barton values and args.
1. As an extension of the native Matter SDK that can perform full over-the-air transactions with
   remote devices, including esablishing CASE sessions to send requests or possibly even receiving
   requests directly.
1. Something between the two above options.

## Using Matter.js

### Simple TLV Transcoding

This approach follows the [original minimal design](#design-of-the-original-minimal-javascript-approach)
and uses the Matter.js cluster library to parse TLV (provided by the native SBMD code) to JSON which
can then be used by small bits of JavaScript to return a string representation of a resource, or
the other way around. It should not require adding polyfill to Matter.js for native (non-JS)
support. Matter.js would be transpilated at build-time along with Barton's other depenendcies, and
only the cluster library would need to be packaged up. SBMD would load this cluster code into the
JavaScript runtime that hosts the "small" TLV translation code and evaluated as needed at runtime.

Some random notes from a discussion with the author, Ingo Fischer:

```
so on example of the content of a LevelControl moveToLevel request  ...
basically the tlv definition is here
https://github.com/matter-js/matter.js/blob/3ccb882bc5a1a8b385d73d0efb0e8a8f89b3fd91/packages/types/src/clusters/level-control.ts#L112

So you can include this via

import { TlvMoveToLevelRequest } from @matter/main/clusters/level-control

When you then have the binary Tlv as "uint8array" kind of you simply to

const data = TlvMoveToLevelRequest.decode(data);

result is a JSON with the data as like

{ level: 50, transitionTime: null, optionsMask: {...}, optionsOverride: {...} }

if you want to encode a JSON into TLV you just do

const binaryTlv = TlvMoveToLevelRequest.encode(jsonData);

For the JSON you also have the JavaScript interface "MoveToLevelRequest" that you can use in
TypeScript to validate your code when you build the json ...
```

### Full Matter.js Integration

This approach requires a much more capable JavaScript runtime environment to satisfy Matter.js'
dependencies, which was designed to run within Node.js. It is possible that a capable runtime
could be found in a project like https://github.com/saghul/txiki.js, however additional support
is likely needed beyond what is provided by default. Barton's native Matter SDK would still host
the fabrics, keys, and node details but according to Ingo, it should be possible to share with
Matter.js.

SBMDs built with full Matter.js support would likely look much different than the
[original minimal design](#design-of-the-original-minimal-javascript-approach), and would instead
likely be completely specified in JavaScript instead of YAML. Well defined entry points to the
specifications would be needed as well as JavaScript interfaces to call back into Barton and/or
the native Matter SDK.

# Design of the Original Minimal JavaScript Approach

## Resource Mapping

Devices in Barton are represented as a collection of resources. It is the job of the SBMD to map
resources to attributes, commands, and events in Matter. For this purpose, each resource has one
or more "mappers" which perform specific transformations between Matter and Barton's string value
of the resource (or argument in the case of executable resources).

### Mappers

#### Attribute Mapper

The Attribute Mapper is responsible for handling a resource read or write request, performing an
associated read or write of a mapped attribute on a Matter cluster, and transforming the attribute
values to and from string values compatible with the Barton device class or profile.

Cases to consider:
- resource and attribute have the same types (no transform other than to/from string)
- resource and attribute have the same types but different meanings (transform required)
- resource and attribute have different types (transform required, such as enum to bool)

##### Examples:

No transform, read and write:
```json
"resources": [
    {
        "id": "label",
        "type": "string",
        "modes": ["read", "write", "dynamic", "emitEvents"],
        "cachingPolicy": "always",
        "readWriteMapper": {
            "attribute": {
                "clusterId": "0x0028",
                "attributeId": "0x0005",
                "name": "NodeLabel",
                "type": "string"
            }
        }
    }
]
```

Transform enum to bool, read only:
```json
"resources": [
    {
        "id": "locked",
        "type": "boolean",
        "modes": ["read", "dynamic", "emitEvents"],
        "cachingPolicy": "always",
        "readMapper": {
            "attribute": {
                "clusterId": "0x0101",
                "attributeId": "0x0000",
                "name": "LockState",
                "type": "enum8"
            },
            "mapping": {
                "default": false,
                "1": true
            }
        }
    }
]
```

Transform values (percentage to uint8 range), read and write:
```json
"resources": [
    {
        "id": "currentLevel",
        "type": "percentage",
        "modes": ["read", "write"],
        "cachingPolicy": "always",
        "readWriteMapper": {
            "attribute": {
                "clusterId": "0x0008",
                "attributeId": "0x0000",
                "name": "CurrentLevel",
                "type": "uint8"
            },
            "mapping": {
                "default": false,
                "1": true
            }
        }
    }
]
```
### SBMD Parser

Schema-Based Matter Driver (SBMD) Parser for parsing YAML driver specifications.

#### Overview

The SBMD Parser reads YAML files that define Matter device drivers using a declarative schema. It parses the YAML into structured C++ objects that can be used to generate or configure device drivers at runtime.

#### Components

##### SbmdSpec.h
Defines the data structures representing an SBMD specification:
- `SbmdSpec` - Complete driver specification
- `SbmdEndpoint` - Device endpoint with profile and resources
- `SbmdResource` - Individual resource (property or function)
- `SbmdMapper` - Mapping between resources and Matter clusters/attributes/commands
- `SbmdAttribute` - Matter attribute definition
- `SbmdCommand` - Matter command definition
- `SbmdBartonMeta` - Barton-specific metadata
- `SbmdMatterMeta` - Matter-specific metadata

##### SbmdParser.h/cpp
Parser implementation with two main entry points:
- `ParseFile(const std::string &filePath)` - Parse from file path
- `ParseString(const std::string &yamlContent)` - Parse from string

#### Usage Example

```cpp
#include "SbmdParser.h"

// Parse an SBMD file
auto spec = barton::SbmdParser::ParseFile("door-lock-sbmd.yaml");

if (spec)
{
    std::cout << "Driver: " << spec->name << std::endl;
    std::cout << "Device Type: 0x" << std::hex << spec->matterMeta.deviceType << std::endl;

    // Iterate through endpoints
    for (const auto &endpoint : spec->endpoints)
    {
        std::cout << "Endpoint " << endpoint.id
                  << ": " << endpoint.profile << std::endl;

        // Access resources
        for (const auto &resource : endpoint.resources)
        {
            std::cout << "  Resource: " << resource.id
                      << " (" << resource.type << ")" << std::endl;
        }
    }
}
```

#### YAML Schema

##### Top Level
```yaml
schemaVersion: "1.0"        # SBMD schema version
driverVersion: "1.0"        # Driver version
name: "Driver Name"         # Human-readable driver name
bartonMeta: {...}           # Barton-specific metadata
matterMeta: {...}           # Matter-specific metadata
resources: []               # Top-level resources (optional)
endpoints: []               # Endpoint definitions
```

##### Barton Metadata
```yaml
bartonMeta:
  deviceClass: "doorLock"         # Barton device class
  deviceClassVersion: 2           # Device class version
```

##### Matter Metadata
```yaml
matterMeta:
  deviceType: "0x000a"      # Matter device type (hex or decimal)
  revision: 3               # Matter device revision
```

##### Endpoints
```yaml
endpoints:
  - id: 1                   # Endpoint ID
    profile: "doorLock"     # Profile name
    profileVersion: 3       # Profile version
    resources: []           # Resource definitions
```

##### Resources
```yaml
resources:
  - id: "locked"            # Resource ID
    type: "boolean"         # Type: boolean, string, number, function
    modes:                  # Access modes
      - "read"
      - "dynamic"
      - "emitEvents"
    cachingPolicy: "always" # Caching policy
    mapper: {...}           # Mapper definition
```

##### Mapper
```yaml
mapper:
  read:                     # Read mapping (optional)
    attribute:
      clusterId: "0x0101"   # Matter cluster ID
      attributeId: "0x0000" # Matter attribute ID
      name: "LockState"     # Attribute name
      type: "enum8"         # Data type
    script: |               # Optional transformation script
      return LockState === 1;

  write:                    # Write mapping (optional)
    command:
      clusterId: "0x0006"
      commandId: "0x00"
      name: "Off"
    script: |               # Optional transformation script
      ...

  execute:                  # Execute mapping (optional)
    command:
      clusterId: "0x0101"
      commandId: "0x01"
      name: "UnlockDoor"
      args:                 # Command arguments
        - name: "PINCode"
          type: "octstr"
    script: |               # Argument transformation
      ...
```

#### Building

The parser is automatically included in the build via the glob pattern in `core/CMakeLists.txt`:
```cmake
file(GLOB sbmdSrc "deviceDrivers/matter/sbmd/*.cpp")
```

##### Dependencies
- yaml-cpp (for YAML parsing)
- icLog (for logging)

#### Testing

A simple example program is provided in `SbmdParserExample.cpp`:

```bash
# Build the project (example binary would need to be added to CMakeLists.txt)
cmake --build build

# Run the example
./sbmd_parser_example door-lock-sbmd.yaml
```

#### Features

- **Hex/Decimal Parsing**: Automatically handles hex strings (0x prefix) and decimal numbers
- **Error Handling**: Comprehensive error checking with logging
- **Type Safety**: Strong typing via C++ structs
- **Extensible**: Easy to add new fields or sections
- **Standards Compliant**: Uses yaml-cpp standard library

#### Error Handling

The parser returns `nullptr` on error and logs detailed error messages via icLog:
- YAML syntax errors
- Invalid structure
- Missing required fields
- Type mismatches

Always check the return value:
```cpp
auto spec = SbmdParser::ParseFile("file.yaml");
if (!spec) {
    // Handle error
}
```
