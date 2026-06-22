## MODIFIED Requirements

### Requirement: SBMD factory loads driver files
The SBMD factory SHALL scan configured directories for `.sbmd.js` files (instead of `.sbmd` YAML files). For each file, the factory SHALL evaluate it in the mquickjs context, extract metadata to C++ structures, and register the driver with `MatterDriverFactory`. The factory SHALL no longer use `SbmdParser` or yaml-cpp for driver loading.

#### Scenario: Factory loads .sbmd.js files
- **WHEN** the SBMD factory scans the specs directory at startup
- **THEN** it finds and loads all files with the `.sbmd.js` extension

#### Scenario: Factory ignores .sbmd files
- **WHEN** the specs directory contains both `.sbmd` and `.sbmd.js` files
- **THEN** only `.sbmd.js` files are loaded

#### Scenario: Invalid .sbmd.js file rejected
- **WHEN** a `.sbmd.js` file contains a JavaScript syntax error
- **THEN** the factory logs an error and continues loading other files

### Requirement: Driver claiming uses C++ metadata
The driver claiming process (vendor-specific pass, then generic device-type pass) SHALL use C++ metadata extracted at load time. Claiming SHALL NOT require the driver to be activated (handler JSValues rooted).

#### Scenario: Inactive driver participates in claiming
- **WHEN** a new device is commissioned and matches an inactive driver's device types
- **THEN** the driver is identified as a candidate, activated, and claiming proceeds

### Requirement: SpecBasedMatterDeviceDriver supports v4 handler model
The `SpecBasedMatterDeviceDriver` SHALL dispatch Barton resource operations to v4 handler functions (seed, read, write, execute) and device-initiated messages to attribute/event/command handlers. It SHALL execute result chains returned by handlers.

#### Scenario: Resource read dispatches to read handler
- **WHEN** a Barton read operation is performed on a resource with a `read` handler
- **THEN** the driver resolves supplements, calls the handler, and returns the result value

#### Scenario: Attribute report dispatches to attribute handler
- **WHEN** a Matter attribute report arrives matching a registered `attributeHandler`
- **THEN** the driver calls the handler and executes the result chain (e.g., resource updates)
