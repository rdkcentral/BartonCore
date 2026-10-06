## ADDED Requirements

### Requirement: Contact Sensor SBMD driver uses dual-path live updates for faulted
The Contact Sensor SBMD driver SHALL add an `eventHandlers.handleStateChange` handler alongside the existing `attributeHandlers.handleStateValue` handler (both active). Live updates to `faulted` SHALL be driven by both the `StateValue` attribute report and the `BooleanState.StateChange` event (cluster 0x0045, event 0x0000); because `StateChange` is optional conformance in Matter 1.5.1, the attribute path is the reliable baseline and same-value updates are suppressed so the two paths do not produce duplicate events. A `seed` handler SHALL read the `StateValue` attribute to establish the `faulted` value. The seed also runs on every synchronize/reconnect: when `StateValue` is available it sets `faulted` from it (closed → `"false"`, open → `"true"`); when `StateValue` is unavailable it SHALL preserve an already-set `faulted` value and fall back to the fail-safe `"true"` (faulted) only at commission when no value exists yet, so a reconnect with a cold attribute cache does not raise a spurious intrusion fault.

On `StateChange` receipt: StateValue = `true` (closed/contact) → `faulted = "false"`; StateValue = `false` (open/no contact) → `faulted = "true"`.

#### Scenario: StateChange with StateValue=true clears faulted
- **WHEN** a `BooleanState.StateChange` event is received with StateValue = `true` (contact present / closed)
- **THEN** the `faulted` resource SHALL be updated to `"false"`

#### Scenario: StateChange with StateValue=false sets faulted
- **WHEN** a `BooleanState.StateChange` event is received with StateValue = `false` (no contact / open)
- **THEN** the `faulted` resource SHALL be updated to `"true"`

#### Scenario: Unknown StateValue at commission fails safe to faulted
- **WHEN** the Contact Sensor is commissioned and its `StateValue` attribute is unavailable (no cached value and no existing `faulted` value)
- **THEN** the `faulted` resource SHALL be seeded to `"true"`

#### Scenario: Unknown StateValue on resynchronize preserves existing faulted
- **WHEN** the seed re-runs on resynchronize, the `faulted` resource already has a value, and `StateValue` is unavailable
- **THEN** the `faulted` resource SHALL retain its existing value

---

### Requirement: Contact Sensor SBMD driver version is 2
The Contact Sensor SBMD driver SHALL have `driverVersion: 2`.

#### Scenario: Driver version is 2
- **WHEN** the Contact Sensor SBMD driver is loaded
- **THEN** `driverVersion` SHALL equal `2`

---

### Requirement: Water Leak Detector SBMD driver uses dual-path live updates for faulted
The Water Leak Detector SBMD driver SHALL add an `eventHandlers.handleStateChange` handler alongside the existing `attributeHandlers.handleStateValue` handler (both active). Live updates to `faulted` SHALL be driven by both the `StateValue` attribute report and the `BooleanState.StateChange` event (cluster 0x0045, event 0x0000); because `StateChange` is optional conformance in Matter 1.5.1, the attribute path is the reliable baseline and same-value updates are suppressed so the two paths do not produce duplicate events. A `seed` handler SHALL read the `StateValue` attribute to establish the `faulted` value. The seed also runs on every synchronize/reconnect: when `StateValue` is available it sets `faulted` from it (water → `"true"`, dry → `"false"`); when `StateValue` is unavailable it SHALL preserve an already-set `faulted` value and fall back to the default `"false"` (not faulted) only at commission when no value exists yet, so a reconnect with a cold attribute cache does not clear a real leak.

On `StateChange` receipt: StateValue = `true` (water detected) → `faulted = "true"`; StateValue = `false` (no water) → `faulted = "false"`.

#### Scenario: StateChange with StateValue=true sets faulted
- **WHEN** a `BooleanState.StateChange` event is received on a Water Leak Detector with StateValue = `true` (water detected)
- **THEN** the `faulted` resource SHALL be updated to `"true"`

#### Scenario: StateChange with StateValue=false clears faulted
- **WHEN** a `BooleanState.StateChange` event is received on a Water Leak Detector with StateValue = `false` (no water)
- **THEN** the `faulted` resource SHALL be updated to `"false"`

#### Scenario: Unknown StateValue at commission defaults to not faulted
- **WHEN** the Water Leak Detector is commissioned and its `StateValue` attribute is unavailable (no cached value and no existing `faulted` value)
- **THEN** the `faulted` resource SHALL be seeded to `"false"`

#### Scenario: Unknown StateValue on resynchronize preserves existing faulted
- **WHEN** the seed re-runs on resynchronize, the `faulted` resource already has a value, and `StateValue` is unavailable
- **THEN** the `faulted` resource SHALL retain its existing value

---

### Requirement: Water Leak Detector SBMD driver version is 2
The Water Leak Detector SBMD driver SHALL have `driverVersion: 2`.

#### Scenario: Driver version is 2
- **WHEN** the Water Leak Detector SBMD driver is loaded
- **THEN** `driverVersion` SHALL equal `2`
