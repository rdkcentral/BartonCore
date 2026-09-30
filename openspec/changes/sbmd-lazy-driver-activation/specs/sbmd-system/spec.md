## MODIFIED Requirements

### Requirement: SBMD factory loads driver files
The SBMD factory SHALL scan configured directories for `.sbmd.js` files (instead of `.sbmd` YAML files). For each file, the factory SHALL evaluate it in the mquickjs context, extract claim metadata to C++ structures, and register the driver with `MatterDriverFactory` in the **inactive** state. The factory SHALL NOT activate drivers at startup; after extracting a driver's claim metadata the factory SHALL release the spec source text and heavy parsed registration so that a registered-but-unused driver retains only its claim stub. The factory SHALL no longer use `SbmdParser` or yaml-cpp for driver loading.

#### Scenario: Factory loads .sbmd.js files
- **WHEN** the SBMD factory scans the specs directory at startup
- **THEN** it finds and loads all files with the `.sbmd.js` extension

#### Scenario: Factory ignores .sbmd files
- **WHEN** the specs directory contains both `.sbmd` and `.sbmd.js` files
- **THEN** only `.sbmd.js` files are loaded

#### Scenario: Invalid .sbmd.js file rejected
- **WHEN** a `.sbmd.js` file contains a JavaScript syntax error
- **THEN** the factory logs an error and continues loading other files

#### Scenario: Drivers registered inactive at startup
- **WHEN** the SBMD factory finishes loading all `.sbmd.js` files at startup and no devices have been commissioned
- **THEN** every registered driver is in the inactive state, holding only its claim stub, and no handler JSValues are rooted

### Requirement: Driver claiming uses C++ metadata
The driver claiming process (vendor-specific pass, then generic device-type pass) SHALL use C++ metadata extracted at load time. Claiming SHALL NOT require the driver to be activated (handler JSValues rooted). A matching driver SHALL be activated when a device is bound to it, and deactivated when its last bound device is removed.

#### Scenario: Inactive driver participates in claiming
- **WHEN** a new device is commissioned and matches an inactive driver's device types
- **THEN** the driver is identified as a candidate, activated when the device is bound, and claiming proceeds

#### Scenario: Driver deactivated after last device removed
- **WHEN** the last device bound to a previously active driver is removed
- **THEN** the driver is deactivated and returns to the inactive claim-stub state, but continues to participate in future claiming using its metadata
