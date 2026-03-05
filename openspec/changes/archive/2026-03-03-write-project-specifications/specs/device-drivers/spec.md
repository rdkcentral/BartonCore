## ADDED Requirements

### Requirement: DeviceDriver interface
The system SHALL define a `DeviceDriver` C struct with function pointers for all driver lifecycle and operational callbacks. Drivers SHALL be registered via `deviceDriverManagerRegisterDriver()`.

#### Scenario: Register a device driver
- **WHEN** a driver calls `deviceDriverManagerRegisterDriver()` with a populated `DeviceDriver` struct
- **THEN** the driver SHALL be added to the driver registry and available for device matching

### Requirement: Driver identification
Each `DeviceDriver` SHALL have a `driverName` (string), `subsystemName` (string identifying the protocol subsystem), and `supportedDeviceClasses` (linked list of device class strings the driver handles).

#### Scenario: Driver with multiple device classes
- **WHEN** a driver supports device classes `["light", "dimmableLight"]`
- **THEN** the driver SHALL be returned by `deviceDriverManagerGetDeviceDriversByDeviceClass()` for either class

### Requirement: Driver startup and shutdown lifecycle
Each driver SHALL implement `startup(callbackContext)` and `shutdown(callbackContext)` callbacks. `startup` SHALL be called during `deviceDriverManagerStartDeviceDrivers()` in registration order (using the ordered linked list). `shutdown` SHALL be called during service stop via hash map iteration (order is not guaranteed to be reverse of startup).

#### Scenario: Driver startup
- **WHEN** the device service starts and calls `deviceDriverManagerStartDeviceDrivers()`
- **THEN** each registered driver's `startup()` callback SHALL be invoked with its `callbackContext`

#### Scenario: Driver shutdown
- **WHEN** the device service stops
- **THEN** each driver's `shutdown()` callback SHALL be invoked, followed by `destroy()` for cleanup

### Requirement: Device discovery callbacks
Drivers SHALL implement `discoverDevices(ctx, deviceClass)` to start scanning, `stopDiscoveringDevices(ctx, deviceClass)` to cancel, and optionally `recoverDevices(ctx, deviceClass)` for recovery mode.

#### Scenario: Start device discovery
- **WHEN** the client calls `discover_start()` for a device class
- **THEN** the driver manager SHALL invoke `discoverDevices()` on all drivers supporting that device class

#### Scenario: Stop device discovery
- **WHEN** the client calls `discover_stop()`
- **THEN** the driver manager SHALL invoke `stopDiscoveringDevices()` on all active drivers

### Requirement: Device configuration pipeline
When a driver claims a discovered device, the system SHALL invoke the following callbacks in sequence: `configureDevice()`, `fetchInitialResourceValues()`, `registerResources()`, `devicePersisted()`.

#### Scenario: Configure newly discovered device
- **WHEN** a driver claims a discovered device
- **THEN** `configureDevice()` SHALL be called first to set up device-specific clusters and attributes

#### Scenario: Fetch initial values
- **WHEN** device configuration succeeds
- **THEN** `fetchInitialResourceValues()` SHALL be called to read the initial state from the physical device

#### Scenario: Register resources
- **WHEN** initial values are fetched
- **THEN** `registerResources()` SHALL be called to create Barton resource entries for the device

#### Scenario: Device persisted notification
- **WHEN** the device is saved to the database
- **THEN** `devicePersisted()` SHALL be called to notify the driver

### Requirement: Resource operation callbacks
Drivers SHALL implement `readResource()`, `writeResource()`, and `executeResource()` callbacks that translate between Barton resource URIs/values and protocol-specific operations.

#### Scenario: Driver read resource
- **WHEN** `readResource()` is called on a driver
- **THEN** the driver SHALL read the current value from the physical device and provide it as a string

#### Scenario: Driver write resource
- **WHEN** `writeResource()` is called with a new value
- **THEN** the driver SHALL translate the string value to the appropriate protocol command and send it to the device

### Requirement: Communication failure handling
Drivers MAY implement `communicationFailed()` and `communicationRestored()` callbacks. Drivers with `customCommFail=true` SHALL handle their own communication failure detection instead of relying on the watchdog.

#### Scenario: Communication failure callback
- **WHEN** the communication watchdog detects a device has not communicated within its timeout
- **THEN** the driver's `communicationFailed()` callback SHALL be invoked

#### Scenario: Communication restored callback
- **WHEN** successful communication occurs with a previously-failed device
- **THEN** the driver's `communicationRestored()` callback SHALL be invoked

### Requirement: Device synchronization
Drivers SHALL implement `synchronizeDevice()` to re-sync device state after communication is restored or on reconnection.

#### Scenario: Synchronize after comm restore
- **WHEN** communication with a device is restored
- **THEN** `synchronizeDevice()` SHALL be called to refresh all resource values from the device

### Requirement: Device removal notification
Drivers SHALL implement `deviceRemoved()` callback that is invoked when a device is removed from the system.

#### Scenario: Device removal
- **WHEN** a device is removed via `b_core_client_remove_device()`
- **THEN** the driver's `deviceRemoved()` callback SHALL be invoked to clean up driver-internal state

### Requirement: Neverreject flag
Drivers with `neverReject=true` SHALL accept all devices during discovery without requiring a matching device descriptor from the allow list.

#### Scenario: Driver with neverReject
- **WHEN** a device is discovered and the claiming driver has `neverReject=true`
- **THEN** the device SHALL be accepted regardless of whether it has a matching device descriptor

### Requirement: Device migrator support
The system SHALL support a `DeviceMigrator` struct for upgrading devices from one driver version to another. The migrator SHALL provide `configureDevice`, `fetchInitialResourceValues`, `registerResources`, and `devicePersisted` callbacks scoped to the migration.

#### Scenario: Device migration on version change
- **WHEN** a device's driver version is older than the current driver version
- **THEN** the system SHALL invoke the device migrator callbacks to upgrade the device's resource set

### Requirement: Endpoint profile version registration
Drivers SHALL register their supported endpoint profile versions via `DRIVER_REGISTER_PROFILE_VERSION(driver, profile, version)` macro, stored in the driver's `endpointProfileVersions` hash map.

#### Scenario: Register profile version
- **WHEN** a driver calls `DRIVER_REGISTER_PROFILE_VERSION(driver, "sensor", 2)`
- **THEN** the driver's endpoint profile versions map SHALL contain the entry `"sensor" -> 2`

### Requirement: Native Zigbee driver catalog
The system SHALL include the following native Zigbee device drivers: `zigbeeDoorLock` (doorLock class), `zigbeeLight` (light class), `zigbeeSensor` (sensor class), `zigbeeThermostat` (thermostat class), `zigbeeWindowCovering` (windowCovering class), `zigbeePresence` (presence class), `zigbeeLightController` (lightController class), `zigbeeOccupancySensor` (sensor class with occupancy type).

#### Scenario: Zigbee light driver resources
- **WHEN** a Zigbee light is paired
- **THEN** the `zigbeeLight` driver SHALL register endpoint resources including `isOn`, `level`, and conditionally `colorMode`, `colorTemp`, `hue`, `saturation` based on device capabilities

#### Scenario: Zigbee sensor driver resources
- **WHEN** a Zigbee sensor is paired
- **THEN** the `zigbeeSensor` driver SHALL register endpoint resources including `faulted`, `tampered`, `bypassed`, `type`, `qualified`, and `label`

### Requirement: Philips Hue driver
The system SHALL include a `philipsHue` IP-based device driver when `BCORE_PHILIPS_HUE=ON`, supporting Philips Hue bridge device management.

#### Scenario: Philips Hue conditional build
- **WHEN** `BCORE_PHILIPS_HUE=ON` in the CMake configuration
- **THEN** the Philips Hue driver SHALL be compiled and available for registration

### Requirement: Config restore callbacks
Drivers MAY implement `restoreConfig()`, `preRestoreConfig()`, and `postRestoreConfig()` callbacks for backup/restore scenarios.

#### Scenario: Restore driver config
- **WHEN** `b_core_client_config_restore()` is called
- **THEN** the driver manager SHALL invoke `preRestoreConfig()` on all drivers, then `restoreConfig()` with the backup directory, then `postRestoreConfig()`

### Requirement: System event callbacks
Drivers MAY implement `systemPowerEvent()` and `propertyChanged()` callbacks to react to system-wide events.

#### Scenario: Property change notification to driver
- **WHEN** a system property changes
- **THEN** all drivers with `propertyChanged` callbacks SHALL be notified with the property key and new value

### Requirement: Additional driver callbacks
Drivers MAY implement these additional callbacks:
- `processDeviceDescriptor(ctx, device, dd)` — examine device against its descriptor for firmware upgrades or changes
- `endpointDisabled(ctx, endpoint)` — notification when an endpoint is disabled
- `fetchRuntimeStats(ctx, output)` — collect device-specific runtime statistics
- `getDeviceClassVersion(ctx, deviceClass, version)` — retrieve the device class version for a given device class
- `subsystemInitialized(ctx)` — notification that the driver's subsystem has been initialized
- `serviceStatusChanged(ctx, status)` — notification when the device service status changes
- `commFailTimeoutSecsChanged(driver, device, commFailTimeoutSecs)` — notification when the comm-fail timeout has changed
