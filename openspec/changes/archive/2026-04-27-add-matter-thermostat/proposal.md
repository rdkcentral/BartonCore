## Why

Barton currently supports Matter lights, door locks, sensors, and other device types via SBMD specs, but has no support for Matter thermostats. Thermostats are one of the most common smart home devices, and the existing Zigbee thermostat driver already defines the resource model that clients depend on. Adding a Matter thermostat SBMD driver enables Barton to manage Matter-based thermostats with the same unified resource API that clients already use for Zigbee thermostats.

## What Changes

- **New SBMD spec**: A `thermostat.sbmd` spec file mapping the Matter Thermostat device type (0x0301) to the existing Barton `thermostat` device class, implementing the same resource model as the Zigbee thermostat driver.
- **Optional fan control resources**: `fanMode` and `fanOn` resources backed by the Fan Control cluster (0x0202) with prerequisite gates, so they are only present when the device supports fan control.
- **New matter.js virtual device**: A `ThermostatDevice.js` virtual device for integration testing, supporting the Thermostat cluster (0x0201) with side-band operations for simulating temperature changes, setpoint adjustments, and mode switches. A `ThermostatWithFanDevice.js` subclass adds the Fan Control cluster (0x0202) and fan-related side-band operations, reusing the base thermostat endpoint logic.
- **New Python test fixtures**: A `matter_thermostat.py` fixture wrapping the matter.js virtual thermostat, and a `matter_thermostat_with_fan.py` fixture for the fan-capable variant (subclassing `MatterThermostat` to avoid duplicating common setup).
- **New integration tests**: A `thermostat_test.py` test suite covering commissioning, setpoint read/write, system mode changes, and side-band attribute report verification for thermostats without fan control. A `thermostat_with_fan_test.py` test suite verifying fan control resources are present and functional when the Fan Control cluster is supported.

## Non-goals

- Thermostat schedule programming (ScheduleConfiguration cluster).
- Setpoint hold/unoccupied setpoint support — only occupied heating/cooling setpoints are in scope (mandatory attributes).
- Legacy thermostat compatibility concerns — this is a new Matter-only driver with no Zigbee backward compatibility considerations.
- Thermostat User Interface Configuration cluster.

## Capabilities

### New Capabilities
- `matter-thermostat-sbmd`: SBMD spec for Matter Thermostat device type mapping Thermostat cluster attributes to the Barton thermostat resource model.
- `matter-thermostat-testing`: Integration test infrastructure (matter.js virtual devices for thermostat with and without fan control, Python fixtures, pytest test suites) for the Matter thermostat driver.

### Modified Capabilities

_(none — no existing spec-level requirements change)_

## Impact

- **Drivers** (`core/deviceDrivers/matter/sbmd/specs/`): New `thermostat.sbmd` file added.
- **Testing** (`testing/`): `ThermostatDevice.js` and `ThermostatWithFanDevice.js` virtual devices, `matter_thermostat.py` and `matter_thermostat_with_fan.py` fixtures, and `thermostat_test.py` and `thermostat_with_fan_test.py` test suites.
- **CMake**: No changes — SBMD specs are auto-discovered at build time by schema validation and at runtime by `SbmdFactory`.
- **API**: No changes — uses the existing `thermostat` device class and resource types already defined in `commonDeviceDefs.h`.
- **CMake feature flag**: `BCORE_MATTER` must be enabled.
