## Context

SBMD v3 is a working system with 10 `.sbmd` YAML driver files, a C++ YAML parser (`SbmdParser`), C++ data structures (`SbmdSpec`), and a mapper-based JavaScript execution model where short scripts transform data between Matter TLV and Barton resource strings. The mquickjs engine runs in a shared context with a pre-allocated memory buffer, mutex-protected access, and IIFE-wrapped script execution for isolation.

The v3 model treats JavaScript as a pure transformation layer — scripts receive input, return output, and have no side effects. This works for simple attribute-to-resource mappings but breaks down for:

- **Multi-step device interactions**: A resource execute that sends a command, waits for a response command, then completes — the v3 model has no way to park an operation.
- **Device-initiated message routing**: Attribute reports reuse read mapper scripts, which can only update the single resource they're bound to. One report updating multiple resources requires duplicate mappers.
- **Event-driven resources**: The v3 `seedFrom` + `event` mapper pairing is an awkward special case bolted onto the resource model.

v4 keeps the core principle (JavaScript has no side effects, no callbacks into C++) but replaces the mapper scripts with handler functions that return a declarative result chain. The C++ runtime interprets the chain after leaving the JS context.

### Current Architecture (v3)

```
.sbmd (YAML)
    │
    ▼
SbmdParser (yaml-cpp) → SbmdSpec (C++ structs)
    │
    ▼
SpecBasedMatterDeviceDriver
    │
    ├── DoReadResource ──▶ SbmdScript.MapAttributeRead()
    │     (IIFE-wrapped mapper script, returns {value: "..."})
    ├── DoWriteResource ──▶ SbmdScript.MapWrite()
    │     (returns {invoke: {...}} or {write: {...}})
    ├── ExecuteResource ──▶ SbmdScript.MapExecute()
    ├── attr report ──▶ SbmdScript.MapAttributeRead() (reuse)
    ├── event ──▶ SbmdScript.MapEvent()
    └── seedFrom ──▶ SbmdScript.MapAttributeRead() (reuse)
```

### Target Architecture (v4)

```
.sbmd.js (JavaScript)
    │
    ▼
IIFE-wrapped evaluation in mquickjs shared context
    │  (two-pass: extract constants, then evaluate)
    │
    ▼
SbmdDriver({...}) captures registration object
    │
    ▼
C++ extracts metadata (always in memory) + handler JSValues (GC-rooted only when active)
    │
    ▼
SpecBasedMatterDeviceDriver (reworked)
    │
    ├── resource read/seed ──▶ resolve supplements, call handler(args)
    │     handler returns SbmdUtils.result() chain → C++ executes ops
    ├── resource write ──▶ call handler(args) → result chain
    ├── resource execute ──▶ call handler(args) → result chain (may defer)
    ├── attr report ──▶ dispatch to attributeHandlers → result chain
    ├── event ──▶ dispatch to eventHandlers → result chain
    └── command ──▶ dispatch to commandHandlers / deferred response handler
```

### Thread Safety

The mquickjs shared context is protected by a single mutex (`MQuickJsRuntime::GetMutex()`). All JS operations — handler calls, registration extraction, GC root management — acquire this mutex. The Matter event loop and Barton's GLib main loop run on separate threads; resource operations arrive on the GLib thread, Matter callbacks arrive on the Matter thread. Both acquire the JS mutex before entering the JS context.

Result chain execution (the C++ side interpreting `{ops, terminal}`) happens **after** releasing the JS mutex, except for deferred response handlers which must re-acquire the mutex to call the stored handler JSValue.

The public Barton API (GObject-based `BCoreClient`, `BCoreDevice`, `BCoreResource`) is unaffected — it remains the same URI-based resource model. Drivers are an internal concern.

## Goals / Non-Goals

**Goals:**
- Replace v3 YAML+mapper architecture with v4 JavaScript handler architecture
- Maintain zero-callback JS execution model (args in, JSON-like result out)
- Support multi-step deferred device interactions (requestCommand, readAttribute)
- Enable efficient driver lifecycle (metadata-only until devices need handlers)
- Preserve all existing integration test behavior
- Measure resource consumption (JS heap, handler latency) via observability metrics
- Convert all 10 existing drivers from v3 to v4

**Non-Goals:**
- Changing the public Barton API or resource model
- Adding new device type support
- OpenTelemetry or distributed tracing integration
- Multi-instance cluster support
- Changing the mquickjs engine or its memory model
- Modifying the Matter subsystem (CHIP SDK integration, subscriptions, commissioning)

## Decisions

### 1. Pure JavaScript drivers (`.sbmd.js`) — no YAML

**Decision**: Each driver is a single `.sbmd.js` file containing a `SbmdDriver({...})` registration call and handler function definitions. No YAML, no embedded script snippets.

**Rationale**: v3's YAML-with-embedded-JS created two parsing layers (yaml-cpp for structure, mquickjs for scripts). v4 consolidates into one: the JS engine evaluates the file directly. This eliminates the YAML parser, the `SbmdSpec` intermediate representation, and the impedance mismatch between declarative YAML and imperative JS.

**Alternative considered**: Keep YAML for metadata, use separate `.js` files for handlers. Rejected because it splits the driver across files and still requires a YAML parser.

### 2. Two-pass evaluation with IIFE wrapping

**Decision**: Pass 1 extracts the `constants:` block via text scanning and evaluates it as a JS object literal to get name→value pairs. Pass 2 prepends `var` declarations for each constant, wraps the entire file in an IIFE, and evaluates it.

```
(function() {
  var EP_LIGHT = "1";
  var CL_ON_OFF = 6;
  // ... original file contents ...
  SbmdDriver({...});
  function readIsOn(args) {...}
})()
```

**Rationale**: Constants must be available as bare names when the `SbmdDriver({...})` object literal is evaluated (e.g., `clusterId: CL_ON_OFF`). mquickjs does not make `JS_SetPropertyStr` globals visible as variable names — the only way to create accessible names is via `var` declarations in the same compilation unit. IIFE wrapping prevents constant and function name collisions across drivers in the shared context.

**Alternative considered**: Separate JS contexts per driver. Rejected due to mquickjs's pre-allocated buffer model — multiple contexts would multiply memory requirements.

**Alternative considered**: `Object.freeze` for read-only constants. mquickjs lacks property flags on global vars. Build-time validation can catch reassignment instead.

### 3. `SbmdDriver()` as a pure JS capture function

**Decision**: `SbmdDriver` is a JavaScript function (not a C callback) that stores its argument in a global `__sbmd_registration` variable. The C++ runtime reads this variable after evaluation.

```js
// Injected once at context initialization:
var __sbmd_registration = null;
function SbmdDriver(reg) {
    if (__sbmd_registration !== null)
        throw new Error("SbmdDriver() called more than once");
    __sbmd_registration = reg;
}
```

**Rationale**: Avoids needing a C function callback during JS evaluation. mquickjs C functions require stdlib table registration at context creation time, which is inflexible. A JS capture function is simpler, and the C++ side only needs `JS_GetPropertyStr` to retrieve the result — a pattern already well-established in the codebase.

### 4. Handler invocation model — no JS-to-C++ callbacks

**Decision**: Handlers are pure functions: `args` in, result object out. All side effects (resource updates, device commands, storage writes, logging) are described in the returned result chain and executed by C++ after releasing the JS mutex.

```
C++ builds args → [acquire mutex] → call handler → get result → [release mutex] → execute ops
```

**Rationale**: Eliminates synchronization complexity and deadlock risk. The JS context is held for the minimum time (handler execution only). The result chain is a plain JS object that C++ walks via `JS_GetPropertyStr` — no serialization/deserialization overhead.

**Consequence**: Storage reads (`getPersistentData`, `getTransientData`) cannot call back into C++. They are provided as supplements — the handler declares which storage keys it needs, and the runtime pre-fetches them into `args.supplements` before calling the handler.

### 5. Mutable result builder with linear chaining

**Decision**: `SbmdUtils.result()` returns a mutable builder. Each method mutates the internal `{ops, terminal}` structure and returns `this` (for non-terminals) or the raw result object (for terminals). Branching is not supported.

**Rationale**: Immutable builders (new object per method call) create GC pressure in mquickjs's constrained heap. Mutable builders with linear chaining are safe because handlers are synchronous, single-threaded, and the spec requires exactly one terminal per chain.

### 6. Driver lifecycle — activate/deactivate

**Decision**: All drivers are parsed for C++ metadata at startup, but handler JSValues are only GC-rooted (activated) when the driver has paired devices. Drivers with no devices are deactivated (GC roots released, handlers eligible for collection).

```
Startup:
  for each .sbmd.js:
    evaluate → extract metadata to C++ → release JS objects
  for each paired device in database:
    activate its driver (re-evaluate file, GC-root handlers)

Commissioning:
  activate candidate drivers → claim → deactivate losers with no devices
```

**Rationale**: With many drivers but few paired devices, keeping all handler functions GC-rooted wastes JS heap. Metadata (device types, vendor/product IDs) is tiny C++ data — always available for claiming. Re-evaluation on activation is the same cost as initial load and happens infrequently.

### 7. Deferred operations with overall timeout

**Decision**: `requestCommand` and `readAttribute` park the resource operation. The pending state is a flat structure with replaceable match criteria, handler refs, and timer. Deferred handlers can return further deferrals — the pending state is re-armed iteratively.

An **overall operation deadline** is set at first park (from `matter.defaultTimeoutMs`) and never resets. Per-hop `timeoutMs` is capped by remaining overall budget. A **max deferral depth** (e.g., 10) provides a hard safety net.

**Rationale**: Multi-step credential operations on door locks require chained command/response sequences. The iterative re-arming model avoids nested data structures. The overall timeout prevents unbounded chains.

### 8. Dispatch tables built at activation time

**Decision**: When a driver is activated, the runtime resolves aliases and builds lookup tables:

```
Attribute dispatch: map<(clusterId, attributeId), vector<HandlerEntry>>
Wildcard dispatch:  map<clusterId, vector<HandlerEntry>>
Event dispatch:     map<(clusterId, eventId), vector<HandlerEntry>>
Command dispatch:   map<(clusterId, commandId), vector<HandlerEntry>>
```

Incoming device messages are matched against these tables. Specific handlers fire before multi-attribute handlers, which fire before wildcards.

**Rationale**: O(1) lookup per message instead of linear scan through handler registrations. The tables are small (tens of entries per driver) and built once.

### 9. Result chain structure

**Decision**: The result is `{ops: [...], terminal: {...}}`. `ops` is an ordered array of non-terminal operations. `terminal` is always present (enforced by the builder — terminals return the raw result object, cutting off further chaining). Operation types are identified by an `op` string field. Unknown `op` values are warned and skipped by the C++ executor.

```js
{
  ops: [
    { op: "updateResource", endpoint: "1", resource: "locked", value: "true" },
    { op: "log", message: "lock applied" },
  ],
  terminal: { op: "sendCommand", clusterId: 257, commandId: 0, payload: null, options: {} }
}
```

**Rationale**: Flat, extensible, easy to walk from C++. New operation types only require adding a method to the JS builder and a case to the C++ executor. The full ops list is preserved for debugging/telemetry.

### 10. Observability — separate PR, lightweight instruments

**Decision**: Implement opaque `ObservabilityCounter`, `ObservabilityGauge`, `ObservabilityHistogram` types backed by simple in-process data structures (no OpenTelemetry). Expose via `gettelemetry`/`gt` command in the reference app, returning JSON. Target metrics: handler invocation time histograms and JS heap usage per driver.

**Rationale**: Need to validate v4 resource consumption before converting all drivers. The observability API shape matches the branch work at `cleith/dev/open-telemetry` so it can be upgraded to OpenTelemetry later without changing call sites.

### 11. Phased driver conversion

**Decision**: Convert drivers in complexity order, starting with the light driver:

1. Light (simple: on/off, level) — proof of life
2. Contact sensor, temperature sensor, humidity sensor, occupancy sensor, water leak detector (simple read-only)
3. Air quality sensor (moderate — multiple resources)
4. Thermostat (complex — many modes/setpoints)
5. Door lock (complex — events, deferred commands, credentials)
6. IKEA Timmerflotte (vendor-specific, multi-endpoint)

Each conversion: write `.sbmd.js`, verify integration tests pass, measure resource consumption.

**Rationale**: Light exercises the core path (read, write, attribute handler, seed) without deferred operations. Validating it first proves the runtime before tackling complex drivers.

## Risks / Trade-offs

**[JS heap pressure from persistent handlers]** → Each activated driver keeps handler function objects GC-rooted in the shared mquickjs context. With 10 drivers × ~5-10 handlers, that's ~50-100 closures in the fixed-size heap. **Mitigation**: Driver lifecycle (activate/deactivate) limits rooted handlers to those with paired devices. Observability metrics track heap usage. `BCORE_MQUICKJS_MEMSIZE_BYTES` can be increased if needed.

**[Re-evaluation cost on activation]** → Activating a driver re-evaluates its `.sbmd.js` file, which includes parsing, compiling, and executing the full file. **Mitigation**: This only happens when a new device type is commissioned (rare, user-initiated). File sizes are small (< 10KB each). Re-evaluation takes milliseconds.

**[IIFE wrapping changes line numbers in error messages]** → The `var` preamble prepended before the file shifts line numbers in JS stack traces. **Mitigation**: Track the preamble line count and adjust reported line numbers in error logging. Or use the mquickjs filename parameter to include an offset hint.

**[Shared context namespace for `SbmdDriver` and `__sbmd_registration`]** → These globals persist across all driver evaluations. **Mitigation**: `__sbmd_registration` is reset to null after each extraction. `SbmdDriver` is set once and is tiny. IIFE wrapping prevents any other leakage.

**[Constants extraction via text scanning is fragile]** → Brace-matching to find the `constants:` block could fail on unusual formatting or comments. **Mitigation**: The constants block is constrained to primitive literals only (no nested objects, no expressions). Build-time validation can verify extraction succeeds. An alternative fallback: evaluate a stub `SbmdDriver` that only extracts constants.

**[Overall operation timeout vs per-hop timeout interaction]** → A long-running multi-hop chain could have its later hops starved of time budget. **Mitigation**: Per-hop timeouts are capped at the remaining overall budget. Drivers that need long chains set a larger `matter.defaultTimeoutMs`.
