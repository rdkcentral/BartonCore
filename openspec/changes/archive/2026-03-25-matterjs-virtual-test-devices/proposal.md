## Why

BartonCore's integration test suite currently uses CHIP SDK sample apps (e.g., `chip-lighting-app`) as virtual Matter devices, controlled through the `chip-tool` CLI. While this enables basic commissioning and cluster-level interaction, it has a fundamental limitation: **there is no way to simulate a user physically interacting with the device** (e.g., turning a thumb-lock). Tests cannot verify that BartonCore correctly processes unsolicited state-change notifications from devices. Additionally, the CHIP SDK sample apps are opaque C++ binaries with fixed behavior — adding new device types or customizing device behavior requires building and maintaining separate C++ applications.

By replacing the CHIP SDK sample apps with matter.js-based virtual devices that include a side-band control channel, tests gain full programmability over the device side. Tests can simulate user interactions, verify state-change notification flows end-to-end, and new device types can be added with minimal JavaScript boilerplate — closing a critical gap in test coverage for the Matter subsystem.

## What Changes

- **Docker environment**: Install Node.js and matter.js (v0.16.10) in the development Docker image to provide the runtime for virtual Matter devices.
- **Virtual device framework (JavaScript/matter.js)**: Create a base virtual device class in JavaScript that handles Matter device initialization, side-band HTTP control channel setup, and extensible operation dispatch. Concrete device subclasses register their specific side-band operations (e.g., lock/unlock for door locks).
- **Python test client library**: Add a Python side-band client class under `testing/` that simplifies sending control commands and state queries to virtual devices from pytest tests.
- **MatterDevice refactor**: Remove all CHIP SDK and chip-tool support from `MatterDevice`. All devices are now backed by matter.js virtual devices — the `_app_name` attribute, chip-app subprocess code, chip-tool cluster class system (`_register_cluster`, `get_cluster`, `_cluster_classes`), and chip-tool interactor integration (`_set_interactor`, `_interactor`, `_chip_tool_node_id`) are all removed. `MatterDevice.start()` exclusively manages Node.js subprocesses with ready signal parsing and side-band client setup.
- **Remove chip-tool entirely**: Delete `device_interactor.py` (`ChipToolDeviceInteractor`) and the entire `testing/mocks/devices/matter/clusters/` directory. chip-tool was previously used for two purposes: (1) commissioning devices so chip-tool could send cluster commands as a form of side-band control, and (2) wrapping those cluster commands in Python classes. Both are superseded — side-band control is now via HTTP to the matter.js process, and Barton commissions devices directly in tests via `commission_device()`. There is no remaining need for chip-tool in the test infrastructure.
- **MatterLight migration**: Migrate `MatterLight` from `chip-lighting-app` to a matter.js virtual light device, following the same pattern as the new door lock. Remove cluster registration calls and imports.
- **Update light test**: Migrate `light_test.py` from chip-tool cluster methods (`OnOffCluster.toggle()`, `OnOffCluster.is_on()`) to the matter.js side-band interface (`device.sideband.send("toggle")`, `device.sideband.get_state()`). Add the `requires_matterjs` marker.
- **Simplify fixtures**: Device fixtures no longer depend on `device_interactor` — they simply start the matter.js device and yield it. Tests commission via Barton using the device's original commissioning code. No chip-tool commissioning step, no ECM window.
- **Reference door lock device**: Implement a concrete matter.js virtual door lock as a standard `MatterDevice` subclass in the existing mock device hierarchy, with side-band operations for lock, unlock, and state retrieval.
- **Reference door lock test**: Create an integration test that commissions the virtual door lock through Barton APIs, controls it via Matter, verifies state through the side-band channel, and triggers side-band state changes to verify Barton resource updates.
- **Conditional test execution**: Gate matter.js-dependent tests on runtime availability of Node.js and matter.js, so builds without these dependencies skip gracefully.

## Non-goals

- Exhaustive Matter device type coverage — only the door lock and light are implemented; other device types will follow the established pattern in future changes.
- Production use of matter.js — this is strictly test infrastructure.
- Modifying the BartonCore C/C++ source, public API, or SBMD driver system.
- Adding new CMake feature flags — this change operates at the test infrastructure and Docker layers only.

## Capabilities

### New Capabilities
- `matterjs-virtual-device-framework`: Base matter.js virtual device class with side-band HTTP control channel and extensible operation dispatch for test devices.
- `matterjs-door-lock-device`: Reference matter.js virtual door lock implementation with lock/unlock/getState side-band operations.
- `python-sideband-client`: Python client library for sending side-band commands to virtual matter.js devices from integration tests.
- `matter-test-infrastructure`: Simplified pytest fixtures, side-band helpers, and conditional execution support for matter.js-based integration tests.

### Modified Capabilities

- `matter-device-class`: `MatterDevice` is refactored to exclusively use matter.js virtual devices, removing all CHIP SDK and chip-tool support — the chip-tool cluster class system, `device_interactor.py`, and all interactor integration code. All subclasses (including `MatterLight`) are migrated to matter.js. Fixtures are simplified (no chip-tool commissioning step). Tests interact with devices via the Barton API and the side-band channel.

## Impact

- **Docker image (`docker/Dockerfile`)**: New layer installing Node.js 22.x LTS and matter.js v0.16.10 via npm. Image size will increase.
- **Testing infrastructure (`testing/`)**: New Python modules for side-band client and a door lock device in the existing mock device hierarchy. `MatterDevice` refactored to remove CHIP SDK sample app support.
- **New JavaScript source (`testing/mocks/devices/matterjs/`)**: matter.js virtual device base class, door lock, and light implementations.
- **Device classes (`testing/mocks/devices/matter/`)**: `MatterDevice` simplified to matter.js-only backend. Chip-tool cluster class directory (`clusters/`) removed entirely. `device_interactor.py` removed entirely — chip-tool is no longer used in any capacity. `MatterLight` migrated from `chip-lighting-app` to matter.js.
- **Fixtures (`matter_light`, `matter_door_lock`)**: Simplified — no longer depend on `device_interactor` fixture. Just start device, yield, cleanup. Tests commission via Barton directly.
- **conftest.py**: `device_interactor` plugin registration removed.
- **Light test (`testing/test/light_test.py`)**: Migrated from chip-tool cluster methods to side-band calls. Added `requires_matterjs` marker.
- **CI (`scripts/ci/`)**: Integration test runner may need awareness of Node.js availability for conditional test execution.
- **Dependencies**: Node.js 22.x (runtime), matter.js v0.16.10 (npm package) — both confined to the test/Docker environment.
- **Relevant CMake flags**: `BCORE_MATTER` (must be ON for Matter tests to be meaningful), though no new flags are introduced.
