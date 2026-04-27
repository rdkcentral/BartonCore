## ADDED Requirements

### Requirement: Matter Thermostat device type claiming
The SBMD driver SHALL claim Matter devices with device type ID 0x0301 (Thermostat) and register them under the Barton `thermostat` device class.

#### Scenario: Thermostat device is commissioned
- **WHEN** a Matter device with device type 0x0301 is commissioned
- **THEN** Barton SHALL create a device with deviceClass `thermostat` and a single endpoint with profile `thermostat`

### Requirement: LocalTemperature attribute mapping
The driver SHALL map the Thermostat cluster (0x0201) LocalTemperature attribute (0x0000, int16) to the `localTemperature` resource with type `com.icontrol.temperature`, modes [read, dynamic, emitEvents]. The value SHALL be the raw Matter value (hundredths of degrees Celsius) serialized as a string integer.

#### Scenario: Read local temperature
- **WHEN** the Matter device reports LocalTemperature as 2350
- **THEN** the `localTemperature` resource value SHALL be `"2350"`

#### Scenario: Null local temperature
- **WHEN** the Matter device reports LocalTemperature as null
- **THEN** the `localTemperature` resource value SHALL be `null`

### Requirement: OccupiedHeatingSetpoint attribute mapping
The driver SHALL map OccupiedHeatingSetpoint (0x0012, int16) to the `heatSetpoint` resource with type `com.icontrol.temperature`, modes [read, write, dynamic, emitEvents]. The value SHALL be in hundredths of degrees Celsius as a string integer.

#### Scenario: Read heating setpoint
- **WHEN** the Matter device reports OccupiedHeatingSetpoint as 2000
- **THEN** the `heatSetpoint` resource value SHALL be `"2000"`

#### Scenario: Write heating setpoint
- **WHEN** a client writes `"2200"` to the `heatSetpoint` resource
- **THEN** the driver SHALL write the int16 value 2200 to OccupiedHeatingSetpoint (0x0012) on the Thermostat cluster (0x0201)

### Requirement: OccupiedCoolingSetpoint attribute mapping
The driver SHALL map OccupiedCoolingSetpoint (0x0011, int16) to the `coolSetpoint` resource with type `com.icontrol.temperature`, modes [read, write, dynamic, emitEvents]. The value SHALL be in hundredths of degrees Celsius as a string integer.

#### Scenario: Read cooling setpoint
- **WHEN** the Matter device reports OccupiedCoolingSetpoint as 2600
- **THEN** the `coolSetpoint` resource value SHALL be `"2600"`

#### Scenario: Write cooling setpoint
- **WHEN** a client writes `"2400"` to the `coolSetpoint` resource
- **THEN** the driver SHALL write the int16 value 2400 to OccupiedCoolingSetpoint (0x0011) on the Thermostat cluster (0x0201)

### Requirement: Absolute setpoint limit attribute mappings
The driver SHALL map the following read-only attributes to resources with type `com.icontrol.temperature`, mode [read]:

| Matter Attribute         | Attr ID | Barton Resource        |
|--------------------------|---------|------------------------|
| AbsMinHeatSetpointLimit  | 0x0003  | absoluteMinHeatLimit   |
| AbsMaxHeatSetpointLimit  | 0x0004  | absoluteMaxHeatLimit   |
| AbsMinCoolSetpointLimit  | 0x0005  | absoluteMinCoolLimit   |
| AbsMaxCoolSetpointLimit  | 0x0006  | absoluteMaxCoolLimit   |

#### Scenario: Read absolute heat limits
- **WHEN** the Matter device reports AbsMinHeatSetpointLimit as 700 and AbsMaxHeatSetpointLimit as 3000
- **THEN** the `absoluteMinHeatLimit` resource SHALL be `"700"` and `absoluteMaxHeatLimit` SHALL be `"3000"`

#### Scenario: Read absolute cool limits
- **WHEN** the Matter device reports AbsMinCoolSetpointLimit as 1600 and AbsMaxCoolSetpointLimit as 3200
- **THEN** the `absoluteMinCoolLimit` resource SHALL be `"1600"` and `absoluteMaxCoolLimit` SHALL be `"3200"`

### Requirement: SystemMode attribute mapping
The driver SHALL map SystemMode (0x001c, enum8) to the `systemMode` resource with type `com.icontrol.tstatSystemMode`, modes [read, write, dynamic, emitEvents].

Read mapping (Matter enum → Barton string):

| Matter Value | Barton String |
|-------------|---------------|
| 0x00        | off           |
| 0x01        | auto          |
| 0x03        | cool          |
| 0x04        | heat          |
| 0x05        | emergencyHeat |
| 0x06        | precooling    |
| 0x07        | fanOnly       |

Write mapping (Barton string → Matter enum): reverse of the above table.

#### Scenario: Read system mode off
- **WHEN** the Matter device reports SystemMode as 0x00
- **THEN** the `systemMode` resource value SHALL be `"off"`

#### Scenario: Read system mode heat
- **WHEN** the Matter device reports SystemMode as 0x04
- **THEN** the `systemMode` resource value SHALL be `"heat"`

#### Scenario: Write system mode to cool
- **WHEN** a client writes `"cool"` to the `systemMode` resource
- **THEN** the driver SHALL write enum8 value 0x03 to SystemMode attribute (0x001c) on the Thermostat cluster

### Requirement: ControlSequenceOfOperation attribute mapping
The driver SHALL map ControlSequenceOfOperation (0x001b, enum8) to the `controlSequenceOfOperation` resource with type `com.icontrol.tstatCtrlSeqOp`, modes [read, dynamic, emitEvents].

#### Scenario: Read control sequence of operation
- **WHEN** the Matter device reports ControlSequenceOfOperation as 0x04
- **THEN** the `controlSequenceOfOperation` resource value SHALL be `"4"`

### Requirement: ThermostatRunningState attribute mapping
The driver SHALL map ThermostatRunningState (0x0029, map16) to the `systemState` resource with type `com.icontrol.tstatSystemState`, modes [read, dynamic, emitEvents]. This resource SHALL be marked as optional since ThermostatRunningState is optional in the Matter specification.

The running state bitmap SHALL be mapped to string values:
- Bit 0 (Heat) set → `"heating"`
- Bit 1 (Cool) set → `"cooling"`
- Neither set → `"off"`

#### Scenario: Running state heating
- **WHEN** the Matter device reports ThermostatRunningState with bit 0 set (value 0x0001)
- **THEN** the `systemState` resource value SHALL be `"heating"`

#### Scenario: Running state cooling
- **WHEN** the Matter device reports ThermostatRunningState with bit 1 set (value 0x0002)
- **THEN** the `systemState` resource value SHALL be `"cooling"`

#### Scenario: Running state off
- **WHEN** the Matter device reports ThermostatRunningState as 0x0000
- **THEN** the `systemState` resource value SHALL be `"off"`

### Requirement: Subscription reporting configuration
The SBMD spec SHALL configure attribute subscription reporting with a minimum interval of 1 second and a maximum interval of 3600 seconds (1 hour), consistent with other SBMD specs.

#### Scenario: Attribute reports are subscribed
- **WHEN** a thermostat device is commissioned
- **THEN** the driver SHALL subscribe to attribute reports with minSecs=1, maxSecs=3600

### Requirement: Optional fanMode resource
The driver SHALL include a `fanMode` resource with type `com.icontrol.tstatFanMode` as an optional resource backed by the Fan Control cluster (0x0202). The resource SHALL only be present when the device supports the Fan Control cluster, using a prerequisite gate on the `fanMode` alias. When present, the resource SHALL map the FanMode attribute (0x0000) to string values.

#### Scenario: fanMode resource present with fan control cluster
- **WHEN** a Matter thermostat with Fan Control cluster (0x0202) is commissioned
- **THEN** the `fanMode` resource SHALL exist with value `"auto"`

#### Scenario: fanMode resource absent without fan control cluster
- **WHEN** a Matter thermostat without Fan Control cluster is commissioned
- **THEN** the `fanMode` resource SHALL NOT be present on the endpoint

#### Scenario: fanMode write
- **WHEN** a client writes a value to the `fanMode` resource on a thermostat with Fan Control cluster
- **THEN** the write SHALL succeed

### Requirement: Optional fanOn resource
The driver SHALL include a `fanOn` resource with type `boolean` as an optional resource derived from the Fan Control cluster (0x0202). The resource SHALL only be present when the device supports the Fan Control cluster, using a prerequisite gate on the `fanMode` alias.

#### Scenario: fanOn resource present with fan control cluster
- **WHEN** a Matter thermostat with Fan Control cluster (0x0202) is commissioned
- **THEN** the `fanOn` resource SHALL exist with value `"false"`

#### Scenario: fanOn resource absent without fan control cluster
- **WHEN** a Matter thermostat without Fan Control cluster is commissioned
- **THEN** the `fanOn` resource SHALL NOT be present on the endpoint
