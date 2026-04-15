## Context

SBMD mapper scripts run inside a shared mquickjs singleton context. All script executions (across all devices and all mapper types) are serialized through a single mutex (`MQuickJsRuntime::GetMutex()`). The current execution path in `SbmdScriptImpl::ExecuteScript` compiles the script via `JS_Eval`, then calls it via `JS_Call`, with no mechanism to abort a running script.

```
Current execution flow (no timeout):

  MapAttributeRead / MapWrite / MapExecute / MapEvent
       │
       ▼
  lock(MQuickJsRuntime::GetMutex())
       │
       ▼
  ExecuteScript(script, argName, jsonArg, outJson)
       │
       ├── JS_Eval(wrappedScript)   ← compile phase (fast, bounded)
       │
       ├── JS_Call(ctx, 1)          ← execution phase (UNBOUNDED)
       │                               while(true){} blocks here forever
       │                               mutex held the entire time
       ▼
  return result
```

The mquickjs engine provides `JS_SetInterruptHandler(ctx, handler)` which installs a callback invoked periodically during JS bytecode execution. If the handler returns non-zero, the engine throws an exception and aborts the current script. This is the standard mechanism for implementing execution timeouts in QuickJS-family engines.



## Goals / Non-Goals

**Goals:**
- Prevent any single SBMD script execution from blocking the service indefinitely.
- Provide a configurable timeout with a sensible default.
- Fail gracefully through existing error paths (no crashes, no leaked state).
- Log clear diagnostics when a script is terminated or rejected.

**Non-Goals:**
- Protecting against cross-script pollution of JS globals/builtins.
- Validating script output (cluster/command IDs).
- Changes to the quickjs backend.
- Per-execution context isolation.

## Decisions

### Decision 1: Use mquickjs interrupt handler with opcode-count-based timeout

**Choice**: Install a `JS_SetInterruptHandler` callback on the shared context. Track a deadline (`std::chrono::steady_clock`) set before each `JS_Call`. The handler checks the clock and returns non-zero if the deadline has passed.

**Why not a separate watchdog thread?** The interrupt handler is called from within the JS engine's bytecode interpreter loop — it naturally fires during long-running scripts without requiring a separate thread, timer, or signal. The mutex is already held during execution, so a separate thread couldn't safely interact with the context anyway.

**Why steady_clock?** It's monotonic (immune to wall-clock adjustments) and available in C++17.

**Alternatives considered:**
- **POSIX signals (SIGALRM)**: Unsafe with mutexes, not portable, difficult to target the right thread.
- **Separate watchdog thread**: Would need to coordinate with the mutex holder; adds complexity for no benefit since the interrupt handler is purpose-built for this.

### Decision 2: Timeout is set per-ExecuteScript call, not globally

**Choice**: Set the deadline immediately before `JS_Call` in `ExecuteScript` and clear it after via `ClearDeadline()`. Use a static variable (safe because the mutex serializes all access). The timeout duration is the compile-time `BARTON_CONFIG_SBMD_SCRIPT_TIMEOUT_MS`.

**Rationale**: The interrupt handler is global to the context, but we only want it active during script execution, not during `SbmdUtilsLoader::LoadBundle` or other internal JS operations. A static deadline variable that is only set during `ExecuteScript` achieves this — when no deadline is set (zero/default), the handler returns 0 (continue).

### Decision 3: Timeout value as a CMake build-time option

**Choice**: Add `BCORE_SBMD_SCRIPT_TIMEOUT_MS` as a `bcore_int_option` in `config/cmake/options.cmake` with a default of 5000ms (5 seconds). Compiled into the binary as `BARTON_CONFIG_SBMD_SCRIPT_TIMEOUT_MS`.

**Rationale**: Consistent with existing pattern (`BCORE_MQUICKJS_MEMSIZE_BYTES`). 5 seconds is generous for scripts that should complete in microseconds — any script taking more than 5 seconds is certainly misbehaving.

### Decision 4: Interrupt handler installed once at Initialize, deadline controls activation

**Choice**: Install the interrupt handler in `MQuickJsRuntime::Initialize()` and leave it installed for the lifetime of the context. The handler checks a static `deadline` variable — when it's zero (default, no active script), the handler returns 0 immediately (no interrupt). `ExecuteScript` sets the deadline before `JS_Call` and clears it after.

```
Proposed execution flow:

  MQuickJsRuntime::Initialize()
       │
       ▼
  JS_SetInterruptHandler(ctx, ScriptInterruptHandler)
       │   (installed once, checks static deadline)

  ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─

  ExecuteScript(script, argName, jsonArg, outJson)
       │
       ├── JS_Eval(wrappedScript)
       │
       ├── SetDeadline(now + BARTON_CONFIG_SBMD_SCRIPT_TIMEOUT_MS)  ← arm
       ├── JS_Call(ctx, 1)                 ← engine periodically calls handler
       │       │                              handler: return (now > deadline) ? 1 : 0
       │       └── (if interrupted, JS_Call returns exception)
       ├── ClearDeadline()                 ← disarm
       ▼
  return result
```

**Why not install/remove per call?** `JS_SetInterruptHandler` may have non-trivial overhead in mquickjs. Installing once and using a flag is simpler and avoids any edge cases with the handler being partially installed during a call.

## Risks / Trade-offs

**[Risk: Interrupt handler granularity]** → The interrupt handler is called at opcode boundaries, not on a precise timer. A script blocked in a native C function (e.g., a hypothetical long-running `SbmdUtils` call) would not be interrupted until the C function returns to JS. **Mitigation**: All current `SbmdUtils` native functions are fast (TLV encode/decode, base64). This is acceptable.

**[Risk: Timeout too aggressive for slow platforms]** → A device running on very constrained hardware might hit the timeout on legitimate scripts. **Mitigation**: The timeout is a build-time configurable option (`BCORE_SBMD_SCRIPT_TIMEOUT_MS`). Deployers can increase it. The 5s default is already 1000x longer than any expected script execution.

**[Risk: State after timeout]** → After an interrupt, the mquickjs context may have partially-constructed objects on the heap. **Mitigation**: The interrupt causes a JS exception, which is caught by the existing `JS_IsException` check after `JS_Call`. GC will clean up unreachable objects. The context remains usable for subsequent scripts.

**[Backward compatibility]** → No public API changes. The timeout and size limit are new internal behaviors with generous defaults. Existing `.sbmd` specs are unaffected — all current scripts are small and fast.

**[Thread safety]** → The static deadline variable is safe because all access to the mquickjs context (and therefore all calls to `ExecuteScript`) is serialized through `MQuickJsRuntime::GetMutex()`. The interrupt handler itself is called from within `JS_Call`, which runs under the same mutex.

**[GObject API implications]** → None. This change is entirely internal to the SBMD script execution layer and does not affect the public GObject API, signals, or properties.
