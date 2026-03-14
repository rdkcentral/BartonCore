## Why

BartonCore's current tracing coverage is shallow — six root spans exist (`device.discovery`, `device.found`, `resource.read`, `resource.write`, `subsystem.init`, `subsystem.shutdown`) but none form parent-child hierarchies. Critical operations like commissioning, device add/remove, Matter CASE sessions, and Zigbee driver interactions have zero span coverage. This makes it impossible to trace a request end-to-end through the stack or diagnose latency across subsystem boundaries. Deeper span instrumentation will close this gap.

## What Changes

- **Commissioning & pairing spans**: Trace the full Matter commissioning flow from `deviceServiceCommissionDevice()` through `CommissioningOrchestrator::Commission()`, and the pairing flow through `CommissioningOrchestrator::Pair()`, as parent-child span trees.
- **Device add/remove spans**: Add spans for the `deviceServiceAddDevice()` and `deviceServiceRemoveDevice()` flows including driver callbacks and database operations.
- **Matter subsystem spans**: Instrument CASE session establishment, subscription lifecycle (established/report/error), and commissioning window operations in the Matter subsystem layer.
- **Zigbee subsystem spans**: Add spans for Zigbee device discovery handling, cluster attribute read/write operations through ZHAL, and firmware upgrade flows.
- **Resource update propagation spans**: Trace the `updateResource()` path from driver callback through resource model update, event production, and telemetry dispatch.
- **Parent-child span linking**: Wire existing root spans into hierarchies — e.g., `device.discovery` → `device.found` → `device.add`, `resource.write` → driver write → subsystem write.
- **Span context propagation across threads**: Pass `ObservabilitySpanContext*` across thread boundaries (commissioning thread, Matter platform thread, ZHAL callback thread) to maintain trace continuity.
- **Integration tests**: Verify span hierarchies (parent-child relationships) and new span names appear in OTLP exports.

## Non-goals

- Changes to the public C API layer (`api/c/public/`).
- Adding tracing to the Thread/OTBR subsystem (limited interaction surface).
- Distributed tracing across network boundaries (e.g., propagating trace IDs to Matter devices).
- Changes to the SBMD/QuickJS scripting layer.
- New tracing API surface — the existing `observabilityTracing.h` API is sufficient.

## Capabilities

### New Capabilities
- `otel-span-device-lifecycle`: Spans covering commissioning, pairing, device add, device remove, and commissioning window operations with parent-child hierarchies.
- `otel-span-matter-subsystem`: Spans for Matter-specific operations: CASE session establishment, subscription lifecycle events, and Matter SDK interactions.
- `otel-span-zigbee-subsystem`: Spans for Zigbee operations: discovery handling, cluster reads/writes through ZHAL, and firmware upgrades.
- `otel-span-resource-flow`: Spans tracing the full resource update path from driver callback through resource model, event production, and telemetry dispatch.

### Modified Capabilities
- `otel-tracing`: Existing root spans (`device.discovery`, `device.found`, `resource.read`, `resource.write`) will be modified to participate in parent-child span hierarchies instead of being isolated root spans.

## Impact

- **Core service** (`core/src/deviceService.c`): Span context threading through discovery → found → add flows, new spans for commission/remove.
- **Matter subsystem** (`core/src/subsystems/matter/`): New spans in `CommissioningOrchestrator.cpp`, `Matter.cpp`, `matterSubsystem.cpp`, `DeviceDataCache.cpp`.
- **Zigbee subsystem** (`core/src/subsystems/zigbee/`): New spans in discovery handlers, cluster operations.
- **Observability layer** (`core/src/observability/`): No API changes needed — existing `observabilityTracing.h` is sufficient.
- **Tests** (`testing/test/observability/`): New integration tests verifying span hierarchies.
- **CMake flags**: All changes guarded by existing `BCORE_OBSERVABILITY` / `BARTON_CONFIG_OBSERVABILITY`.
- **Dependencies**: No new dependencies. Uses existing OTel C++ SDK.
