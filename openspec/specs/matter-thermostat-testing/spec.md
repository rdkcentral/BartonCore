## ADDED Requirements

### Requirement: matter.js virtual thermostat device
A matter.js virtual thermostat device (`ThermostatDevice.js`) SHALL be created extending `VirtualDevice` with:
- Matter Thermostat device type (0x0301) on endpoint 1
- Thermostat cluster (0x0201) with initial state: LocalTemperature=2100 (21.00°C), OccupiedHeatingSetpoint=2000, OccupiedCoolingSetpoint=2600, SystemMode=Off
- Side-band operations: `setTemperature`, `setSystemMode`, `getState`

#### Scenario: Virtual thermostat starts with default state
- **WHEN** the virtual thermostat device is started
- **THEN** it SHALL be a Matter Thermostat device type (0x0301) with LocalTemperature=2100, OccupiedHeatingSetpoint=2000, OccupiedCoolingSetpoint=2600, SystemMode=Off

#### Scenario: Side-band set temperature
- **WHEN** the `setTemperature` side-band operation is called with a temperature value
- **THEN** the device SHALL update its LocalTemperature attribute to the specified value and the change SHALL be reported to subscribed controllers

#### Scenario: Side-band set system mode
- **WHEN** the `setSystemMode` side-band operation is called with a mode value (e.g., "heat")
- **THEN** the device SHALL update its SystemMode attribute accordingly

#### Scenario: Side-band get state
- **WHEN** the `getState` side-band operation is called
- **THEN** it SHALL return the current thermostat state including localTemperature, heatingSetpoint, coolingSetpoint, and systemMode

### Requirement: matter.js virtual thermostat with fan control device
A matter.js virtual thermostat with fan control device (`ThermostatWithFanDevice.js`) SHALL be created by subclassing `ThermostatDevice` to add:
- Fan Control cluster (0x0202) on the same thermostat endpoint, with initial FanMode=Auto
- Additional side-band operations: `setFanMode`, `getFanState`

The subclass SHALL reuse the base thermostat endpoint setup (Thermostat cluster, initial state, core side-band operations) and only add the Fan Control cluster and fan-related side-band operations.

#### Scenario: Virtual thermostat with fan starts with fan control cluster
- **WHEN** the virtual thermostat with fan device is started
- **THEN** it SHALL include both the Thermostat cluster (0x0201) and Fan Control cluster (0x0202) on the thermostat endpoint

#### Scenario: Side-band set fan mode
- **WHEN** the `setFanMode` side-band operation is called with a fan mode value
- **THEN** the device SHALL update its FanMode attribute on the Fan Control cluster

#### Scenario: Side-band get fan state
- **WHEN** the `getFanState` side-band operation is called
- **THEN** it SHALL return the current fan control state including fanMode

### Requirement: Python test fixture for Matter thermostat
A Python test fixture (`matter_thermostat.py`) SHALL be created as a `MatterDevice` subclass with:
- `device_class` set to `"thermostat"`
- `matterjs_entry_point` set to `"ThermostatDevice.js"`
- A pytest fixture function `matter_thermostat` that creates, starts, yields, and cleans up the device

#### Scenario: Fixture creates and starts thermostat
- **WHEN** the `matter_thermostat` pytest fixture is used in a test
- **THEN** it SHALL yield a started `MatterThermostat` instance ready for commissioning

### Requirement: Python test fixture for Matter thermostat with fan control
A Python test fixture (`matter_thermostat_with_fan.py`) SHALL be created by subclassing `MatterThermostat` to avoid duplicating common setup:
- `matterjs_entry_point` set to `"ThermostatWithFanDevice.js"`
- A pytest fixture function `matter_thermostat_with_fan` that creates, starts, yields, and cleans up the device

#### Scenario: Fixture creates and starts thermostat with fan
- **WHEN** the `matter_thermostat_with_fan` pytest fixture is used in a test
- **THEN** it SHALL yield a started `MatterThermostatWithFan` instance ready for commissioning, with Fan Control cluster support

### Requirement: Commission test
An integration test SHALL verify that a Matter thermostat can be commissioned and appears as a `thermostat` device class with the expected common resources.

#### Scenario: Commission thermostat
- **WHEN** a virtual Matter thermostat is commissioned via Barton
- **THEN** the device SHALL appear with deviceClass `thermostat` and have common resources (firmwareVersionString, macAddress, networkType, serialNumber)

### Requirement: Setpoint read/write test
Integration tests SHALL verify that heating and cooling setpoints can be read after commissioning and written via the Barton resource API.

#### Scenario: Write heating setpoint via Barton
- **WHEN** a client writes a new value to the `heatSetpoint` resource
- **THEN** the virtual thermostat device SHALL reflect the new heating setpoint value via side-band query

#### Scenario: Write cooling setpoint via Barton
- **WHEN** a client writes a new value to the `coolSetpoint` resource
- **THEN** the virtual thermostat device SHALL reflect the new cooling setpoint value via side-band query

### Requirement: System mode read/write test
An integration test SHALL verify that the system mode can be read and changed via the Barton resource API.

#### Scenario: Write system mode via Barton
- **WHEN** a client writes `"heat"` to the `systemMode` resource
- **THEN** the virtual thermostat device SHALL report its system mode as "heat" via side-band query

### Requirement: Side-band temperature change triggers Barton update
An integration test SHALL verify that a temperature change initiated via the side-band interface triggers a resource update in Barton.

#### Scenario: Side-band temperature change
- **WHEN** the virtual thermostat's local temperature is changed via the side-band interface
- **THEN** Barton SHALL receive a resource update for the `localTemperature` resource with the new value

### Requirement: Base thermostat tests verify no fan resources
The base `thermostat_test.py` tests SHALL verify that `fanMode` and `fanOn` resources are NOT present on a thermostat without the Fan Control cluster, confirming the prerequisite gate works correctly.

#### Scenario: No fan resources on base thermostat
- **WHEN** a virtual Matter thermostat without Fan Control cluster is commissioned
- **THEN** the `fanMode` and `fanOn` resources SHALL NOT be present on the thermostat endpoint

### Requirement: Commission thermostat with fan control test
An integration test in `thermostat_with_fan_test.py` SHALL verify that a Matter thermostat with Fan Control cluster can be commissioned and includes both base thermostat resources and fan control resources (`fanMode`, `fanOn`).

#### Scenario: Commission thermostat with fan control
- **WHEN** a virtual Matter thermostat with Fan Control cluster is commissioned via Barton
- **THEN** the device SHALL appear with deviceClass `thermostat` and have fan resources (`fanMode`, `fanOn`) in addition to all base thermostat resources

### Requirement: Fan mode read test
An integration test SHALL verify that the `fanMode` resource can be read on a thermostat with Fan Control cluster support.

#### Scenario: Read fan mode
- **WHEN** a thermostat with Fan Control cluster is commissioned
- **THEN** the `fanMode` resource SHALL be readable with an initial value of `"auto"`

### Requirement: Fan on read test
An integration test SHALL verify that the `fanOn` resource can be read on a thermostat with Fan Control cluster support.

#### Scenario: Read fan on
- **WHEN** a thermostat with Fan Control cluster is commissioned
- **THEN** the `fanOn` resource SHALL be readable with an initial value of `"false"`
