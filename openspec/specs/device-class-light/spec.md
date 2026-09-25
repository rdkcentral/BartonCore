# Device Class: Light

## Purpose

Specifies the Barton `light` device class: a device with a `light`-profile endpoint exposing on/off (`isOn`) and optional brightness (`currentLevel`) resources. It also specifies how an SBMD light driver maps the Matter On/Off (0x0006) and Level Control (0x0008) clusters onto those resources. Concrete light drivers are `.sbmd.js` files governed by the SBMD schema validator.

## Requirements

### Requirement: Light device type claiming
A device advertising a Matter light device type SHALL be claimed into the Barton `light` device class with at least one endpoint of profile `light`. The light class SHALL cover the Matter light device types 0x0100, 0x010a, 0x0101, 0x010b, 0x0102, 0x0200, 0x010d, 0x0210, 0x010c, 0x0220, 0x0103, 0x0104, and 0x0105.

#### Scenario: Light device is claimed
- **WHEN** a Matter device advertising device type 0x0100 (On/Off Light) is commissioned
- **THEN** Barton SHALL create a device with device class `light` and an endpoint with profile `light`

### Requirement: On/off resource (isOn)
The `isOn` resource on the light endpoint SHALL be readable, writable, dynamic, and emit events. It SHALL be backed by the Matter On/Off cluster (0x0006): attribute reports for the OnOff attribute (0x0000) SHALL update the resource, its initial value SHALL be seeded from device supplements, and writes SHALL send the On (0x0001) or Off (0x0000) command on cluster 0x0006.

#### Scenario: On/Off attribute report updates resource
- **WHEN** a Matter attribute report for cluster 0x0006, attribute 0x0000 arrives with value `true`
- **THEN** the `isOn` resource is updated to `"true"`

#### Scenario: Write true sends On command
- **WHEN** a Barton write operation sets `isOn` to `"true"`
- **THEN** the driver sends Matter command 0x0001 (On) on cluster 0x0006

#### Scenario: Write false sends Off command
- **WHEN** a Barton write operation sets `isOn` to `"false"`
- **THEN** the driver sends Matter command 0x0000 (Off) on cluster 0x0006

#### Scenario: Seed handler reads initial value
- **WHEN** the device is commissioned or the service restarts
- **THEN** the initial `isOn` value is seeded from the OnOff attribute in device supplements

### Requirement: Brightness resource (currentLevel, optional)
The `currentLevel` resource on the light endpoint SHALL be optional, present only when the device exposes the Level Control cluster (0x0008). It SHALL map the Matter level (0–254) to a percentage string (0–100), and writes SHALL send the MoveToLevelWithOnOff command (0x0004) on cluster 0x0008.

#### Scenario: Level attribute report updates resource as percentage
- **WHEN** a Matter attribute report for cluster 0x0008, attribute 0x0000 arrives with value 127
- **THEN** the `currentLevel` resource is updated to `"50"`

#### Scenario: Write percentage sends MoveToLevel command
- **WHEN** a Barton write sets `currentLevel` to `"75"`
- **THEN** the driver sends MoveToLevelWithOnOff with level 191 (round(75/100*254)), transition time 0

#### Scenario: Resource skipped when cluster absent
- **WHEN** a commissioned device does not have the Level Control cluster (0x0008)
- **THEN** the `currentLevel` resource is not created and no error occurs
