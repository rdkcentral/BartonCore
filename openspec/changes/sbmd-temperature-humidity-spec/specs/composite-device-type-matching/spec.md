## ADDED Requirements

### Requirement: Device type match mode in SBMD specs
The `matterMeta` section of an SBMD spec SHALL support an optional `deviceTypeMatch` field with values `"any"` (default) or `"all"`. When omitted, the behavior SHALL default to `"any"`.

#### Scenario: Default match mode when field is omitted
- **WHEN** an SBMD spec does not include `matterMeta.deviceTypeMatch`
- **THEN** the parser SHALL set `deviceTypeMatch` to `"any"`

#### Scenario: Explicit "any" match mode
- **WHEN** an SBMD spec includes `matterMeta.deviceTypeMatch: "any"`
- **THEN** the parser SHALL set `deviceTypeMatch` to `"any"`

#### Scenario: Explicit "all" match mode
- **WHEN** an SBMD spec includes `matterMeta.deviceTypeMatch: "all"`
- **THEN** the parser SHALL set `deviceTypeMatch` to `"all"`

#### Scenario: Invalid match mode value
- **WHEN** an SBMD spec includes `matterMeta.deviceTypeMatch` with a value other than `"any"` or `"all"`
- **THEN** the parser SHALL reject the spec with an error

### Requirement: ANY-match claim semantics (default)
When `deviceTypeMatch` is `"any"`, the driver SHALL claim a device if ANY device type in `matterMeta.deviceTypes` matches ANY device type on ANY non-root endpoint of the device. This preserves the existing OR-match behavior.

#### Scenario: Single device type match on one endpoint
- **WHEN** a driver with `deviceTypeMatch: "any"` lists device type `0x0100` and the device has endpoint 1 with device type `0x0100`
- **THEN** `ClaimDevice` SHALL return true

#### Scenario: No matching device type
- **WHEN** a driver with `deviceTypeMatch: "any"` lists device type `0x0100` and no endpoint on the device has `0x0100`
- **THEN** `ClaimDevice` SHALL return false

#### Scenario: Root endpoint is excluded
- **WHEN** a device has device type `0x0100` only on endpoint 0 (root)
- **THEN** `ClaimDevice` SHALL return false regardless of match mode

### Requirement: ALL-match claim semantics
When `deviceTypeMatch` is `"all"`, the driver SHALL claim a device only if EVERY device type in `matterMeta.deviceTypes` is found across the device's non-root endpoints. Each device type must appear on at least one endpoint but they need not all be on the same endpoint.

#### Scenario: All device types present across endpoints
- **WHEN** a driver with `deviceTypeMatch: "all"` lists device types `[0x0302, 0x0307]` and the device has endpoint 1 with `0x0302` and endpoint 2 with `0x0307`
- **THEN** `ClaimDevice` SHALL return true

#### Scenario: Only some device types present
- **WHEN** a driver with `deviceTypeMatch: "all"` lists device types `[0x0302, 0x0307]` and the device has endpoint 1 with `0x0302` but no endpoint with `0x0307`
- **THEN** `ClaimDevice` SHALL return false

#### Scenario: All device types on the same endpoint
- **WHEN** a driver with `deviceTypeMatch: "all"` lists device types `[0x0302, 0x0307]` and the device has a single endpoint 1 with both `0x0302` and `0x0307`
- **THEN** `ClaimDevice` SHALL return true

#### Scenario: Existing drivers unaffected
- **WHEN** an existing SBMD spec (e.g., `light.sbmd`) does not declare `deviceTypeMatch`
- **THEN** the driver SHALL continue to use `"any"` (OR) semantics with no behavior change
