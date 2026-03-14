## Context

Matter subsystem initialization runs asynchronously in a dedicated thread spawned by `matterSubsystemInitialize()`. The `subsystem.init` span in `subsystemManager.c` covers only the synchronous `subsystem->initialize()` call, which for Matter just spawns the thread and returns immediately. The actual heavy work — CHIP stack init, credential loading, fabric creation, server init, commissioner setup — happens later in `maybeInitMatter()` → `Matter::Init()` → `Matter::Start()` → `InitCommissioner()`, entirely outside any span context.

The existing observability infrastructure from `add-deep-span-tracing` provides all the primitives needed: `observabilitySpanStart`, `observabilitySpanStartWithParent`, `observabilitySpanGetContext`, `observabilitySpanContextSetCurrent/GetCurrent`, and `g_autoptr`-based RAII cleanup.

```
subsystemManagerInitialize() [main thread]
  └─ observabilitySpanStart("subsystem.init")
  └─ matterSubsystemInitialize()
     └─ std::thread(matterInitLoopThreadFunc)  ← span context lost here
        └─ maybeInitMatter()
           ├─ Matter::Init()
           │  ├─ LoadOrGenerateLocalNodeId()
           │  ├─ PlatformMgr().InitChipStack()
           │  ├─ PersistentStorageOpCertStore::Init()
           │  └─ PersistentStorageOperationalKeystore::Init()
           └─ Matter::Start()
              ├─ Server::GetInstance().Init()
              ├─ InitCommissioner()
              │  ├─ LoadNOCChain()
              │  ├─ [GenerateNOCChainSynchronous() + SaveNOCChain() — if new fabric]
              │  ├─ DeviceControllerFactory::Init()
              │  ├─ DeviceControllerFactory::SetupCommissioner()
              │  ├─ SetIPKOnce()
              │  ├─ ConfigureOTAProviderNode()
              │  ├─ FabricTable::CommitPendingFabricData()
              │  └─ DnssdServer::AdvertiseOperational()
              └─ std::thread(&Matter::StackThreadProc)
```

Thread safety: `maybeInitMatter()` is guarded by `subsystemMtx` and the `busy` flag. `Matter::Init()` and `Matter::Start()` are called sequentially on the init thread only. No concurrent access to the Matter singleton during init.

## Goals / Non-Goals

**Goals:**
- Provide span coverage for the Matter async initialization sequence so operators can see: (a) total init duration, (b) which phase is slow, (c) whether fabric creation (credential generation) occurred, (d) retry attempts
- Maintain the existing parent-child hierarchy pattern — `matter.init` as the root with child spans for major phases
- Use TLS context propagation to link spans across `matterSubsystem.cpp` → `Matter.cpp` call boundary
- Record error details when init fails (step name, CHIP error code)

**Non-Goals:**
- Spanning the Matter event loop or post-init runtime operations
- Adding spans inside CHIP SDK code
- Changing the retry/backoff mechanism or `maybeInitMatter`'s return semantics
- Adding new metrics (existing `matter.initializing` gauge and `subsystem.init.duration` histogram sufficient)
- Modifying the public GObject API — this is internal subsystem instrumentation

## Decisions

### Decision 1: Root span placement — `maybeInitMatter()` not `matterSubsystemInitialize()`

**Choice**: Create `matter.init` root span at the top of `maybeInitMatter()`, not in `matterSubsystemInitialize()`.

**Rationale**: `matterSubsystemInitialize()` just spawns a thread and returns. The actual init work happens in `maybeInitMatter()`, potentially after backoff delays. Placing the span there means it captures exactly one init attempt. On retry, a new `matter.init` span is created with a `retry.attempt` attribute, giving clear per-attempt visibility.

**Alternative rejected**: Wrapping the entire thread lifetime — this would create one very long span that includes idle backoff time, making duration meaningless.

### Decision 2: Span link to `subsystem.init` instead of parent-child

**Choice**: `matter.init` is a root span that carries an OTel *link* to the `subsystem.init` span context — not a child of it. A new C API function `observabilitySpanStartWithLink(name, linkedCtx)` creates a root span with a link reference. The `subsystem.init` context is captured in `subsystemManagerInitialize()`, passed to the Matter init thread, and used as the link target.

**Rationale**: `subsystem.init` is a short-lived span (~50ms) that ends when `subsystemManagerInitialize()` returns. Matter init runs seconds or minutes later (with exponential backoff retries). Making `matter.init` a child would inflate the parent's trace timeline. A link preserves the correlation (clickable in Jaeger's References panel) without distorting duration semantics. Each `matter.init` attempt gets its own clean trace.

**Alternative rejected**: Parent-child relationship — valid in OTel but produces misleading trace waterfalls where the parent appears to span the entire retry sequence.

### Decision 3: TLS propagation from `maybeInitMatter()` through `Init()`/`Start()`

**Choice**: Set TLS current context from the `matter.init` span in `maybeInitMatter()`. `Matter::Init()` and `Matter::Start()` create child spans using `observabilitySpanStartWithParent(..., observabilitySpanContextGetCurrent())`. No function signature changes needed.

**Rationale**: Consistent with the pattern established in `add-deep-span-tracing`. The init thread is single-threaded through the Init/Start sequence, so TLS is safe and straightforward.

### Decision 4: Span granularity — 6 spans covering major phases

**Choice**: Add spans at the following boundaries:
1. `matter.init` — root, wraps full `maybeInitMatter()` attempt
2. `matter.init.stack` — child, wraps `Matter::Init()` (platform + cert/key store init)
3. `matter.init.server` — child, wraps `Server::GetInstance().Init()` (SDK server initialization)
4. `matter.init.commissioner` — child, wraps `InitCommissioner()` (fabric + controller setup)
5. `matter.fabric.create` — child of commissioner, wraps NOC chain generation (only when creating new fabric)
6. `matter.init.advertise` — child of commissioner, wraps `DnssdServer::AdvertiseOperational()`

**Rationale**: These correspond to the major phases visible in the call chain. Each can fail independently and has distinct performance characteristics. `matter.fabric.create` is conditional and involves crypto + persistence — making it a separate span shows whether fabric creation occurred and how long it took. `matter.init.advertise` is the only network-facing call.

**Alternative rejected**: Finer-grained spans (e.g., per-cert-store, per-keystore) — diminishing returns, clutters traces without actionable insight.

### Decision 5: Error attribution with CHIP error codes

**Choice**: On failure, call `observabilitySpanSetError(span, chipErr.AsString())` to record the CHIP SDK error string as the span error message.

**Rationale**: CHIP error strings (e.g., `"CHIP Error 0x00000032: Timeout"`) are the canonical diagnostic output. Recording them in spans connects trace data to the existing log messages.

## Risks / Trade-offs

**[Risk: Init retry creates many root spans]** → Each `maybeInitMatter()` call creates a new `matter.init` root span. With exponential backoff, retries could produce multiple failed spans. Mitigation: the `retry.attempt` attribute lets operators filter by attempt number, and failed spans are small. This is actually desirable — each attempt should be independently traceable.

**[Risk: TLS context leak if init throws]** → `maybeInitMatter()` has a try/catch around Init/Start. If TLS is set before the try and an exception bypasses cleanup: Mitigation: Use `g_autoptr(ObservabilitySpanContext)` for RAII cleanup. Set TLS in the try block and clear in a finally-equivalent pattern (clear in catch + after try). The existing `observabilitySpanContextSetCurrent(NULL)` pattern handles this.

**[Risk: Span overhead during init]** → Negligible. Init runs once (or a few times on retry). 6 spans per attempt adds microseconds to a process that takes seconds.

## Open Questions

None — the approach follows established patterns from `add-deep-span-tracing`.
