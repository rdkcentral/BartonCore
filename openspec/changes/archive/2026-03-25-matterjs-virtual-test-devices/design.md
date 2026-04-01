## Context

BartonCore's integration test suite uses a well-established pattern: pytest fixtures orchestrate a Barton client (`DefaultEnvironmentOrchestrator`) and mock Matter devices run as subprocesses.

The previous approach had several limitations:

1. **No user-side simulation**: There was no way to simulate a user physically interacting with the device (e.g., turning a thumb-lock). Tests could not verify that BartonCore correctly processes unsolicited state-change notifications from devices.
2. **Opaque device behavior**: CHIP SDK sample apps were pre-built C++ binaries with fixed behavior. Adding new device types or customizing interactions required building and maintaining separate C++ applications.
3. **Chip-tool coupling**: The entire test infrastructure was coupled to chip-tool — chip-tool commissioned devices first (so it could send cluster commands as a form of side-band control), then opened an ECM window so Barton could also commission. The Python cluster classes (`OnOffCluster`, `LevelControlCluster`, `ColorControlCluster`) wrapped chip-tool CLI for these interactions. This was an unnecessary multi-admin complexity.

matter.js — a JavaScript Matter SDK — solves all three problems. Virtual devices built with matter.js are fully programmable, and a side-band HTTP channel enables tests to trigger state changes on the device side (simulating user interaction) and query device state independently of Matter.

This change introduces matter.js as the **replacement** for CHIP SDK sample apps in the test infrastructure. The `MatterDevice` class is refactored to exclusively use matter.js — all CHIP SDK and chip-tool code is removed entirely: the `_app_name` attribute, chip-app subprocess code, cluster class system, and `device_interactor.py`. Barton commissions devices directly using the device's original commissioning code — no chip-tool commissioning step, no ECM window. The existing `MatterDevice` class hierarchy and pytest fixture pattern are preserved.

### Architecture

```
┌──────────────┐     ┌─────────────────────┐     ┌───────────────────────────┐
│  pytest test  │     │    Barton Client     │     │  matter.js virtual device  │
│              │────▶│  (GObject/BCore API) │────▶│  (Node.js process)        │
│              │     │                     │◀────│    ┌──────────────────┐   │
│              │     └─────────────────────┘     │    │ Matter endpoint   │   │
│              │                                 │    └──────────────────┘   │
│              │                                 │    ┌──────────────────┐   │
│              │                                 │    │ Side-band HTTP   │   │
│              │                                 │    │ server (:PORT)   │   │
│              │                                 │    └────────▲─────────┘   │
│              │                                              │            │
│              │          device.sideband.send("unlock")       │            │
└──────────────┘──────────────────────────────────────────────┘            │
                                                  └───────────────────────────┘
```

All virtual devices are matter.js Node.js processes. `MatterDevice` manages:
- Node.js subprocess lifecycle (start, ready signal, shutdown)
- Side-band communication via `SidebandClient` (exposed as `device.sideband`)
- Commissioning credentials (passcode, discriminator, commissioning code)

Tests interact with devices through two channels:
1. **Barton API** (via `default_environment.get_client()`) for Matter protocol commands from the controller side. Barton commissions devices directly using the device's original commissioning code.
2. **Side-band** (via `device.sideband.send()`) for simulating user-side device interactions

chip-tool is not used. There is no `ChipToolDeviceInteractor`, no chip-tool commissioning step, and no ECM window.

## Goals / Non-Goals

**Goals:**
- Replace CHIP SDK sample apps with matter.js virtual devices for all virtual test devices
- Remove CHIP SDK sample app support from `MatterDevice` entirely — no dual-backend
- Migrate `MatterLight` from `chip-lighting-app` to a matter.js virtual light
- Enable integration tests to simulate user-side device interactions (side-band operations) on virtual Matter devices
- Provide an extensible framework where new virtual device types require minimal boilerplate (register side-band operations in a subclass)
- Create a reference door lock virtual device and integration test as a working example
- Gate matter.js tests on runtime dependency availability so non-Docker targets skip gracefully

**Non-Goals:**
- Modifying the BartonCore C/C++ source, public API, or SBMD drivers
- Supporting matter.js devices outside of the test environment
- Full Matter specification compliance for virtual devices — only the clusters/features needed for testing
- Adding new CMake feature flags — this operates at the test/Docker layer only

## Decisions

### 1. Side-band Transport: HTTP/JSON

**Decision**: Use a lightweight HTTP server (built into Node.js) inside each virtual device process for the side-band channel.

**Rationale**:
- Node.js has a built-in `http` module — no additional dependencies
- HTTP is synchronous request/response, natural for "do X, get result" test operations
- JSON is already the lingua franca between test components
- Python's `urllib` or `requests` can call it trivially
- Port is dynamically assigned (port 0) to avoid conflicts when running multiple devices

**Alternatives considered**:
- **Unix domain sockets**: Lower overhead but harder to debug, no browser-based testing
- **WebSocket**: Adds complexity for what are fundamentally request/response operations
- **stdin/stdout IPC**: Used by chip-tool but fragile with matter.js's own logging
- **TCP raw sockets**: Requires custom framing protocol; HTTP provides this for free

### 2. Virtual Device Process Model: Node.js Subprocess

**Decision**: Each virtual matter.js device runs as a separate Node.js subprocess. `MatterDevice.start()` spawns Node.js, waits for the ready signal, and configures the side-band client.

**Rationale**:
- Process isolation prevents matter.js state from leaking between tests
- Barton commissions the device directly via its Matter commissioning code — no intermediate chip-tool step
- `MatterDevice` owns the full subprocess lifecycle: spawn, ready signal, teardown

### 3. MatterDevice: matter.js-only backend

**Decision**: `MatterDevice` exclusively uses matter.js virtual devices. All CHIP SDK and chip-tool code is removed: the `_app_name` attribute, chip-app subprocess code, cluster class system (`_register_cluster`, `get_cluster`, `_cluster_classes`), and interactor integration (`_set_interactor`, `_interactor`, `_chip_tool_node_id`). All subclasses specify a `matterjs_entry_point` — the path to a JavaScript file under `testing/mocks/devices/matterjs/src/`. `MatterDevice.start()` spawns Node.js, parses the ready signal, and configures the `SidebandClient`.

**Rationale**:
- No dual-backend complexity — one code path for all devices
- `MatterDevice` subclasses are simple: specify `matterjs_entry_point` and `device_class`
- The `SidebandClient` is composed into `MatterDevice` and auto-configured from the ready signal, exposed via `device.sideband`
- Existing fixture and test patterns are preserved — `MatterLight` and `MatterDoorLock` both follow the same subclass pattern

### 4. Remove chip-tool entirely — Barton commissions directly

**Decision**: Delete `device_interactor.py` (`ChipToolDeviceInteractor`) entirely. Remove the `testing/mocks/devices/matter/clusters/` directory. Remove `_set_interactor()`, `_interactor`, and `_chip_tool_node_id` from `MatterDevice`. Remove the `device_interactor` plugin registration from `conftest.py`. Fixtures no longer depend on `device_interactor` — they simply start the matter.js device and yield it.

**Rationale**:
- chip-tool was previously used for two purposes: (1) commissioning devices so chip-tool could send cluster commands (the old side-band mechanism), and (2) opening an ECM window so Barton could commission as a second admin
- With matter.js side-band, chip-tool no longer needs to be a controller. There is no need for cluster commands via chip-tool.
- Without chip-tool as first commissioner, there is no need for ECM multi-admin — Barton commissions devices directly using the device's original commissioning code
- This eliminates the entire chip-tool dependency from the test infrastructure, simplifying fixtures and removing a complex commissioning dance
- The cluster classes (`OnOffCluster`, etc.) wrapped chip-tool CLI and have no callers once chip-tool is removed

### 5. Side-band Operation Dispatch: Registration Pattern

**Decision**: The JavaScript base class (`VirtualDevice`) provides a `registerOperation(name, handler)` method. Subclasses call it in their constructor to register device-specific operations. The HTTP handler dispatches by operation name.

**Rationale**:
- Matches the proposal's requirement for "a simple way for each concrete subclass to specify their unique side-band operations"
- No switch/case sprawl — adding a new operation is one `registerOperation()` call
- Operations can be async (Matter cluster state changes may involve async SDK calls)

**Wire format**:
```
POST /sideband
Content-Type: application/json

{ "operation": "lock" }

Response: 200 OK
{ "success": true, "result": { ... } }
```

### 6. JavaScript Source Location: `testing/mocks/devices/matterjs/`

**Decision**: Place matter.js virtual device JavaScript source under `testing/mocks/devices/matterjs/`.

**Rationale**:
- JavaScript source is kept separate from the Python mock device hierarchy since it's a different language
- `package.json` lives in this directory to scope the npm dependency
- Python device classes (e.g., `MatterDoorLock`) live in `testing/mocks/devices/matter/` alongside `MatterLight`, following the existing hierarchy

### 7. Dependency Gating: Runtime Check via pytest marker

**Decision**: Use a custom pytest marker (`@pytest.mark.requires_matterjs`) and a `conftest.py` hook that checks for `node` and the matter.js package at collection time, skipping marked tests if unavailable.

**Rationale**:
- pytest markers are idiomatic and well-understood
- Runtime check (not build-time) aligns with the existing test infrastructure which doesn't use CMake for integration tests
- `shutil.which("node")` + subprocess check for matter.js package availability is simple and reliable

### 8. matter.js Version: v0.16.10 (pinned)

**Decision**: Pin to matter.js v0.16.10 via `package.json` and install in Dockerfile.

**Rationale**:
- User-specified version requirement
- Pinning prevents surprise breakage from upstream changes
- npm's `package-lock.json` ensures reproducible installs

## Risks / Trade-offs

**[matter.js API instability]** → matter.js is pre-1.0; APIs may change between versions. **Mitigation**: Version is pinned at v0.16.10. Base class abstracts matter.js internals so only one file needs updating if the API changes.

**[Docker image size increase]** → Node.js 22.x adds ~100MB to the image. **Mitigation**: Node.js is installed in the existing Docker image which is already large (CHIP SDK, OTBR, etc.). The incremental cost is acceptable for test infrastructure.

**[Port conflicts]** → Multiple virtual devices running simultaneously could conflict. **Mitigation**: Both the Matter port and side-band HTTP port use dynamic assignment (port 0), and the side-band server reports its actual port via stdout on startup for the Python harness to capture.

**[Side-band server startup race]** → Python test may try to connect before the HTTP server is ready. **Mitigation**: `MatterDevice.start()` blocks until the ready signal (including the side-band port number) is received on stdout before returning.

**[Thread safety with GLib main loop]** → The Barton client runs a GLib main loop; test code interacts from the pytest thread. **Mitigation**: This is already handled by the existing orchestrator infrastructure — the same patterns apply. The side-band client is synchronous HTTP from the test thread and doesn't touch the GLib loop.

**[No backward compatibility concerns]** → `MatterLight` is migrated from `chip-lighting-app` to matter.js as part of this change. The test interface (`matter_light` fixture) remains identical. No changes to test code that uses the fixture.

## Open Questions

- Should `npm install` for matter.js happen at Docker build time (in Dockerfile) or at test runtime? Docker build time is preferred for reproducibility and CI speed, but runtime install allows flexibility for non-Docker environments. **Recommendation**: Docker build time, with a fallback check at runtime.
- What Node.js version to install? The proposal doesn't specify. **Recommendation**: Node.js 22.x LTS as it's the current active LTS line and matter.js v0.16.10 supports it.
