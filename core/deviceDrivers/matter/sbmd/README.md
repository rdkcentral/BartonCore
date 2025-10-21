# Specification-Based Matter Drivers (SBMD)

overview...

# Resource Mapping

Devices in Barton are represented as a collection of resources. It is the job of the SBMD to map
resources to attributes, commands, and events in Matter. For this purpose, each resource has one
or more "mappers" which perform specific transformations between Matter and Barton's string value
of the resource (or argument in the case of executable resources).

## Mappers

### Attribute Mapper

The Attribute Mapper is responsible for handling an resource read or write request, performing an
associated read or write of a mapped attribute on a Matter cluster, and transforming the attribute
values to and from string values compatible with the Barton device class or profile.

Cases to consider:
- resource and attribute have the same types (no transform other than to/from string)
- resource and attribute have the same types but different meanings (transform required)
- resource and attribute have different types (transform required, such as enum to bool)

#### Examples:

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
## SBMD Parser

Schema-Based Matter Driver (SBMD) Parser for parsing YAML driver specifications.

### Overview

The SBMD Parser reads YAML files that define Matter device drivers using a declarative schema. It parses the YAML into structured C++ objects that can be used to generate or configure device drivers at runtime.

### Components

#### SbmdSpec.h
Defines the data structures representing an SBMD specification:
- `SbmdSpec` - Complete driver specification
- `SbmdEndpoint` - Device endpoint with profile and resources
- `SbmdResource` - Individual resource (property or function)
- `SbmdMapper` - Mapping between resources and Matter clusters/attributes/commands
- `SbmdAttribute` - Matter attribute definition
- `SbmdCommand` - Matter command definition
- `SbmdBartonMeta` - Barton-specific metadata
- `SbmdMatterMeta` - Matter-specific metadata

#### SbmdParser.h/cpp
Parser implementation with two main entry points:
- `ParseFile(const std::string &filePath)` - Parse from file path
- `ParseString(const std::string &yamlContent)` - Parse from string

### Usage Example

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

### YAML Schema

#### Top Level
```yaml
schemaVersion: "1.0"        # SBMD schema version
driverVersion: "1.0"        # Driver version
name: "Driver Name"         # Human-readable driver name
bartonMeta: {...}           # Barton-specific metadata
matterMeta: {...}           # Matter-specific metadata
resources: []               # Top-level resources (optional)
endpoints: []               # Endpoint definitions
```

#### Barton Metadata
```yaml
bartonMeta:
  deviceClass: "doorLock"         # Barton device class
  deviceClassVersion: 2           # Device class version
```

#### Matter Metadata
```yaml
matterMeta:
  deviceType: "0x000a"      # Matter device type (hex or decimal)
  revision: 3               # Matter device revision
```

#### Endpoints
```yaml
endpoints:
  - id: 1                   # Endpoint ID
    profile: "doorLock"     # Profile name
    profileVersion: 3       # Profile version
    resources: []           # Resource definitions
```

#### Resources
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

#### Mapper
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
          featureMask: "& 0x81"
    script: |               # Argument transformation
      ...
```

### Building

The parser is automatically included in the build via the glob pattern in `core/CMakeLists.txt`:
```cmake
file(GLOB sbmdSrc "deviceDrivers/matter/sbmd/*.cpp")
```

#### Dependencies
- yaml-cpp (for YAML parsing)
- icLog (for logging)

### Testing

A simple example program is provided in `SbmdParserExample.cpp`:

```bash
# Build the project (example binary would need to be added to CMakeLists.txt)
cmake --build build

# Run the example
./sbmd_parser_example door-lock-sbmd.yaml
```

### Features

- **Hex/Decimal Parsing**: Automatically handles hex strings (0x prefix) and decimal numbers
- **Error Handling**: Comprehensive error checking with logging
- **Type Safety**: Strong typing via C++ structs
- **Extensible**: Easy to add new fields or sections
- **Standards Compliant**: Uses yaml-cpp standard library

### Error Handling

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
