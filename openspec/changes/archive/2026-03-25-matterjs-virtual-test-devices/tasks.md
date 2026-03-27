## 1. Docker Environment Setup

- [x] 1.1 Add Node.js 22.x LTS installation to `docker/Dockerfile` (apt-get install or nodesource setup) — **requires Docker rebuild**
- [x] 1.2 Add matter.js v0.16.10 npm install step to `docker/Dockerfile` — install into `testing/mocks/devices/matterjs/` at build time — **requires Docker rebuild**
- [x] 1.3 Verify Node.js and npm are available on PATH inside the container and matter.js is installed

## 2. matter.js Virtual Device Base Class (JavaScript)

- [x] 2.1 Create `testing/mocks/devices/matterjs/package.json` declaring matter.js v0.16.10 dependency with `"type": "module"`
- [x] 2.2 Create `testing/mocks/devices/matterjs/src/VirtualDevice.js` — base class with Matter `ServerNode` initialization, configurable parameters (vendor ID, product ID, passcode, discriminator, port, device name, device type)
- [x] 2.3 Implement side-band HTTP server in `VirtualDevice` — listens on dynamic port, dispatches POST `/sideband` requests by operation name to registered handlers
- [x] 2.4 Implement `registerOperation(name, handler)` method for subclass operation registration
- [x] 2.5 Implement ready signal — emit JSON `{ "ready": true, "sidebandPort": ..., "matterPort": ..., "passcode": ..., "discriminator": ... }` on stdout after initialization
- [x] 2.6 Implement graceful shutdown on SIGTERM/SIGINT — close Matter server node and HTTP server
- [x] 2.7 Test base class manually: create a minimal test device, start it, send a side-band request, verify response, stop it

## 3. matter.js Door Lock Virtual Device (JavaScript)

- [x] 3.1 Create `testing/mocks/devices/matterjs/src/DoorLockDevice.js` — extends `VirtualDevice` with DoorLock device type and DoorLock cluster on endpoint 1
- [x] 3.2 Register side-band `lock` operation — update DoorLock cluster `LockState` attribute to Locked, return `{ "lockState": "locked" }`
- [x] 3.3 Register side-band `unlock` operation — update DoorLock cluster `LockState` attribute to Unlocked, return `{ "lockState": "unlocked" }`
- [x] 3.4 Register side-band `getState` operation — return current lock state, users, and pin codes as JSON
- [x] 3.5 Set initial lock state to Locked on startup
- [x] 3.6 Entry point logic inlined into `DoorLockDevice.js` via `import.meta.url` guard (separate `door-lock-device.js` removed)
- [x] 3.7 Test door lock manually: start device, commission with chip-tool, send lock/unlock via chip-tool and side-band, verify state changes

## 4. Python Side-band Client

- [x] 4.1 Create `testing/helpers/matterjs/sideband_client.py` — `SidebandClient` class with `send(operation, params)` method using `urllib.request` for HTTP POST
- [x] 4.2 Implement `get_state()` convenience method
- [x] 4.3 Implement error handling — `SidebandOperationError` for failed operations, `ConnectionError` for unreachable devices
- [x] 4.4 Implement configurable timeout support (default 5 seconds)
- [x] 4.5 Create `testing/helpers/matterjs/__init__.py`
- [x] 4.6 Unit test `SidebandClient` — test send, get_state, error handling, and timeout behavior

## 5. MatterDevice Refactor and Device Classes

- [x] 5.1 Create `testing/mocks/devices/matter/matter_door_lock.py` — `MatterDoorLock` class as a `MatterDevice` subclass. Specify `matterjs_entry_point="DoorLockDevice.js"` and `device_class="doorLock"`.
- [x] 5.2 Implement matter.js subprocess management in `MatterDevice` — `start()` spawns Node.js, parses JSON ready signal from stdout, and configures `SidebandClient`. Expose via `device.sideband` property.
- [x] 5.3 Integrate `SidebandClient` into `MatterDevice` — auto-configure from parsed ready signal.
- [x] 5.4 Create pytest fixture `matter_door_lock` — starts device, commissions with `ChipToolDeviceInteractor`, opens ECM window, yields device, cleans up.
- [x] 5.5 Register fixture in `testing/conftest.py` via `pytest_plugins`
- [x] 5.6 Remove parallel class tree — remove `testing/mocks/devices/matterjs/matter_js_device.py` and `testing/mocks/devices/matterjs/matter_js_door_lock.py`
- [x] 5.7 Remove CHIP SDK sample app support from `MatterDevice` — remove `_app_name` attribute, `_start_chip_app()` method, and the `app_name` constructor parameter. `MatterDevice.__init__()` takes `matterjs_entry_point` as a required parameter. `start()` exclusively calls `_start_matterjs()`.
- [x] 5.8 Create `testing/mocks/devices/matterjs/src/LightDevice.js` — matter.js virtual light device extending `VirtualDevice` with OnOff, LevelControl, and ColorControl clusters on endpoint 1. Register side-band operations: `on`, `off`, `toggle`, `getState`.
- [x] 5.9 Entry point logic inlined into `LightDevice.js` via `import.meta.url` guard (separate `light-device.js` removed)
- [x] 5.10 Migrate `MatterLight` from `chip-lighting-app` to matter.js — change from `app_name="chip-lighting-app"` to `matterjs_entry_point="LightDevice.js"`. Remove `mdns_port` parameter. Verify `matter_light` fixture still works.
- [x] 5.11 Verify `MatterLight` migration — run existing light tests to confirm no regressions.

## 6. Conditional Test Execution

- [x] 6.1 Implement `requires_matterjs` pytest marker — check `shutil.which("node")` and matter.js package availability at collection time
- [x] 6.2 Register the marker in `testing/conftest.py` or an appropriate conftest
- [x] 6.3 Test marker behavior: verify tests skip when Node.js is absent, and run when present

## 7. Reference Door Lock Integration Test

- [x] 7.1 Create `testing/test/door_lock_test.py` with `@pytest.mark.requires_matterjs` marker
- [x] 7.2 Implement `test_commission_door_lock` — commission virtual door lock via Barton API, verify it appears in `get_devices_by_device_class("doorLock")`, assert common resources
- [x] 7.3 Implement `test_lock_unlock_via_barton` — execute `lock` and `unlock` resources via Barton (`execute_resource`), verify `locked` resource (boolean) updates, verify via side-band
- [x] 7.4 Implement `test_sideband_unlock_triggers_barton_update` — send side-band `unlock` (simulating manual unlock), verify Barton client receives resource updated event with `locked` = `false`
- [x] 7.5 Implement `test_sideband_lock_triggers_barton_update` — send side-band `lock` (simulating manual lock), verify Barton client receives resource updated event with `locked` = `true`
- [x] 7.6 Fix `door_lock_test.py` — the test was never run end-to-end (only verified via `pytest --collect-only`). Fix incorrect resource model usage: change `"lockState"` resource references to `"locked"` (boolean), replace `write_resource()` with `execute_resource()` for `lock`/`unlock` function resources, update resource value assertions from string (`"locked"`/`"unlocked"`) to boolean (`true`/`false`)
- [x] 7.7 Run door lock integration tests end-to-end to verify they actually pass — **requires Docker**

## 8. Remove Chip-tool and Migrate Tests to Side-band

- [x] 8.1 Delete `testing/mocks/devices/matter/device_interactor.py` — chip-tool is no longer used. Previously chip-tool commissioned devices to send cluster commands (old side-band); now side-band is HTTP and Barton commissions directly.
- [x] 8.2 Delete the `testing/mocks/devices/matter/clusters/` directory — remove `matter_cluster.py`, `onoff_cluster.py`, `levelcontrol_cluster.py`, `colorcontrol_cluster.py`, and the `__pycache__/` subdirectory
- [x] 8.3 Remove all chip-tool and cluster infrastructure from `MatterDevice` — remove `_register_cluster()` method, `get_cluster()` method, `_cluster_classes` attribute, `_set_interactor()` method, `_interactor` attribute, `_chip_tool_node_id` attribute, `set_commissioning_code()` method (no longer needed without ECM), and imports of `MatterCluster`/`ClusterId`/`ChipToolDeviceInteractor`
- [x] 8.4 Remove cluster imports and `_register_cluster()` calls from `MatterLight`
- [x] 8.5 Simplify `matter_door_lock` fixture — remove `device_interactor` parameter, remove `device_interactor.register_device()` call. Fixture just starts device and yields it.
- [x] 8.6 Simplify `matter_light` fixture — remove `device_interactor` parameter, remove `device_interactor.register_device()` call. Fixture just starts device and yields it.
- [x] 8.7 Remove `device_interactor` plugin from `conftest.py` — remove the `testing.mocks.devices.matter.device_interactor` entry from `pytest_plugins`
- [x] 8.8 Update `light_test.py` — replace `matter_light.get_cluster(OnOffCluster.CLUSTER_ID).is_on()` with `matter_light.sideband.get_state()["onOff"]`, replace `matter_light.get_cluster(OnOffCluster.CLUSTER_ID).toggle()` with `matter_light.sideband.send("toggle")`, remove `OnOffCluster` import, add `pytestmark = pytest.mark.requires_matterjs`
- [x] 8.9 Remove stale untracked files — delete `testing/mocks/devices/matterjs/doorlock_stderr.log` (leftover debug log) and `testing/mocks/devices/matterjs/__pycache__/` (stale bytecode from previously deleted `matter_js_device.py` and `matter_js_door_lock.py`)
- [x] 8.10 Verify all tests collect and no import errors — run `pytest --collect-only testing/test/light_test.py testing/test/door_lock_test.py`
