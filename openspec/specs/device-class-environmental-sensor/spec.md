# Device Class: Environmental Sensor

## Purpose

Specifies the Barton `environmentalSensor` device class: a device with a `sensor`-profile endpoint exposing measured environmental quantities — `temperature` and `humidity` — mapped from the Matter Temperature Measurement (0x0402) and Relative Humidity Measurement (0x0405) clusters. Concrete environmental-sensor drivers (standalone temperature, standalone humidity, and vendor-specific composite devices) are `.sbmd.js` files governed by the SBMD schema validator.

## Requirements

### Requirement: Environmental sensor device type claiming
A device advertising Matter device type 0x0302 (Temperature Sensor) and/or 0x0307 (Humidity Sensor) SHALL be claimed into the `environmentalSensor` device class. A vendor-specific composite driver that matches by vendor and product ID SHALL take priority over the generic per-function drivers for the same device.

#### Scenario: Standalone temperature sensor is claimed
- **WHEN** a Matter device exposes an endpoint with device type 0x0302
- **THEN** it SHALL be claimed into the `environmentalSensor` class and expose a `temperature` resource

#### Scenario: Vendor-specific composite device takes priority
- **WHEN** a Matter device exposes device types 0x0302 and 0x0307 and matches a vendor/product-specific driver
- **THEN** the vendor-specific driver SHALL claim the device instead of the generic temperature and humidity drivers

### Requirement: Temperature resource
The `temperature` resource SHALL be backed by the Matter Temperature Measurement cluster (0x0402), MeasuredValue attribute (0x0000, int16). Its value SHALL be the raw hundredths-of-a-degree-Celsius value serialized as a string; a null measurement (0x8000) SHALL serialize as an empty string `""`.

#### Scenario: Temperature read
- **WHEN** the MeasuredValue for cluster 0x0402 is 2550
- **THEN** the `temperature` resource value SHALL be `"2550"`

#### Scenario: Null temperature
- **WHEN** the MeasuredValue for cluster 0x0402 is null (0x8000)
- **THEN** the `temperature` resource value SHALL be `""`

### Requirement: Humidity resource
The `humidity` resource SHALL be backed by the Matter Relative Humidity Measurement cluster (0x0405), MeasuredValue attribute (0x0000, uint16). Its value SHALL convert the raw hundredths-of-a-percent value to whole percent, serialized as a string; a null measurement (0xFFFF) SHALL serialize as an empty string `""`.

#### Scenario: Humidity read
- **WHEN** the MeasuredValue for cluster 0x0405 is 5000
- **THEN** the `humidity` resource value SHALL be `"50"`

#### Scenario: Null humidity
- **WHEN** the MeasuredValue for cluster 0x0405 is null (0xFFFF)
- **THEN** the `humidity` resource value SHALL be `""`
