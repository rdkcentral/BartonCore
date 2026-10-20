## MODIFIED Requirements

### Requirement: Driver load cost tracking
The SBMD runtime SHALL record, for each successfully loaded driver: the wall-clock time to evaluate its `.sbmd.js` file and register it as a histogram named `sbmd.driver.load.duration_ms` with attribute `"driver"`, and the net heap bytes consumed during load and reduction to claim metadata as a histogram named `sbmd.driver.load.heap_bytes` with attribute `"driver"`. Both SHALL be recorded once per driver at startup in `SbmdFactory::RegisterDriversFromDirectory`. These metrics SHALL NOT include activation cost, because drivers are no longer activated at load.

#### Scenario: Load duration recorded per driver
- **WHEN** a `.sbmd.js` file is successfully loaded and registered
- **THEN** `sbmd.driver.load.duration_ms` contains one observation attributed to that driver's name

#### Scenario: Load heap cost recorded per driver
- **WHEN** a `.sbmd.js` file is successfully loaded and reduced to its claim metadata
- **THEN** `sbmd.driver.load.heap_bytes` contains one observation reflecting the resident heap consumed

### Requirement: Driver load failure tracking
The SBMD runtime SHALL count driver load failures using a counter named `sbmd.driver.load.failure` with attributes `"driver"` (filename stem) and `"reason"`. Valid reason values are: `"file_read"` (file could not be opened or read) and `"eval_failed"` (JS evaluation or registration extraction failed). Activation no longer occurs at load, so `reason="activation_failed"` SHALL NOT be emitted here; runtime activation failures are tracked by the activation-failure counter (see Driver activation lifecycle tracking).

#### Scenario: Evaluation failure counted
- **WHEN** `SbmdLoader::LoadDriver` fails due to a JS evaluation or registration-extraction error
- **THEN** `sbmd.driver.load.failure` counter for `reason="eval_failed"` increments for that driver

#### Scenario: File read failure counted
- **WHEN** a `.sbmd.js` file cannot be opened or read during load
- **THEN** `sbmd.driver.load.failure` counter for `reason="file_read"` increments for that driver

## ADDED Requirements

### Requirement: Driver activation lifecycle tracking
Because drivers are loaded inactive and activated on demand when a device first binds (and deactivated when the last device is removed), the SBMD runtime SHALL record the following under `BARTON_CONFIG_SBMD_METRICS`, owned by `SpecBasedMatterDeviceDriver`:
- a counter `sbmd.driver.activation` with attribute `"driver"` (filename stem), incremented on each successful on-demand activation;
- a counter `sbmd.driver.deactivation` with attribute `"driver"`, incremented on each deactivation;
- a gauge `sbmd.driver.active.count` set to the live number of activated drivers on each activation/deactivation transition (and published once after startup registration);
- a histogram `sbmd.driver.activation.duration_ms` with attribute `"driver"`, recording the time to activate a driver on demand;
- a counter `sbmd.driver.activation.failure` with attributes `"driver"` and `"reason"`, incremented when a bind-time activation fails. Valid reason values are `"activate_failed"` (spec unreadable/unparseable or claim-identity mismatch) and `"version_mismatch"` (the re-read device-class or endpoint profile version/set no longer matches the values cached at construction).

#### Scenario: Activation recorded on first bind
- **WHEN** a device first binds to an inactive driver and activation succeeds
- **THEN** `sbmd.driver.activation` increments for that driver, `sbmd.driver.active.count` reflects the new active total, and `sbmd.driver.activation.duration_ms` records one observation

#### Scenario: Deactivation recorded on last-device removal
- **WHEN** the last device bound to an active driver is removed
- **THEN** `sbmd.driver.deactivation` increments for that driver and `sbmd.driver.active.count` reflects the reduced active total

#### Scenario: Activation failure counted with reason
- **WHEN** a bind-time activation fails because the re-read spec cannot be read or parsed or its claim identity changed
- **THEN** `sbmd.driver.activation.failure` increments for that driver with `reason="activate_failed"`

#### Scenario: Version-mismatch activation failure counted
- **WHEN** a bind-time activation is rolled back because the re-read device-class or profile version/set no longer matches the cached values
- **THEN** `sbmd.driver.activation.failure` increments for that driver with `reason="version_mismatch"`
