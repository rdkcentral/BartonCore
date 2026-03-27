## ADDED Requirements

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
A `MatterDoorLock` Python class SHALL be a standard `MatterDevice` subclass in `testing/mocks/devices/matter/`. It SHALL specify `matterjs_entry_point="door-lock-device.js"` and `device_class="doorLock"`.

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
`MatterLight` SHALL be migrated from `chip-lighting-app` to a matter.js virtual light device. It SHALL specify `matterjs_entry_point="light-device.js"`. Cluster registration calls (`_register_cluster`) and cluster imports SHALL be removed since the cluster class system is eliminated. The `matter_light` pytest fixture SHALL remain unchanged in its interface.

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
The Docker development image SHALL include Node.js 22.x and matter.js v0.16.10 pre-installed so that matter.js virtual device tests work out of the box in CI and development containers.

#### Scenario: Node.js available in Docker
- **WHEN** a developer runs tests inside the Docker development container
- **THEN** `node` and `npm` SHALL be available on the PATH

#### Scenario: matter.js installed in Docker
- **WHEN** a developer runs matter.js virtual device tests inside the Docker container
- **THEN** the matter.js package (v0.16.10) SHALL be available to the virtual device scripts

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
