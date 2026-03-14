## 1. Span link API and matter.init root span

- [x] 1.1 Add `observabilitySpanStartWithLink()` declaration to `observabilityTracing.h` (enabled branch + no-op inline stub in disabled branch); implement in `observabilityTracing.cpp` — creates a root span with an OTel link to the provided context
- [x] 1.2 Add unit test `SpanStartWithLinkCreatesRootSpan` in `observabilityTracingTest.cpp` verifying the linked span is a root (no parent) and is non-null
- [x] 1.3 In `subsystemManagerInitialize()`, capture the `subsystem.init` span context via TLS and store it in a file-scoped variable accessible to subsystems; add `subsystemManagerGetInitSpanContext()` getter to `subsystemManager.h`
- [x] 1.4 Add `#include <observability/observabilityTracing.h>` to `matterSubsystem.cpp` (inside `extern "C"` block) and `Matter.cpp` (inside `extern "C"` block)
- [x] 1.5 In `maybeInitMatter()`, create `matter.init` root span using `observabilitySpanStartWithLink("matter.init", subsystemManagerGetInitSpanContext())` with attribute `retry.attempt`; set span context as TLS current; clear TLS and end span on all exit paths (success, failure, exception)
- [x] 1.6 Run all unit tests (`ctest`) and integration tests (`run_integration_tests.sh`) to confirm no regressions
- [x] 1.7 Commit: "feat(observability): add span link API and matter.init root span linked to subsystem.init"

## 2. CHIP stack and server init spans

- [x] 2.1 Add `matter.init.stack` child span in `maybeInitMatter()` around the `Matter::GetInstance().Init()` call, inheriting from TLS; on failure record error message
- [x] 2.2 Add `matter.init.server` child span in `Matter::Start()` around `Server::GetInstance().Init()`, inheriting from TLS; only create when `!serverIsInitialized`; on failure record CHIP error via `observabilitySpanSetError`
- [x] 2.3 Run all unit tests (`ctest`) and integration tests (`run_integration_tests.sh`) to confirm no regressions
- [x] 2.4 Commit: "feat(observability): add matter.init.stack and matter.init.server child spans"

## 3. Commissioner and fabric creation spans

- [x] 3.1 Add `matter.init.commissioner` child span in `Matter::Start()` around the `InitCommissioner()` call, inheriting from TLS; on failure record CHIP error
- [x] 3.2 Add `matter.fabric.create` child span inside `InitCommissioner()` around the conditional NOC chain generation block (AllocatePendingOperationalKey through SaveNOCChain); inheriting from TLS; only created when no existing chain/fabric
- [x] 3.3 Add `matter.init.advertise` child span inside `InitCommissioner()` around `DnssdServer::AdvertiseOperational()`; inheriting from TLS
- [x] 3.4 Run all unit tests (`ctest`) and integration tests (`run_integration_tests.sh`) to confirm no regressions
- [x] 3.5 Commit: "feat(observability): add commissioner, fabric creation, and mDNS advertisement spans"

## 4. Integration test and documentation

- [x] 4.1 Add integration test `test_matter_init_produces_span_hierarchy` verifying `matter.init`, `matter.init.stack`, `matter.init.server`, and `matter.init.commissioner` spans share the same traceId
- [x] 4.2 Update `docs/OBSERVABILITY.md` span hierarchy and inventory with the new Matter init spans
- [x] 4.3 Update `docs/OBSERVABILITY_METRICS.md` span-derived RED metrics table with new span names
- [x] 4.4 Run all unit tests (`ctest`) and integration tests (`run_integration_tests.sh`) to confirm no regressions
- [x] 4.5 Commit: "feat(observability): add Matter init span integration test and documentation"
