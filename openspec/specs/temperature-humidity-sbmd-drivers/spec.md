## ADDED Requirements

### Requirement: IKEA TIMMERFLOTTE SBMD spec
The system SHALL include an SBMD spec file `ikea-timmerflotte.sbmd` that defines a TIMMERFLOTTE-specific driver claiming by vendor ID and product ID. The spec SHALL also list device types `0x0302` (Temperature Sensor) and `0x0307` (Humidity Sensor) for endpoint mapping purposes.

#### Scenario: TIMMERFLOTTE device is claimed
- **WHEN** a Matter device reports VendorID matching IKEA and ProductID matching TIMMERFLOTTE
- **THEN** the `ikea-timmerflotte` driver SHALL claim the device

#### Scenario: Device with same device types but different vendor is not claimed
- **WHEN** a Matter device has endpoints with device types `0x0302` and `0x0307` but a different vendor ID
- **THEN** the `ikea-timmerflotte` driver SHALL NOT claim the device (the generic temperature-sensor or humidity-sensor driver will claim it instead)

### Requirement: IKEA TIMMERFLOTTE sensor resources
The `ikea-timmerflotte.sbmd` spec SHALL define a single SBMD endpoint with profile `sensor` containing two resources: `temperature` and `humidity`.

#### Scenario: Temperature resource read
- **WHEN** the `temperature` resource is read from a composite temperature-humidity device
- **THEN** the driver SHALL read Matter cluster `0x0402` (Temperature Measurement), attribute `0x0000` (MeasuredValue, type `int16`), and the mapper script SHALL return the raw hundredths-of-a-degree value as a string (e.g., raw `2550` → `"2550"`)

#### Scenario: Humidity resource read
- **WHEN** the `humidity` resource is read from a composite temperature-humidity device
- **THEN** the driver SHALL read Matter cluster `0x0405` (Relative Humidity Measurement), attribute `0x0000` (MeasuredValue, type `uint16`), and the mapper script SHALL convert the raw hundredths-of-a-percent value to whole percent as a string (e.g., raw `5000` → `"50"`)

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
- **THEN** the driver SHALL read Matter cluster `0x0402`, attribute `0x0000` (MeasuredValue, `int16`), and the mapper script SHALL return the raw hundredths-of-a-degree value as a string

### Requirement: Humidity sensor SBMD spec
The system SHALL include an SBMD spec file `humidity-sensor.sbmd` that defines a `humiditySensor` device class matching Matter device type `0x0307` (Humidity Sensor) using the default `"any"` match semantics.

#### Scenario: Humidity-only device is claimed
- **WHEN** a Matter device has an endpoint with device type `0x0307`
- **THEN** the `humidity-sensor` driver SHALL claim the device

#### Scenario: Humidity resource read
- **WHEN** the `humidity` resource is read
- **THEN** the driver SHALL read Matter cluster `0x0405`, attribute `0x0000` (MeasuredValue, `uint16`), and the mapper script SHALL convert the raw hundredths-of-a-percent value to whole percent as a string

### Requirement: Test devices for temperature and humidity sensors
The test infrastructure SHALL include Python virtual Matter devices for a TIMMERFLOTTE-like temperature-humidity sensor (with matching vendor/product ID), a standalone temperature sensor, and a standalone humidity sensor. These devices SHALL expose the appropriate Matter device types, clusters, and BasicInformation attributes for integration testing.

#### Scenario: TIMMERFLOTTE test device exposes both device types and vendor info
- **WHEN** the TIMMERFLOTTE test device is instantiated
- **THEN** it SHALL expose endpoint 1 with device type `0x0302` and cluster `0x0402`, endpoint 2 with device type `0x0307` and cluster `0x0405`, and BasicInformation with the IKEA vendor ID and TIMMERFLOTTE product ID

#### Scenario: Single-function test device exposes one device type
- **WHEN** the temperature-only test device is instantiated
- **THEN** it SHALL expose one endpoint with device type `0x0302` and cluster `0x0402`

### Requirement: Integration tests for sensor drivers
Integration tests SHALL verify device discovery, claiming, resource reading, and dynamic updates for all three sensor driver types.

#### Scenario: Discover and read TIMMERFLOTTE sensor
- **WHEN** a TIMMERFLOTTE temperature-humidity test device is commissioned
- **THEN** the device SHALL be claimed by the `ikea-timmerflotte` driver (via vendor/product ID match), and both `temperature` and `humidity` resources SHALL return correct values

#### Scenario: Discover and read standalone temperature sensor
- **WHEN** a temperature-only test device is commissioned
- **THEN** the device SHALL be claimed by the `temperature-sensor` driver and the `temperature` resource SHALL return a correct value

#### Scenario: Discover and read standalone humidity sensor
- **WHEN** a humidity-only test device is commissioned
- **THEN** the device SHALL be claimed by the `humidity-sensor` driver and the `humidity` resource SHALL return a correct value
