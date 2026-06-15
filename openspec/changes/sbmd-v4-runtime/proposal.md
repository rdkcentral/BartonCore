## Why

SBMD v3 uses declarative YAML specifications with embedded JavaScript mapper scripts that serve as pure data transformers between Matter TLV and Barton resource strings. As device support has expanded to more complex devices (door locks, thermostats), the mapper-only model has proven insufficient: handling command response chains, correlating attribute reports with resource updates across multiple resources, and managing device-initiated events all require increasingly awkward workarounds in the v3 architecture. The v3 model has no clean way to express multi-step device interactions where a resource operation triggers a command, waits for a specific response command, and then completes — a pattern required by the Matter Door Lock cluster's credential operations.

SBMD v4 replaces YAML `.sbmd` files with self-contained `.sbmd.js` JavaScript files where the entire driver — metadata, resource declarations, and handler functions — is expressed in a single `SbmdDriver({...})` registration call. Handlers are arbitrary functions that return an immutable result chain describing operations for the C++ runtime to execute outside the JavaScript context. This eliminates JavaScript-to-C++ callbacks, avoids synchronization/deadlock risks, and enables multi-step device interactions through deferred operation chains.

## What Changes

- **New file format**: `.sbmd.js` files replace `.sbmd` YAML files. Each file is a complete JavaScript driver evaluated by the mquickjs engine.
- **Handler-based architecture**: Replace per-resource mapper scripts with handler functions that receive a common `args` object and return a result chain via `SbmdUtils.result()` builder.
- **Result builder pattern**: `SbmdUtils.result()` builds a plain JS object describing operations (resource updates, device commands, storage writes, logging) and a terminal (success, error, sendCommand, writeAttribute, requestCommand, readAttribute). The C++ runtime executes these after leaving the JS context.
- **First-class device message handlers**: Dedicated `attributeHandlers`, `eventHandlers`, and `commandHandlers` registrations replace the v3 pattern of reusing read mapper scripts for attribute reports.
- **Deferred command/response chains**: `requestCommand` and `readAttribute` park a resource operation and register response/error handlers that fire when the device responds or times out. Chains can extend through multiple deferrals with an overall operation timeout.
- **Supplements**: Handlers declare data dependencies (attributes from device cache, resource values) that the runtime pre-fetches before calling the handler — no callbacks from JS to C++.
- **Driver lifecycle management**: Drivers are parsed for metadata at startup but only fully activated (handler JSValues GC-rooted) when they have paired devices. Drivers with no devices are deactivated to free JS heap memory.
- **Constants injection**: Two-pass file evaluation extracts the `constants` block, injects values as `var` declarations, then evaluates the full file wrapped in an IIFE for namespace isolation.
- **Remove v3 infrastructure**: `SbmdParser` (YAML parser), `SbmdSpec` C++ data structures, JSON schema validation files, and the v3 mapper-based `SbmdScript` interface are removed. The yaml-cpp dependency is removed from SBMD (retained if used elsewhere).
- **v3 driver staging**: Existing `.sbmd` drivers are moved aside during conversion. The light driver is converted first as proof of life, then remaining drivers in complexity order.
- **Result builder in sbmd-utils.js**: `SbmdUtils.result()` is implemented in the existing JS utilities bundle. `SbmdUtils.Response.*` v3 helpers are removed.
- **Observability foundation** (separate PR, merged first): Lightweight metric instruments (counters, gauges, histograms) with a `gettelemetry`/`gt` JSON dump command, used to track driver resource consumption (JS heap, handler invocation times).

## Non-goals

- **No OpenTelemetry integration**: The observability work implements opaque metric instruments only. No OTLP export, spans, or log bridging.
- **No new device type support**: This change converts existing drivers to v4 format; no new Matter device types are added.
- **No changes to the public Barton API**: The resource model, device classes, and client-facing interfaces remain unchanged.
- **No changes to Python integration tests**: Existing tests are expected to pass unchanged against v4 drivers.
- **No multi-instance cluster support**: This limitation from v3 is not addressed in this change.
- **No changes to the Matter subsystem**: The CHIP SDK integration, commissioning flow, and subscription management remain unchanged.

## Capabilities

### New Capabilities
- `sbmd-v4-runtime`: The v4 SBMD runtime — file evaluation, registration extraction, handler dispatch, result execution, deferred operations, driver lifecycle (activate/deactivate), supplements resolution, and the `SbmdUtils.result()` builder.
- `sbmd-v4-light-driver`: The light driver converted from v3 YAML to v4 JavaScript, serving as the proof-of-life for the new runtime.
- `observability-metrics`: Lightweight in-process metric instruments (counter, gauge, histogram) with JSON dump via `gettelemetry`/`gt` command flow, independent of OpenTelemetry.

### Modified Capabilities
- `sbmd-system`: The SBMD factory now loads `.sbmd.js` files instead of `.sbmd` files. Driver registration, claiming, and the `SpecBasedMatterDeviceDriver` interface change to support the v4 handler model and driver lifecycle.
- `sbmd-script-execution-limits`: Script timeout enforcement applies to handler invocations. Overall operation timeouts and max deferral depth are added for deferred chains.

## Impact

- **Core drivers layer** (`core/deviceDrivers/matter/sbmd/`): Major rework — new registration system, handler dispatch, result execution engine, driver lifecycle. `SbmdParser`, `SbmdSpec`, `ScriptResult` replaced. `SbmdScript` interface changes significantly. `SpecBasedMatterDeviceDriver` rewritten to dispatch to handlers and execute result chains.
- **mquickjs integration** (`core/deviceDrivers/matter/sbmd/mquickjs/`): `SbmdScriptImpl` rewritten for v4 handler invocation, JSValue extraction from registration objects, GC root management for handler lifetime. `SbmdUtilsLoader` updated with result builder additions.
- **JS utilities** (`core/deviceDrivers/matter/sbmd/scriptCommon/sbmd-utils.js`): Extended with `SbmdUtils.result()` builder. v3 `SbmdUtils.Response.*` helpers removed. `SbmdUtils.Tlv.*` and `SbmdUtils.Base64.*` unchanged.
- **Spec files** (`core/deviceDrivers/matter/sbmd/specs/`): All 10 `.sbmd` files replaced with `.sbmd.js` equivalents over the course of this change.
- **Build system** (`core/CMakeLists.txt`, `config/cmake/`): Source file lists updated. YAML schema validation replaced with JS syntax validation. `BCORE_MATTER_SBMD_JS_ENGINE` CMake option unchanged (mquickjs remains default).
- **Unit tests** (`core/test/src/`): `sbmdParserTest.cpp` removed. `SbmdScriptTest.cpp` rewritten for v4 handler model. New tests for result execution, handler dispatch, deferred operations, driver lifecycle.
- **Integration tests** (`testing/test/`): Non-light tests temporarily disabled during conversion, re-enabled as drivers are converted. No test logic changes expected.
- **Reference app** (`reference/`): New `gettelemetry`/`gt` command added (observability PR).
- **Dependencies**: yaml-cpp dependency removed from SBMD build. No new external dependencies.
- **CMake flags**: `BCORE_MATTER_SBMD_JS_ENGINE`, `BCORE_MQUICKJS_MEMSIZE_BYTES`, `BCORE_SBMD_SCRIPT_TIMEOUT_MS` remain relevant. May need to adjust `BCORE_MQUICKJS_MEMSIZE_BYTES` default based on v4 memory profiling.
