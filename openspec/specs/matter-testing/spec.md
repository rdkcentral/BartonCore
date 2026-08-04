# Matter Testing

## Purpose

Specifies the Matter test infrastructure: the matter.js virtual device framework and its side-band control, the concrete virtual test devices (door lock, thermostat with and without fan control), the Python test fixtures and side-band client, conditional test execution, and Docker environment setup.

## Requirements

### Requirement: MatterDevice refactored for matter.js-only backend
The `MatterDevice` base class SHALL exclusively use matter.js virtual devices. The `_app_name` attribute and all CHIP SDK sample app subprocess code SHALL be removed. `MatterDevice.__init__()` SHALL require a `matterjs_entry_point` parameter specifying the JavaScript file to run. `MatterDevice.start()` SHALL spawn a Node.js subprocess, wait for the JSON ready signal on stdout, and configure the `SidebandClient`.

#### Scenario: MatterDevice only supports matter.js
- **WHEN** a `MatterDevice` subclass is instantiated
- **THEN** it SHALL specify a `matterjs_entry_point`
- **AND** `start()` SHALL spawn `node <entry_point>` as a subprocess
- **AND** there SHALL be no code path for launching CHIP SDK sample apps

#### Scenario: SidebandClient auto-configured on start
- **WHEN** `device.start()` is called
- **THEN** the device SHALL parse the JSON ready signal from stdout
- **AND** configure a `SidebandClient` with the reported side-band port
- **AND** expose it via `device.sideband`

### Requirement: MatterDoorLock device class
A `MatterDoorLock` Python class SHALL be a standard `MatterDevice` subclass in `testing/mocks/devices/matter/`. It SHALL specify `matterjs_entry_point="DoorLockDevice.js"` and `device_class="doorLock"`.

#### Scenario: Start a door lock device
- **WHEN** `device.start()` is called on a `MatterDoorLock` instance
- **THEN** a Node.js subprocess SHALL be spawned running the door lock's JavaScript entry point
- **AND** the method SHALL block until the ready signal is received on stdout
- **AND** the device SHALL be ready for commissioning via Barton using its original commissioning code

#### Scenario: Stop a device
- **WHEN** `device.stop()` is called
- **THEN** the Node.js subprocess SHALL be terminated (SIGTERM)
- **AND** associated resources (temp directories, ports) SHALL be cleaned up

#### Scenario: Access commissioning code
- **WHEN** `device.get_commissioning_code()` is called
- **THEN** it SHALL return a valid Matter manual pairing code suitable for commissioning

### Requirement: MatterLight migrated to matter.js
`MatterLight` SHALL be migrated from `chip-lighting-app` to a matter.js virtual light device. It SHALL specify `matterjs_entry_point="LightDevice.js"`. Cluster registration calls (`_register_cluster`) and cluster imports SHALL be removed since the cluster class system is eliminated. The `matter_light` pytest fixture SHALL remain unchanged in its interface.

#### Scenario: MatterLight uses matter.js
- **WHEN** `MatterLight` is instantiated
- **THEN** it SHALL use a matter.js entry point instead of `chip-lighting-app`
- **AND** `start()` SHALL spawn a Node.js subprocess
- **AND** the `matter_light` fixture SHALL work identically to before
- **AND** `MatterLight` SHALL NOT import or register any chip-tool cluster classes

### Requirement: Chip-tool cluster classes and device interactor removed
The entire `testing/mocks/devices/matter/clusters/` directory SHALL be removed. This includes `MatterCluster` (base class), `OnOffCluster`, `LevelControlCluster`, and `ColorControlCluster`. The `_register_cluster()` method, `get_cluster()` method, and `_cluster_classes` attribute SHALL be removed from `MatterDevice`. `device_interactor.py` (`ChipToolDeviceInteractor`) SHALL be deleted entirely — chip-tool is no longer used in any capacity. The `_set_interactor()` method, `_interactor` attribute, and `_chip_tool_node_id` attribute SHALL be removed from `MatterDevice`. The `device_interactor` plugin registration SHALL be removed from `conftest.py`. Tests SHALL interact with device state through the side-band interface (`device.sideband`) and commission devices directly through the Barton API.

#### Scenario: No cluster class system
- **WHEN** `MatterDevice` is used
- **THEN** it SHALL NOT have `_register_cluster()`, `get_cluster()`, or `_cluster_classes`
- **AND** the `testing/mocks/devices/matter/clusters/` directory SHALL NOT exist

#### Scenario: No device interactor
- **WHEN** a device fixture creates a matter.js virtual device
- **THEN** it SHALL NOT depend on `device_interactor`
- **AND** `device_interactor.py` SHALL NOT exist
- **AND** `conftest.py` SHALL NOT register `device_interactor` as a plugin

#### Scenario: Tests use side-band instead of clusters
- **WHEN** a test needs to query or change device state from the device side
- **THEN** it SHALL use `device.sideband.send(operation)` or `device.sideband.get_state()`
- **AND** it SHALL NOT use chip-tool cluster methods

#### Scenario: Barton commissions directly
- **WHEN** a test commissions a device
- **THEN** it SHALL use `default_environment.get_client().commission_device(device.get_commissioning_code(), timeout)`
- **AND** the commissioning code SHALL be the device's original code (no ECM window)

### Requirement: Light test uses side-band
`light_test.py` SHALL be updated to use the matter.js side-band interface instead of chip-tool cluster methods. It SHALL use `device.sideband.send("toggle")` and `device.sideband.get_state()` in place of `get_cluster(OnOffCluster.CLUSTER_ID).toggle()` and `get_cluster(OnOffCluster.CLUSTER_ID).is_on()`. The `requires_matterjs` marker SHALL be added.

#### Scenario: Light test toggle via side-band
- **WHEN** the test toggles the light
- **THEN** it SHALL use `matter_light.sideband.send("toggle")`
- **AND** it SHALL NOT use `OnOffCluster` or `get_cluster()`

#### Scenario: Light test state query via side-band
- **WHEN** the test queries the light state
- **THEN** it SHALL use `matter_light.sideband.get_state()` to check the `onOff` field
- **AND** it SHALL NOT use `OnOffCluster.is_on()`

### Requirement: MatterDoorLock fixture
A pytest fixture `matter_door_lock` SHALL provide a started matter.js door lock device, ready for commissioning by Barton in tests. The fixture SHALL simply start the device and yield it — no chip-tool commissioning step.

#### Scenario: Fixture provides started door lock
- **WHEN** a test function declares `matter_door_lock` as a parameter
- **THEN** the fixture SHALL start the door lock virtual device
- **AND** yield the `MatterDoorLock` instance
- **AND** clean up the device after the test completes
- **AND** the fixture SHALL NOT depend on `device_interactor`

### Requirement: Conditional test execution marker
A custom pytest marker `requires_matterjs` SHALL be available to mark tests that depend on Node.js and matter.js. Tests with this marker SHALL be automatically skipped when the runtime dependencies are not available.

#### Scenario: Dependencies available
- **WHEN** `node` is on the PATH and the matter.js package is installed
- **THEN** tests marked with `@pytest.mark.requires_matterjs` SHALL run normally

#### Scenario: Node.js not available
- **WHEN** `node` is not found on the PATH
- **THEN** tests marked with `@pytest.mark.requires_matterjs` SHALL be skipped with a message indicating Node.js is required

#### Scenario: matter.js not installed
- **WHEN** `node` is available but the matter.js package is not installed
- **THEN** tests marked with `@pytest.mark.requires_matterjs` SHALL be skipped with a message indicating matter.js is required

### Requirement: Docker environment setup
The Docker development image SHALL include Node.js 22.x, and the CI / development container tooling (e.g., entrypoint scripts or test setup) SHALL install matter.js v0.16.10 into the `testing/mocks/devices/matterjs` workspace so that matter.js virtual device tests work out of the box in CI and development containers.

#### Scenario: Node.js available in Docker
- **WHEN** a developer runs tests inside the Docker development container
- **THEN** `node` and `npm` SHALL be available on the PATH

#### Scenario: matter.js installed for virtual device tests
- **WHEN** the container runtime or test setup runs the matter.js installation step (for example, `npm --prefix testing/mocks/devices/matterjs install`) inside the Docker container
- **THEN** the matter.js package (v0.16.10) SHALL be installed in the `testing/mocks/devices/matterjs` workspace and available to the virtual device scripts

### Requirement: Reference door lock integration test
A reference integration test SHALL demonstrate the full lifecycle of commissioning, controlling, and verifying a matter.js virtual door lock through Barton APIs and the side-band interface. The test SHALL use the `matter_door_lock` fixture and be indistinguishable in structure from tests using other `MatterDevice` subclasses.

#### Scenario: Commission door lock through Barton
- **WHEN** the test commissions the virtual door lock using Barton's `commission_device` API
- **THEN** the device SHALL appear in `get_devices_by_device_class("doorLock")`
- **AND** the device SHALL have the expected common resources

#### Scenario: Lock device via Barton and verify via side-band
- **WHEN** the test executes the `lock` resource on the device via Barton (`execute_resource`)
- **THEN** the `locked` resource (boolean) SHALL reflect `true`
- **AND** querying the device's side-band state SHALL confirm the device is locked

#### Scenario: Unlock device via Barton and verify via side-band
- **WHEN** the test executes the `unlock` resource on the device via Barton (`execute_resource`)
- **THEN** the `locked` resource (boolean) SHALL reflect `false`
- **AND** querying the device's side-band state SHALL confirm the device is unlocked

#### Scenario: Side-band unlock triggers Barton resource update
- **WHEN** the test triggers a side-band `unlock` operation (simulating manual unlock)
- **THEN** the Barton client SHALL receive a resource updated event for the `locked` resource
- **AND** the `locked` resource value SHALL be `false`

#### Scenario: Side-band lock triggers Barton resource update
- **WHEN** the test triggers a side-band `lock` operation (simulating manual lock)
- **THEN** the Barton client SHALL receive a resource updated event for the `locked` resource
- **AND** the `locked` resource value SHALL be `true`

### Requirement: Virtual device base class initialization
The matter.js virtual device base class (`VirtualDevice`) SHALL initialize a Matter `ServerNode` with configurable vendor ID, product ID, device name, passcode, discriminator, and port. The class SHALL handle all common Matter device setup so that subclasses only need to define their device type and side-band operations.

#### Scenario: Base class creates a Matter server node
- **WHEN** a `VirtualDevice` subclass is instantiated with device type, vendor ID, product ID, passcode, discriminator, and port parameters
- **THEN** a Matter `ServerNode` SHALL be created with those parameters and the device SHALL be ready to accept commissioning

#### Scenario: Default values for optional parameters
- **WHEN** a `VirtualDevice` subclass is instantiated without specifying vendor ID, product ID, or port
- **THEN** the base class SHALL use sensible defaults (vendor ID `0xFFF1`, product ID `0x8000`, port `0` for dynamic assignment)

### Requirement: Side-band HTTP server
The `VirtualDevice` base class SHALL start an HTTP server on a dynamically assigned port that accepts side-band operation requests from test drivers. The server SHALL listen on `127.0.0.1` and accept JSON-encoded POST requests.

#### Scenario: Side-band server starts on dynamic port
- **WHEN** the virtual device starts
- **THEN** an HTTP server SHALL start on a dynamically assigned port (port 0)
- **AND** the actual assigned port SHALL be reported via stdout in a parseable format

#### Scenario: Side-band server accepts JSON POST requests
- **WHEN** a POST request is sent to `/sideband` with a JSON body containing an `operation` field
- **THEN** the server SHALL dispatch to the registered handler for that operation
- **AND** the response SHALL be JSON with `success` (boolean) and `result` (object) fields

#### Scenario: Side-band server rejects unknown operations
- **WHEN** a POST request is sent with an `operation` value that has no registered handler
- **THEN** the server SHALL respond with HTTP 400 and `{ "success": false, "error": "Unknown operation: <name>" }`

### Requirement: Side-band operation registration
The `VirtualDevice` base class SHALL provide a `registerOperation(name, handler)` method that subclasses use to register their device-specific side-band operations. Each handler SHALL be an async function that receives the request payload and returns a result object.

#### Scenario: Subclass registers a custom operation
- **WHEN** a subclass calls `registerOperation("myOp", handler)` during construction
- **THEN** POST requests with `{ "operation": "myOp" }` SHALL be dispatched to that handler

#### Scenario: Subclass registers multiple operations
- **WHEN** a subclass registers operations "opA" and "opB"
- **THEN** both operations SHALL be independently dispatchable via the side-band HTTP interface

### Requirement: Ready signal on stdout
The virtual device process SHALL emit a JSON ready signal on stdout when both the Matter server node and the side-band HTTP server are fully initialized and operational.

#### Scenario: Ready signal includes connection details
- **WHEN** the virtual device has completed initialization
- **THEN** it SHALL print a single JSON line to stdout containing `{ "ready": true, "sidebandPort": <port>, "matterPort": <port>, "passcode": <code>, "discriminator": <disc> }`

### Requirement: Graceful shutdown
The virtual device SHALL handle SIGTERM and SIGINT signals by cleanly shutting down the Matter server node and the side-band HTTP server.

#### Scenario: Process terminated with SIGTERM
- **WHEN** the virtual device process receives SIGTERM
- **THEN** the Matter server node SHALL be closed
- **AND** the side-band HTTP server SHALL be closed
- **AND** the process SHALL exit with code 0

### Requirement: Package configuration
A `package.json` SHALL exist in the virtual device source directory that declares matter.js v0.16.10 as a dependency and configures the project as an ES module.

#### Scenario: Package dependencies are correct
- **WHEN** `npm install` is run in the virtual device source directory
- **THEN** matter.js v0.16.10 SHALL be installed along with any required peer dependencies

### Requirement: Door lock device type
The matter.js door lock virtual device SHALL present itself as a Matter Door Lock device type (Device Type ID `0x000A`) with a DoorLock cluster (Cluster ID `0x0101`) on endpoint 1.

#### Scenario: Device advertises as door lock
- **WHEN** the door lock virtual device is started and commissioned
- **THEN** it SHALL be discoverable as a Matter Door Lock device type
- **AND** the DoorLock cluster SHALL be available on endpoint 1

### Requirement: Matter lock and unlock commands
The door lock virtual device SHALL respond to standard Matter DoorLock cluster `LockDoor` and `UnlockDoor` commands, updating its internal lock state accordingly.

#### Scenario: Lock door via Matter command
- **WHEN** a Matter `LockDoor` command is sent to the device
- **THEN** the device's lock state SHALL change to locked
- **AND** the device SHALL report the updated `LockState` attribute as `Locked` (value 1)

#### Scenario: Unlock door via Matter command
- **WHEN** a Matter `UnlockDoor` command is sent to the device
- **THEN** the device's lock state SHALL change to unlocked
- **AND** the device SHALL report the updated `LockState` attribute as `Unlocked` (value 2)

### Requirement: Side-band lock operation
The door lock SHALL register a side-band operation `lock` that simulates a user manually locking the device (e.g., turning the thumb-turn). This operation SHALL update the Matter cluster state, triggering standard Matter notifications.

#### Scenario: Side-band lock triggers Matter state change
- **WHEN** a side-band `lock` operation is sent to the device
- **THEN** the device's `LockState` attribute SHALL change to `Locked`
- **AND** connected Matter controllers SHALL receive a state change notification
- **AND** the side-band response SHALL be `{ "success": true, "result": { "lockState": "locked" } }`

### Requirement: Side-band unlock operation
The door lock SHALL register a side-band operation `unlock` that simulates a user manually unlocking the device. This operation SHALL update the Matter cluster state, triggering standard Matter notifications.

#### Scenario: Side-band unlock triggers Matter state change
- **WHEN** a side-band `unlock` operation is sent to the device
- **THEN** the device's `LockState` attribute SHALL change to `Unlocked`
- **AND** connected Matter controllers SHALL receive a state change notification
- **AND** the side-band response SHALL be `{ "success": true, "result": { "lockState": "unlocked" } }`

### Requirement: Matter lock and unlock commands emit LockOperation events
The door lock virtual device SHALL emit a `LockOperation` event (DoorLock cluster `0x0101`, event `0x0002`) whenever the lock state changes via a Matter `LockDoor` or `UnlockDoor` command, in addition to updating the `LockState` attribute. The `LockOperationType` field SHALL be `0` (Lock) for `LockDoor` and `1` (Unlock) for `UnlockDoor`. This is required so that Matter-command-driven lock/unlock operations are reflected in event-driven SBMD resources.

#### Scenario: LockDoor command emits LockOperation event
- **WHEN** a Matter `LockDoor` command is sent to the device
- **THEN** the device SHALL emit a `LockOperation` event with `LockOperationType` = 0 (Lock)
- **AND** the `LockState` attribute SHALL be updated to `Locked`

#### Scenario: UnlockDoor command emits LockOperation event
- **WHEN** a Matter `UnlockDoor` command is sent to the device
- **THEN** the device SHALL emit a `LockOperation` event with `LockOperationType` = 1 (Unlock)
- **AND** the `LockState` attribute SHALL be updated to `Unlocked`

### Requirement: Side-band operations emit LockOperation events
The door lock virtual device SHALL emit a `LockOperation` event (DoorLock cluster `0x0101`, event `0x0002`) whenever the lock state changes via a side-band operation, in addition to updating the `LockState` attribute. The `LockOperationType` field in the event SHALL be `0` (Lock) for side-band lock operations and `1` (Unlock) for side-band unlock operations. This is required for SBMD drivers that use event-driven resource updates rather than attribute subscription, such as the `door-lock.sbmd` spec with `mapper.event` on the `locked` resource.

#### Scenario: Side-band lock emits LockOperation event
- **WHEN** a side-band `lock` operation is sent to the virtual door lock
- **THEN** the device SHALL emit a `LockOperation` event with `LockOperationType` = 0 (Lock)
- **AND** the `LockState` attribute SHALL be updated to `Locked`

#### Scenario: Side-band unlock emits LockOperation event
- **WHEN** a side-band `unlock` operation is sent to the virtual door lock
- **THEN** the device SHALL emit a `LockOperation` event with `LockOperationType` = 1 (Unlock)
- **AND** the `LockState` attribute SHALL be updated to `Unlocked`

### Requirement: Side-band getState operation
The door lock SHALL register a side-band operation `getState` that returns the current state of the device, including lock state and any configured users/pin codes.

#### Scenario: Get state of locked device
- **WHEN** a side-band `getState` operation is sent and the device is locked
- **THEN** the response SHALL include `{ "success": true, "result": { "lockState": "locked", "users": [...], "pinCodes": [...] } }`

#### Scenario: Get state of unlocked device
- **WHEN** a side-band `getState` operation is sent and the device is unlocked
- **THEN** the response SHALL include `{ "success": true, "result": { "lockState": "unlocked", "users": [...], "pinCodes": [...] } }`

### Requirement: Initial lock state
The door lock virtual device SHALL start in the locked state by default.

#### Scenario: Device starts locked
- **WHEN** the door lock virtual device is started
- **THEN** the initial `LockState` attribute SHALL be `Locked`
- **AND** a side-band `getState` SHALL return `lockState: "locked"`

### Requirement: User and PIN code management via Matter
The door lock SHALL support basic user and PIN code management through the Matter DoorLock cluster, enabling tests to create users and set credentials.

#### Scenario: Set user credential via Matter
- **WHEN** a `SetCredential` command is sent via Matter with a PIN code
- **THEN** the credential SHALL be stored on the device
- **AND** a subsequent `getState` side-band operation SHALL include the credential in the `pinCodes` array

### Requirement: matter.js virtual thermostat device
A matter.js virtual thermostat device (`ThermostatDevice.js`) SHALL be created extending `VirtualDevice` with:
- Matter Thermostat device type (0x0301) on endpoint 1
- Thermostat cluster (0x0201) with initial state: LocalTemperature=2100 (21.00°C), OccupiedHeatingSetpoint=2000, OccupiedCoolingSetpoint=2600, SystemMode=Off
- Side-band operations: `setTemperature`, `setSystemMode`, `getState`

#### Scenario: Virtual thermostat starts with default state
- **WHEN** the virtual thermostat device is started
- **THEN** it SHALL be a Matter Thermostat device type (0x0301) with LocalTemperature=2100, OccupiedHeatingSetpoint=2000, OccupiedCoolingSetpoint=2600, SystemMode=Off

#### Scenario: Side-band set temperature
- **WHEN** the `setTemperature` side-band operation is called with a temperature value
- **THEN** the device SHALL update its LocalTemperature attribute to the specified value and the change SHALL be reported to subscribed controllers

#### Scenario: Side-band set system mode
- **WHEN** the `setSystemMode` side-band operation is called with a mode value (e.g., "heat")
- **THEN** the device SHALL update its SystemMode attribute accordingly

#### Scenario: Side-band get state
- **WHEN** the `getState` side-band operation is called
- **THEN** it SHALL return the current thermostat state including localTemperature, occupiedHeatingSetpoint, occupiedCoolingSetpoint, and systemMode

### Requirement: matter.js virtual thermostat with fan control device
A matter.js virtual thermostat with fan control device (`ThermostatWithFanDevice.js`) SHALL be created by subclassing `ThermostatDevice` to add:
- Fan Control cluster (0x0202) on the same thermostat endpoint, with initial FanMode=Auto
- Additional side-band operations: `setFanMode`, `getFanState`

The subclass SHALL reuse the base thermostat endpoint setup (Thermostat cluster, initial state, core side-band operations) and only add the Fan Control cluster and fan-related side-band operations.

#### Scenario: Virtual thermostat with fan starts with fan control cluster
- **WHEN** the virtual thermostat with fan device is started
- **THEN** it SHALL include both the Thermostat cluster (0x0201) and Fan Control cluster (0x0202) on the thermostat endpoint

#### Scenario: Side-band set fan mode
- **WHEN** the `setFanMode` side-band operation is called with a fan mode value
- **THEN** the device SHALL update its FanMode attribute on the Fan Control cluster

#### Scenario: Side-band get fan state
- **WHEN** the `getFanState` side-band operation is called
- **THEN** it SHALL return the current fan control state including fanMode

### Requirement: Python test fixture for Matter thermostat
A Python test fixture (`matter_thermostat.py`) SHALL be created as a `MatterDevice` subclass with:
- `device_class` set to `"thermostat"`
- `matterjs_entry_point` set to `"ThermostatDevice.js"`
- A pytest fixture function `matter_thermostat` that creates, starts, yields, and cleans up the device

#### Scenario: Fixture creates and starts thermostat
- **WHEN** the `matter_thermostat` pytest fixture is used in a test
- **THEN** it SHALL yield a started `MatterThermostat` instance ready for commissioning

### Requirement: Python test fixture for Matter thermostat with fan control
A Python test fixture (`matter_thermostat_with_fan.py`) SHALL be created by subclassing `MatterThermostat` to avoid duplicating common setup:
- `matterjs_entry_point` set to `"ThermostatWithFanDevice.js"`
- A pytest fixture function `matter_thermostat_with_fan` that creates, starts, yields, and cleans up the device

#### Scenario: Fixture creates and starts thermostat with fan
- **WHEN** the `matter_thermostat_with_fan` pytest fixture is used in a test
- **THEN** it SHALL yield a started `MatterThermostatWithFan` instance ready for commissioning, with Fan Control cluster support

### Requirement: Commission test
An integration test SHALL verify that a Matter thermostat can be commissioned and appears as a `thermostat` device class with the expected common resources.

#### Scenario: Commission thermostat
- **WHEN** a virtual Matter thermostat is commissioned via Barton
- **THEN** the device SHALL appear with deviceClass `thermostat` and have common resources (firmwareVersionString, macAddress, networkType, serialNumber)

### Requirement: Setpoint read/write test
Integration tests SHALL verify that heating and cooling setpoints can be read after commissioning and written via the Barton resource API.

#### Scenario: Write heating setpoint via Barton
- **WHEN** a client writes a new value to the `heatSetpoint` resource
- **THEN** the virtual thermostat device SHALL reflect the new heating setpoint value via side-band query

#### Scenario: Write cooling setpoint via Barton
- **WHEN** a client writes a new value to the `coolSetpoint` resource
- **THEN** the virtual thermostat device SHALL reflect the new cooling setpoint value via side-band query

### Requirement: System mode read/write test
An integration test SHALL verify that the system mode can be read and changed via the Barton resource API.

#### Scenario: Write system mode via Barton
- **WHEN** a client writes `"heat"` to the `systemMode` resource
- **THEN** the virtual thermostat device SHALL report its system mode as "heat" via side-band query

### Requirement: Side-band temperature change triggers Barton update
An integration test SHALL verify that a temperature change initiated via the side-band interface triggers a resource update in Barton.

#### Scenario: Side-band temperature change
- **WHEN** the virtual thermostat's local temperature is changed via the side-band interface
- **THEN** Barton SHALL receive a resource update for the `localTemperature` resource with the new value

### Requirement: Base thermostat tests verify no fan resources
The base `thermostat_test.py` tests SHALL verify that `fanMode` and `fanOn` resources are NOT present on a thermostat without the Fan Control cluster, confirming the prerequisite gate works correctly.

#### Scenario: No fan resources on base thermostat
- **WHEN** a virtual Matter thermostat without Fan Control cluster is commissioned
- **THEN** the `fanMode` and `fanOn` resources SHALL NOT be present on the thermostat endpoint

### Requirement: Commission thermostat with fan control test
An integration test in `thermostat_with_fan_test.py` SHALL verify that a Matter thermostat with Fan Control cluster can be commissioned and includes both base thermostat resources and fan control resources (`fanMode`, `fanOn`).

#### Scenario: Commission thermostat with fan control
- **WHEN** a virtual Matter thermostat with Fan Control cluster is commissioned via Barton
- **THEN** the device SHALL appear with deviceClass `thermostat` and have fan resources (`fanMode`, `fanOn`) in addition to all base thermostat resources

### Requirement: Fan mode read test
An integration test SHALL verify that the `fanMode` resource can be read on a thermostat with Fan Control cluster support.

#### Scenario: Read fan mode
- **WHEN** a thermostat with Fan Control cluster is commissioned
- **THEN** the `fanMode` resource SHALL be readable with an initial value of `"auto"`

### Requirement: Fan on read test
An integration test SHALL verify that the `fanOn` resource can be read on a thermostat with Fan Control cluster support.

#### Scenario: Read fan on
- **WHEN** a thermostat with Fan Control cluster is commissioned
- **THEN** the `fanOn` resource SHALL be readable with an initial value of `"false"`

### Requirement: Sideband client class
A Python `SidebandClient` class SHALL provide a simple interface for sending side-band operations to matter.js virtual devices over HTTP. The client SHALL be initialized with the device's side-band host and port.

#### Scenario: Client sends an operation
- **WHEN** `client.send("lock")` is called
- **THEN** the client SHALL send a POST request to `http://<host>:<port>/sideband` with body `{ "operation": "lock" }`
- **AND** the response SHALL be parsed as JSON and returned

#### Scenario: Client sends an operation with parameters
- **WHEN** `client.send("setPin", {"pin": "1234", "userId": 1})` is called
- **THEN** the client SHALL send `{ "operation": "setPin", "params": {"pin": "1234", "userId": 1} }`
- **AND** the response SHALL be parsed as JSON and returned

### Requirement: Convenience methods
The `SidebandClient` SHALL provide a `get_state()` convenience method that calls the `getState` side-band operation and returns the parsed result.

#### Scenario: Get state returns device state
- **WHEN** `client.get_state()` is called
- **THEN** it SHALL send a `getState` operation and return the `result` field from the response

### Requirement: Error handling
The `SidebandClient` SHALL raise a clear exception when a side-band operation fails or the device is unreachable.

#### Scenario: Device unreachable
- **WHEN** a side-band operation is sent and the device process is not running
- **THEN** the client SHALL raise a `ConnectionError` (or subclass) with a descriptive message

#### Scenario: Operation returns failure
- **WHEN** a side-band operation returns `{ "success": false, "error": "..." }`
- **THEN** the client SHALL raise a `SidebandOperationError` with the error message

### Requirement: Timeout support
The `SidebandClient` SHALL support configurable timeouts for side-band operations, defaulting to a reasonable value (e.g., 5 seconds).

#### Scenario: Operation times out
- **WHEN** a side-band operation does not receive a response within the timeout period
- **THEN** the client SHALL raise a timeout exception
