## ADDED Requirements

### Requirement: Temperature-humidity composite sensor SBMD spec
The system SHALL include an SBMD spec file `temperature-humidity-sensor.sbmd` that defines a composite `temperatureHumiditySensor` device class requiring both Matter device types `0x0302` (Temperature Sensor) and `0x0307` (Humidity Sensor) using `deviceTypeMatch: "all"`.

#### Scenario: Composite device is claimed
- **WHEN** a Matter device has endpoint 1 with device type `0x0302` and endpoint 2 with device type `0x0307`
- **THEN** the `temperature-humidity-sensor` driver SHALL claim the device

#### Scenario: Single-function device is not claimed by composite driver
- **WHEN** a Matter device has endpoint 1 with device type `0x0302` but no endpoint with `0x0307`
- **THEN** the `temperature-humidity-sensor` driver SHALL NOT claim the device

### Requirement: Temperature-humidity sensor resources
The `temperature-humidity-sensor.sbmd` spec SHALL define a single SBMD endpoint with profile `sensor` containing two resources: `temperature` and `humidity`.

#### Scenario: Temperature resource read
- **WHEN** the `temperature` resource is read from a composite temperature-humidity device
- **THEN** the driver SHALL read Matter cluster `0x0402` (Temperature Measurement), attribute `0x0000` (MeasuredValue, type `int16`), and the mapper script SHALL convert the raw value (in 0.01°C units) to a decimal Celsius string (e.g., raw `2550` → `"25.50"`)

#### Scenario: Humidity resource read
- **WHEN** the `humidity` resource is read from a composite temperature-humidity device
- **THEN** the driver SHALL read Matter cluster `0x0405` (Relative Humidity Measurement), attribute `0x0000` (MeasuredValue, type `uint16`), and the mapper script SHALL convert the raw value (in 0.01% units) to a decimal percentage string (e.g., raw `5000` → `"50.00"`)

#### Scenario: Null measurement value
- **WHEN** the MeasuredValue attribute for temperature or humidity is null (0x8000 for int16 or 0xFFFF for uint16)
- **THEN** the mapper script SHALL return an empty string `""`

### Requirement: Temperature sensor SBMD spec
The system SHALL include an SBMD spec file `temperature-sensor.sbmd` that defines a `temperatureSensor` device class matching Matter device type `0x0302` (Temperature Sensor) using the default `"any"` match semantics.

#### Scenario: Temperature-only device is claimed
- **WHEN** a Matter device has an endpoint with device type `0x0302`
- **THEN** the `temperature-sensor` driver SHALL claim the device

#### Scenario: Temperature resource read
- **WHEN** the `temperature` resource is read
- **THEN** the driver SHALL read Matter cluster `0x0402`, attribute `0x0000` (MeasuredValue, `int16`), and the mapper script SHALL convert the raw value to a decimal Celsius string

### Requirement: Humidity sensor SBMD spec
The system SHALL include an SBMD spec file `humidity-sensor.sbmd` that defines a `humiditySensor` device class matching Matter device type `0x0307` (Humidity Sensor) using the default `"any"` match semantics.

#### Scenario: Humidity-only device is claimed
- **WHEN** a Matter device has an endpoint with device type `0x0307`
- **THEN** the `humidity-sensor` driver SHALL claim the device

#### Scenario: Humidity resource read
- **WHEN** the `humidity` resource is read
- **THEN** the driver SHALL read Matter cluster `0x0405`, attribute `0x0000` (MeasuredValue, `uint16`), and the mapper script SHALL convert the raw value to a decimal percentage string

### Requirement: Test devices for temperature and humidity sensors
The test infrastructure SHALL include Python virtual Matter devices for a composite temperature-humidity sensor, a standalone temperature sensor, and a standalone humidity sensor. These devices SHALL expose the appropriate Matter device types and clusters for integration testing.

#### Scenario: Composite test device exposes both device types
- **WHEN** the composite test device is instantiated
- **THEN** it SHALL expose endpoint 1 with device type `0x0302` and cluster `0x0402`, and endpoint 2 with device type `0x0307` and cluster `0x0405`

#### Scenario: Single-function test device exposes one device type
- **WHEN** the temperature-only test device is instantiated
- **THEN** it SHALL expose one endpoint with device type `0x0302` and cluster `0x0402`

### Requirement: Integration tests for sensor drivers
Integration tests SHALL verify device discovery, claiming, resource reading, and dynamic updates for all three sensor driver types.

#### Scenario: Discover and read composite sensor
- **WHEN** a composite temperature-humidity test device is commissioned
- **THEN** the device SHALL be claimed by the `temperature-humidity-sensor` driver, and both `temperature` and `humidity` resources SHALL return correct values

#### Scenario: Discover and read standalone temperature sensor
- **WHEN** a temperature-only test device is commissioned
- **THEN** the device SHALL be claimed by the `temperature-sensor` driver and the `temperature` resource SHALL return a correct value

#### Scenario: Discover and read standalone humidity sensor
- **WHEN** a humidity-only test device is commissioned
- **THEN** the device SHALL be claimed by the `humidity-sensor` driver and the `humidity` resource SHALL return a correct value
