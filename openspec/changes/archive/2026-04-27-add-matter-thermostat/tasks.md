## 1. SBMD Thermostat Spec

- [x] 1.1 Create `core/deviceDrivers/matter/sbmd/specs/thermostat.sbmd` with Matter Thermostat device type (0x0301), Thermostat cluster (0x0201), and all mandatory attribute read mappers (LocalTemperature, OccupiedHeatingSetpoint, OccupiedCoolingSetpoint, AbsMinHeatSetpointLimit, AbsMaxHeatSetpointLimit, AbsMinCoolSetpointLimit, AbsMaxCoolSetpointLimit, ControlSequenceOfOperation, SystemMode, ThermostatRunningState)
- [x] 1.2 Add write mappers for OccupiedHeatingSetpoint and OccupiedCoolingSetpoint (int16 attribute writes to cluster 0x0201)
- [x] 1.3 Add write mapper for SystemMode (string-to-enum8 mapping, attribute write to 0x001c)
- [x] 1.4 Mark ThermostatRunningState (systemState) resource as optional since the attribute is optional in the Matter spec
- [x] 1.5 Add optional `fanMode` and `fanOn` resources backed by Fan Control cluster (0x0202) with prerequisite gates — present only when device supports Fan Control
- [x] 1.6 Validate the SBMD spec builds successfully (`make` in build directory — schema validation runs at build time)

## 2. matter.js Virtual Thermostat Device

- [x] 2.1 Create `testing/mocks/devices/matterjs/src/ThermostatDevice.js` extending VirtualDevice with Thermostat device type (0x0301) and Thermostat cluster on endpoint 1
- [x] 2.2 Set initial thermostat state: LocalTemperature=2100, OccupiedHeatingSetpoint=2000, OccupiedCoolingSetpoint=2600, SystemMode=Off
- [x] 2.3 Implement side-band operations: `setTemperature`, `setSystemMode`, `getState`
- [x] 2.4 Verify virtual device starts and emits ready signal with `node ThermostatDevice.js --passcode ... --discriminator ...`

## 2b. matter.js Virtual Thermostat with Fan Control Device

- [x] 2b.1 Create `testing/mocks/devices/matterjs/src/ThermostatWithFanDevice.js` subclassing `ThermostatDevice` to add Fan Control cluster (0x0202) on the thermostat endpoint with initial FanMode=Auto
- [x] 2b.2 Implement fan-related side-band operations: `setFanMode`, `getFanState`
- [x] 2b.3 Verify virtual device starts with both Thermostat and Fan Control clusters

## 3. Python Test Fixture

- [x] 3.1 Create `testing/mocks/devices/matter/matter_thermostat.py` with `MatterThermostat` class extending `MatterDevice` and a `matter_thermostat` pytest fixture

## 3b. Python Test Fixture for Thermostat with Fan Control

- [x] 3b.1 Create `testing/mocks/devices/matter/matter_thermostat_with_fan.py` with `MatterThermostatWithFan` subclassing `MatterThermostat` (entry point `ThermostatWithFanDevice.js`) and a `matter_thermostat_with_fan` pytest fixture

## 4. Integration Tests

- [x] 4.1 Create `testing/test/thermostat_test.py` with `test_commission_thermostat` test verifying device appears as `thermostat` device class with common resources
- [x] 4.2 Add `test_write_heating_setpoint` test verifying setpoint write via Barton and side-band verification
- [x] 4.3 Add `test_write_cooling_setpoint` test verifying cooling setpoint write via Barton and side-band verification
- [x] 4.4 Add `test_write_system_mode` test verifying system mode write via Barton and side-band verification
- [x] 4.5 Add `test_sideband_temperature_change_triggers_barton_update` test verifying side-band temperature change generates a Barton resource update event
- [x] 4.6 Add `test_no_fan_resources_on_base_thermostat` test verifying `fanMode` and `fanOn` resources are NOT present on a thermostat without Fan Control cluster
- [x] 4.7 Run full integration test suite and verify all thermostat tests pass (requires Docker environment with Matter stack)

## 4b. Integration Tests for Thermostat with Fan Control

- [x] 4b.1 Create `testing/test/thermostat_with_fan_test.py` with `test_commission_thermostat_with_fan` test verifying device appears as `thermostat` device class with fan resources (`fanMode`, `fanOn`) present
- [x] 4b.2 Add `test_read_fan_mode` test verifying `fanMode` resource is readable with initial value `"auto"`
- [x] 4b.3 Add `test_read_fan_on` test verifying `fanOn` resource is readable with initial value `"false"`
- [x] 4b.4 Run thermostat with fan integration tests and verify all pass
