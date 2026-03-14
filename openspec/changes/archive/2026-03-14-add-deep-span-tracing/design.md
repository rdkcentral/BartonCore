## Context

BartonCore has a C99 span API (`observabilityTracing.h`) exposing `ObservabilitySpan*` and `ObservabilitySpanContext*` opaque types with start, attribute, error, and context-propagation functions. Six spans exist today — all are isolated root spans with no parent-child linking:

```
Current state (all root spans, no hierarchy):

  [device.discovery]     (deviceService.c:589)
  [device.found]         (deviceService.c:2374)
  [resource.read]        (deviceService.c:1148)
  [resource.write]       (deviceService.c:1340)
  [subsystem.init]       (subsystemManager.c:528)
  [subsystem.shutdown]   (subsystemManager.c:634)
```

The `observabilitySpanStartWithParent()` API exists but is unused. Commissioning, pairing, device add/remove, Matter CASE sessions, subscription lifecycle, Zigbee driver operations, and the `updateResource()` pipeline have no span coverage.

### Threading model

```
  Main thread          Commission threads         Matter platform thread
  ───────────          ──────────────────         ─────────────────────
  deviceService*()  →  commissionMatterDeviceFunc  ScheduleWork() callbacks
  discovery/add     →  pairMatterDeviceFunc        DeviceDataCache callbacks
  resource read/write                              OnAttributeData/OnReportEnd
```

Span context must cross from the commissioning thread → Matter platform thread → driver callback → `updateResource()` on the main thread.

## Goals / Non-Goals

**Goals:**
- Wire existing root spans into parent-child hierarchies.
- Add spans for commissioning, pairing, device add, and device remove operations.
- Add spans for Matter CASE session establishment and subscription lifecycle.
- Add spans for Zigbee discovery handling and cluster read/write operations.
- Trace the `updateResource()` pipeline from driver callback through event production.
- Propagate span context across thread boundaries (commissioning → Matter → driver → updateResource).
- Add integration tests verifying span hierarchies in OTLP exports.

**Non-Goals:**
- Changes to the public C API layer (`api/c/public/`).
- Thread/OTBR subsystem tracing.
- Distributed tracing across network boundaries.
- New tracing API surface — existing `observabilityTracing.h` is sufficient.
- SBMD/QuickJS scripting layer instrumentation.

## Decisions

### Decision 1: Hybrid context propagation — objects store context, TLS for same-thread flows

**Choice**: Use two complementary mechanisms:
1. **Object-stored context** — `CommissioningOrchestrator` and `DeviceDataCache` store a ref-counted `ObservabilitySpanContext*` member set at construction. Their callbacks (on the Matter platform thread) create child spans from this stored context.
2. **Thread-local current context** — A per-thread `_Thread_local` current context set/get API allows same-thread flows (e.g., discovery → device.found, resource.write → driver write) to inherit context implicitly without modifying function signatures.
3. **Explicit pass at thread boundaries only** — The file-local pthread arg structs (`commissionMatterDeviceArgs`, `OnboardMatterDeviceArgs`) carry a context across thread creation. The new thread sets TLS at entry.

**Rationale**: This avoids modifying any internal function signatures (`DeviceFoundDetails`, `matterSubsystemCommissionDevice()`, `onDeviceFoundCallback()`, driver callbacks). Only private pthread arg structs and C++ object constructors gain a context parameter. The TLS pattern is safe because each thread sets its context at entry — the Matter platform thread sets it from the object-stored context in each callback, preventing stale TLS.

**Alternatives considered**:
- Fully explicit (pass context through every function) — too invasive, modifies ~6 signatures including `DeviceFoundDetails`
- TLS only (no object storage) — fragile on Matter platform thread where multiple devices share the thread
- Baggage propagation — over-engineered for in-process tracing

### Decision 1a: Ref-counted ObservabilitySpanContext with acquire/release semantics

**Choice**: Add `observabilitySpanContextRef()` to the tracing API. All context ownership follows acquire/release:
- `observabilitySpanGetContext()` returns a context with ref_count=1 (caller owns)
- `observabilitySpanContextRef()` increments ref_count (for sharing with TLS, objects, arg structs)
- `observabilitySpanContextRelease()` decrements; deletes at zero
- `observabilitySpanContextSetCurrent()` refs the context and stores in TLS; releases the previous TLS context
- `observabilitySpanContextGetCurrent()` returns the TLS context WITHOUT incrementing ref_count (borrowed reference)
- `g_autoptr(ObservabilitySpanContext)` already integrates with `observabilitySpanContextRelease()`

**Rationale**: Consistent ref-counting prevents use-after-free when contexts are shared across threads and objects. The `SetCurrent()` call owns a ref, so the context survives even if the caller releases their ref. `GetCurrent()` returns a borrowed reference (not owned) to avoid double-release — callers who need to store it long-term must call `Ref()` explicitly.

**New API surface** (3 functions in `observabilityTracing.h`):
```c
void observabilitySpanContextRef(ObservabilitySpanContext *ctx);
void observabilitySpanContextSetCurrent(ObservabilitySpanContext *ctx);
ObservabilitySpanContext *observabilitySpanContextGetCurrent(void);
```

### Decision 2: One root span per top-level operation, child spans for internal steps

**Choice**: Create root spans at operation entry points (`device.commission`, `device.pair`, `device.remove`), child spans for internal steps (`matter.case.connect`, `matter.subscribe`, `device.configure`, `device.persist`).

**Rationale**: Keeps traces clean with one root per user-initiated operation. Internal steps become children, showing the full operation breakdown in Jaeger waterfall views.

**Target hierarchy**:
```
device.commission (root, commissioning thread)
  ├── matter.case.connect (Matter platform thread)
  ├── matter.subscribe (Matter platform thread)
  ├── device.found → device.configure (driver thread / main thread)
  └── device.persist (main thread)

device.pair (root, pairing thread)
  ├── matter.commissioning.complete (Matter platform thread)
  ├── matter.case.connect (Matter platform thread)
  ├── matter.subscribe (Matter platform thread)
  ├── device.found → device.configure (driver thread)
  └── device.persist (main thread)

device.remove (root, main thread)
  ├── matter.fabric.remove (Matter platform thread)
  └── device.persist (main thread)

device.discovery (existing root, relinked)
  └── device.found (existing, now child instead of root)

resource.read (existing root, unchanged)
resource.write (existing root, unchanged)
  └── resource.update (new child for driver-initiated writes)

zigbee.cluster.read (root, Zigbee callback thread)
zigbee.cluster.write (root, Zigbee callback thread)
```

### Decision 3: Store span context in CommissioningOrchestrator and DeviceDataCache

**Choice**: Add an `ObservabilitySpanContext*` field to `CommissioningOrchestrator` (ref acquired at construction) and `DeviceDataCache` (ref acquired from orchestrator). Callbacks on the Matter platform thread set TLS from the stored context, then create child spans via `observabilitySpanStartWithParent(name, observabilitySpanContextGetCurrent())`.

**Rationale**: The Matter SDK dispatches callbacks on its own thread — we can't predict when they fire. By storing a ref-counted context in the objects that receive callbacks, each callback sets TLS and creates child spans safely. The ref-count ensures the context lives as long as the object, regardless of what happens on the originating thread.

### Decision 4: No span for every `updateResource()` call — only driver-initiated updates

**Choice**: Add a `resource.update` span only in the Matter `OnAttributeChanged()` and Zigbee `writeResource()` paths, not in the generic `updateResource()` function.

**Rationale**: `updateResource()` is called extremely frequently (every attribute report from every device). Adding a span to the generic function would create excessive trace volume. Spanning at the driver layer captures the meaningful operations.

### Decision 5: Integration tests verify span parent-child by trace ID matching

**Choice**: Tests assert that child spans share the same `traceId` as their parent and that `parentSpanId` matches the parent's `spanId`.

**Rationale**: The OTLP mock receiver already captures full span data. Verifying trace ID continuity confirms context propagation works across threads.

## Risks / Trade-offs

**[Risk: Span context lifetime across async Matter callbacks]** → Ref-counted `ObservabilitySpanContext*` stored in `DeviceDataCache` and `CommissioningOrchestrator`. Each object acquires a ref at construction and releases in destructor. The context is lightweight (just an OTel `Context` + ref count) so holding it for the subscription lifetime is safe.

**[Risk: TLS stale context]** → Each callback that uses TLS must set it at entry from the object-stored context. The pattern is: `observabilitySpanContextSetCurrent(this->parentCtx)` at callback entry. `SetCurrent()` refs the context, so the TLS value is valid for the callback's duration. Discovery callbacks set TLS from the stored discovery context before invoking device-found.

**[Risk: Excessive span volume from subscription reports]** → Do NOT span every `OnAttributeData()` callback. Only span at the `OnReportBegin()`/`OnReportEnd()` level for subscription cycles, and at `OnAttributeChanged()` in the driver for resource updates that actually produce a value change.

**[Risk: Thread-safety of span operations]** → `ObservabilitySpan*` and `ObservabilitySpanContext*` are both thread-safe (ref-counted with `g_atomic_int`). Creating child spans from TLS on any thread is safe — the OTel SDK handles concurrent span creation.

**[Risk: Zigbee span coverage is limited]** → ZHAL abstracts most Zigbee operations. We can only instrument at the zigbeeDriverCommon.c layer (discovery, read/write callbacks), not inside the HAL. This is acceptable — the driver layer is where BartonCore code runs.
