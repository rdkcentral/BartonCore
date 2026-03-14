## Why

Matter subsystem initialization is the most complex and failure-prone startup path in BartonCore — it involves credential loading/generation, fabric creation, SDK server init, commissioner setup, IPK provisioning, and mDNS advertisement. Today, the `subsystem.init` span ends before Matter's async init thread even runs, leaving this entire flow invisible in traces. When init fails or takes unexpectedly long (e.g., fabric re-creation, credential generation), operators have no span-level visibility into which step is responsible. Adding child spans to the Matter init path will close this observability gap.

## What Changes

- Add a `matter.init` root span covering the full async `maybeInitMatter()` flow in `matterSubsystem.cpp`, with retry attempt tracking
- Add `matter.init.stack` child span around `Matter::Init()` covering CHIP platform initialization, cert store setup, and keystore init
- Add `matter.init.server` child span around `Server::GetInstance().Init()` in `Matter::Start()`
- Add `matter.init.commissioner` child span around `InitCommissioner()` covering the full fabric/credential/controller setup
- Add `matter.fabric.create` child span inside `InitCommissioner()` when a new NOC chain is generated (CSR, credential generation, persistence) — this is the heaviest crypto operation
- Add `matter.init.advertise` child span around `DnssdServer::AdvertiseOperational()` — the only network-facing call in the init path
- Propagate span context via TLS from `maybeInitMatter` through `Init()` and `Start()` so all child spans form a coherent trace hierarchy

## Non-goals

- Spanning the Matter event loop (`StackThreadProc` / `RunEventLoop`) — this runs indefinitely and is not an init operation
- Spanning individual CHIP SDK internal functions beyond the call boundaries listed above
- Adding retry/backoff metrics — the `matter.initializing` gauge and `subsystem.init.duration` histogram already cover this
- Modifying function signatures — context propagation uses the existing TLS mechanism from the `add-deep-span-tracing` change

## Capabilities

### New Capabilities
- `otel-span-matter-init`: Span coverage for the Matter subsystem initialization sequence, including stack init, server init, commissioner setup, fabric creation, and mDNS advertisement

### Modified Capabilities
- `otel-tracing`: The `subsystem.init` span hierarchy gains Matter-specific child spans via TLS context propagation

## Impact

- **Subsystem layer**: `core/src/subsystems/matter/matterSubsystem.cpp` — new spans in `maybeInitMatter()`, TLS context propagation
- **Matter layer**: `core/src/subsystems/matter/Matter.cpp` — new spans in `Init()`, `Start()`, and `InitCommissioner()`
- **Observability docs**: `docs/OBSERVABILITY.md` and `docs/OBSERVABILITY_METRICS.md` — span inventory updates
- **CMake flag**: `BCORE_OBSERVABILITY` (existing) — all new spans gated by `BARTON_CONFIG_OBSERVABILITY`
- **Dependencies**: No new dependencies — uses existing `observabilityTracing.h` C API
