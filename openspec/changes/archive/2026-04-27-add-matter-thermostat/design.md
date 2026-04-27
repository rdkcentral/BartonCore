## Context

Barton manages thermostat devices today via a native Zigbee C driver (`zigbeeThermostatDeviceDriver.c`) that maps Zigbee Thermostat and Fan Control cluster attributes to a well-defined resource model under the `thermostat` device class. Clients already consume this resource model.

The SBMD framework enables declarative Matter device support without C code. Six SBMD specs exist today (light, door-lock, contact-sensor, occupancy-sensor, temperature-humidity-sensor, air-quality-sensor, water-leak-detector). Adding a thermostat SBMD spec follows the established pattern.

The Matter Thermostat device type (0x0301) mandates the Thermostat cluster (0x0201) with attributes for local temperature, setpoints, system mode, and running state. This maps naturally to the existing Barton thermostat resource model.

```
+--------------------+     +--------------------+     +--------------------+
| Client (GObject)   |     | Core / DeviceService|    | Matter Subsystem   |
|                    |     |                    |     |                    |
| BCoreResource      |<--->| SBMD Runtime       |<--->| CHIP SDK           |
| read/write/events  |     | thermostat.sbmd    |     | Thermostat 0x0201  |
+--------------------+     +--------------------+     +--------------------+
                                    |
                           +--------+--------+
                           | QuickJS Engine   |
                           | JS mapper scripts|
                           +-----------------+
```

## Goals / Non-Goals

**Goals:**
- Map Matter Thermostat cluster (0x0201) mandatory attributes to the existing Barton thermostat resource model via an SBMD spec
- Reuse the same resource IDs, types, and value formats as the Zigbee thermostat driver so clients work without changes
- Provide integration test coverage using a matter.js virtual thermostat device
- Support reading and writing of setpoints and system mode
- Optional Fan Control cluster (0x0202) support — when a thermostat device includes the Fan Control cluster, expose `fanMode` and `fanOn` resources via prerequisite-gated optional resources in the SBMD spec
- Test both thermostat-only and thermostat-with-fan configurations using a subclass-based virtual device approach to avoid duplicating common thermostat logic

**Non-Goals:**
- Schedule programming or Thermostat User Interface Configuration clusters
- Setpoint hold (not a mandatory attribute in Matter Thermostat)
- Unoccupied setpoints
- Local temperature calibration (optional attribute in Matter)
- Multi-endpoint thermostat support beyond what SBMD handles natively

## Decisions

### 1. SBMD spec (not native C++ driver)

**Decision**: Implement as an SBMD YAML spec with JavaScript mappers.

**Rationale**: All other Matter device drivers in Barton use SBMD. The thermostat resource model is a straightforward attribute-to-resource mapping that SBMD handles well. No custom lifecycle logic or cross-cluster coordination is needed for mandatory-only support.

**Alternative considered**: Native C++ `MatterDeviceDriver` subclass — rejected because it would be the only native Matter device driver besides Philips Hue (which has special IP/bridge requirements), and SBMD provides the same functionality declaratively.

### 2. Resource model alignment with Zigbee driver

**Decision**: Use the same resource IDs and value string formats as the Zigbee thermostat driver (`localTemperature`, `heatSetpoint`, `coolSetpoint`, `systemMode`, `systemState`).

**Rationale**: Clients already consume the thermostat device class. Using identical resource IDs ensures zero client-side changes. Temperature values use the same hundredths-of-degrees Celsius format (matching both Matter and the existing Zigbee convention).

### 3. Matter Thermostat cluster attribute mapping

**Decision**: Map the following mandatory Thermostat cluster (0x0201) attributes:

| Matter Attribute            | Attr ID  | Type   | Barton Resource       | Barton Type                    | Modes                     |
|-----------------------------|----------|--------|-----------------------|--------------------------------|---------------------------|
| LocalTemperature            | 0x0000   | int16  | localTemperature      | com.icontrol.temperature       | read, dynamic, emitEvents |
| OccupiedHeatingSetpoint     | 0x0012   | int16  | heatSetpoint          | com.icontrol.temperature       | read, write, dynamic, emitEvents |
| OccupiedCoolingSetpoint     | 0x0011   | int16  | coolSetpoint          | com.icontrol.temperature       | read, write, dynamic, emitEvents |
| AbsMinHeatSetpointLimit     | 0x0003   | int16  | absoluteMinHeatLimit  | com.icontrol.temperature       | read                      |
| AbsMaxHeatSetpointLimit     | 0x0004   | int16  | absoluteMaxHeatLimit  | com.icontrol.temperature       | read                      |
| AbsMinCoolSetpointLimit     | 0x0005   | int16  | absoluteMinCoolLimit  | com.icontrol.temperature       | read                      |
| AbsMaxCoolSetpointLimit     | 0x0006   | int16  | absoluteMaxCoolLimit  | com.icontrol.temperature       | read                      |
| ControlSequenceOfOperation  | 0x001b   | enum8  | controlSequenceOfOperation | com.icontrol.tstatCtrlSeqOp | read, dynamic, emitEvents |
| SystemMode                  | 0x001c   | enum8  | systemMode            | com.icontrol.tstatSystemMode   | read, write, dynamic, emitEvents |
| ThermostatRunningState      | 0x0029   | map16  | systemState           | com.icontrol.tstatSystemState  | read, dynamic, emitEvents |

**Rationale**: These are the mandatory attributes from the Matter Thermostat cluster specification. The mapping preserves the existing Zigbee resource model convention of storing temperatures in hundredths of degrees Celsius as string integers.

### 4. SystemMode write mapping

**Decision**: Map Barton systemMode string values to Matter SystemMode enum values and write via the Thermostat cluster `SetSystemMode` approach (direct attribute write of SystemMode attribute 0x001c).

| Barton Value  | Matter Enum |
|---------------|-------------|
| off           | 0x00        |
| heat          | 0x04        |
| cool          | 0x03        |
| auto          | 0x01        |

**Alternative considered**: Using a Matter command instead of attribute write — rejected because the Matter Thermostat cluster uses direct attribute writes for SystemMode per the specification.

### 5. Setpoint writes

**Decision**: Write setpoints via direct attribute writes to OccupiedHeatingSetpoint (0x0012) and OccupiedCoolingSetpoint (0x0011) on the Thermostat cluster.

**Rationale**: The Matter Thermostat cluster supports both direct attribute writes and the `SetpointRaiseLower` command. Direct writes are simpler and match the Zigbee driver behavior of setting absolute setpoint values.

### 6. matter.js virtual thermostat for testing

**Decision**: Create a `ThermostatDevice.js` extending `VirtualDevice` with Thermostat cluster support and side-band operations for `setTemperature`, `setSystemMode`, and `getState`.

**Rationale**: Follows the established pattern (DoorLockDevice.js, LightDevice.js). Side-band operations simulate real-world thermostat behavior changes that should trigger attribute reports to Barton.

### 7. Optional fan control resources (fanMode, fanOn)

**Decision**: Include `fanMode` and `fanOn` as optional resources backed by the Fan Control cluster (0x0202) with prerequisite gates, so they are only present when the device supports fan control.

**Rationale**: The Matter Thermostat device type does not mandate the Fan Control cluster, but some thermostats do support it. Using SBMD schema 2.0 `optional` resources with prerequisite gates on the Fan Control cluster aliases, the driver automatically exposes fan resources when available and omits them when not. This avoids hard-coding assumptions about fan support.

**Alternative considered**: Always providing static stub fan resources — rejected because optional prerequisite-gated resources are more accurate and schema 2.0 supports them natively.

### 8. Subclass-based virtual device variants for testing

**Decision**: Create a `ThermostatWithFanDevice.js` that extends `ThermostatDevice` to add the Fan Control cluster (0x0202) and fan-related side-band operations (`setFanMode`, `getFanState`). On the Python side, create a `MatterThermostatWithFan` subclass of `MatterThermostat` that points to the fan-capable JS entry point. Tests are split into `thermostat_test.py` (base thermostat, no fan control) and `thermostat_with_fan_test.py` (fan control present).

**Rationale**: Subclassing avoids duplicating the common thermostat endpoint setup (clusters, initial state, core side-band operations) in both the JS and Python layers. The base `thermostat_test.py` verifies that fan resources are absent when the Fan Control cluster is not present, while `thermostat_with_fan_test.py` verifies they appear and function correctly. This ensures both configurations are exercised.

**Alternative considered**: A single `ThermostatDevice.js` with a constructor option to conditionally include the Fan Control cluster — rejected because no existing virtual device uses constructor-time cluster selection, and the subclass approach keeps each device file simple and consistent with the existing 1:1 pattern.

## Risks / Trade-offs

- **[Risk] Matter thermostat devices may support varying optional attributes** → Mitigation: SBMD `optional` resource flag can be used for non-mandatory attributes in future iterations. The initial spec covers only mandatory attributes.
- **[Risk] SystemMode enum mapping may not cover all values** → Mitigation: Only map values that have Barton equivalents. Unknown values from the device will be passed through as raw integers. The Zigbee driver has the same limitation.
- **[Risk] ThermostatRunningState is optional in Matter spec** → Mitigation: Mark the `systemState` resource as optional in the SBMD spec so devices without this attribute still commission successfully.
- **[Trade-off] Optional fan control** → The Matter Thermostat device type does not mandate Fan Control. The `fanMode` and `fanOn` resources are optional and prerequisite-gated on the Fan Control cluster (0x0202). Both configurations (with and without fan control) are tested via subclass-based virtual device variants.

## Thread Safety

No thread safety concerns specific to this change. SBMD specs execute JavaScript mappers in the QuickJS engine, which is single-threaded per device instance and managed by the existing `SpecBasedMatterDeviceDriver` runtime. All attribute reports flow through the Matter subsystem's event loop into the GLib main loop via the established callback path.

## GObject API / Backward Compatibility

No GObject API changes. The `thermostat` device class and all associated resource types (`com.icontrol.temperature`, `com.icontrol.tstatSystemMode`, etc.) are already defined in `commonDeviceDefs.h` and `resourceTypes.h`. Matter thermostats will appear as `thermostat` device class devices, indistinguishable from Zigbee thermostats at the API level.
