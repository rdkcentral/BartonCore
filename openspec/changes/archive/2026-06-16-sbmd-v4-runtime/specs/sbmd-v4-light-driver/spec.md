## ADDED Requirements

### Requirement: Light driver as v4 JavaScript file
The light driver SHALL be implemented as a single `light.sbmd.js` file using the v4 `SbmdDriver({...})` registration format. It SHALL declare constants for all cluster, attribute, command, and resource IDs. It SHALL support the same device types as the v3 `light.sbmd` driver.

#### Scenario: Light driver loads successfully
- **WHEN** the SBMD factory scans the specs directory at startup
- **THEN** `light.sbmd.js` is evaluated, metadata is extracted, and the driver is registered for device types 0x0100, 0x010a, 0x0101, 0x010b, 0x0102, 0x0200, 0x010d, 0x0210, 0x010c, 0x0220, 0x0103, 0x0104, 0x0105

### Requirement: Light on/off resource via attribute handler and write handler
The `isOn` resource on endpoint "1" SHALL be readable, writable, dynamic, and emit events. An `attributeHandler` for the OnOff attribute SHALL update the resource when attribute reports arrive. A `seed` handler SHALL read the initial value from supplements. A `write` handler SHALL send the On (0x0001) or Off (0x0000) command on the OnOff cluster (0x0006).

#### Scenario: On/Off attribute report updates resource
- **WHEN** a Matter attribute report for cluster 0x0006, attribute 0x0000 arrives with value `true`
- **THEN** the `isOn` resource on endpoint "1" is updated to `"true"`

#### Scenario: Write true sends On command
- **WHEN** a Barton write operation sets `isOn` to `"true"`
- **THEN** the driver sends Matter command 0x0001 (On) on cluster 0x0006

#### Scenario: Write false sends Off command
- **WHEN** a Barton write operation sets `isOn` to `"false"`
- **THEN** the driver sends Matter command 0x0000 (Off) on cluster 0x0006

#### Scenario: Seed handler reads initial value
- **WHEN** the device is commissioned or the service restarts
- **THEN** the seed handler reads the OnOff attribute from supplements and sets the initial `isOn` value

### Requirement: Light current level resource (optional)
The `currentLevel` resource on endpoint "1" SHALL be optional (prerequisite: `currentLevel` alias). It SHALL map Matter level (0–254) to a percentage string (0–100). A `write` handler SHALL send the MoveToLevelWithOnOff command (0x0004) on the LevelControl cluster (0x0008).

#### Scenario: Level attribute report updates resource as percentage
- **WHEN** a Matter attribute report for cluster 0x0008, attribute 0x0000 arrives with value 127
- **THEN** the `currentLevel` resource is updated to `"50"`

#### Scenario: Write percentage sends MoveToLevel command
- **WHEN** a Barton write sets `currentLevel` to `"75"`
- **THEN** the driver sends MoveToLevelWithOnOff with level 191 (round(75/100*254)), transition time 0

#### Scenario: Resource skipped when cluster absent
- **WHEN** a commissioned device does not have the LevelControl cluster (0x0008)
- **THEN** the `currentLevel` resource is not created and no error occurs

### Requirement: Existing integration tests pass unchanged
All light integration tests (`testing/test/light_test.py`) SHALL pass against the v4 light driver without any modifications to the test code.

#### Scenario: Commission and verify resources
- **WHEN** `test_commission_light` runs against the v4 driver
- **THEN** the test passes with the same resource set as v3

#### Scenario: On/off toggle via sideband
- **WHEN** `test_light_on_off` runs against the v4 driver
- **THEN** the test passes — toggling the sideband device updates the Barton resource

#### Scenario: Attribute report for common clusters
- **WHEN** `test_light_common_cluster_attribute_report` runs against the v4 driver
- **THEN** the test passes — identifySeconds attribute reports are handled correctly
