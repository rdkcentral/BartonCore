## MODIFIED Requirements

### Requirement: Peak heap watermark gauge
The SBMD runtime SHALL maintain a gauge named `sbmd.js.heap.peak_bytes` recording the highest value of `usage.heap_used` observed at a heap snapshot taken via `RecordHeapSnapshot()` (in-activity and idle captures only). (`JS_MEMUSAGE_WALK_HEAP` is not set, so `usage.heap_free_blocks` is always 0; `heap_used` is used directly as the high-water mark of heap expansion.) The gauge SHALL only update when the newly observed `heap_used` exceeds the previously recorded peak. This gauge SHALL NOT be sampled from the GC callback: under production's lazy compacting GC, a collection only fires once the arena is nearly exhausted, so a GC-start reading is consistently close to that trigger threshold on every cycle rather than reflecting any particular driver's or handler's true allocation footprint. (An earlier iteration of this change folded a GC-start sample into this gauge via a `RecordPeakCandidate()` method; that approach was found unsound for the reason above and replaced with the `sbmd.js.gc.alloc_churn_bytes` histogram below, which is not subject to the same saturation.)

#### Scenario: Peak watermark monotonically increases
- **WHEN** successive JS handler invocations allocate progressively more memory
- **THEN** `sbmd.js.heap.peak_bytes` reflects the highest value observed, never decreasing

## ADDED Requirements

### Requirement: Per-GC-cycle allocation churn tracking
The SBMD runtime SHALL record, for each completed GC cycle after the first, the allocation churn since the previous cycle as a histogram named `sbmd.js.gc.alloc_churn_bytes`, computed as this cycle's pre-compaction `heap_used` (captured at `is_end == 0` in `MQuickJsRuntimeMetrics::GCCallback`) minus the previous cycle's post-compaction `heap_used` (the previous cycle's retained live set). Unlike `sbmd.js.heap.peak_bytes`, this is a cycle-to-cycle delta and so is not saturated by the allocator's GC-trigger threshold — it can distinguish a heavy allocation burst from a light one. The very first GC cycle in a process's lifetime has no prior post-GC baseline and SHALL NOT produce an observation. This histogram SHALL be compiled only when `BCORE_SBMD_GC_INSTRUMENTATION` is enabled.

#### Scenario: Churn recorded from the second GC cycle onward
- **WHEN** a second (or later) GC cycle completes
- **THEN** `sbmd.js.gc.alloc_churn_bytes` gains one observation equal to this cycle's pre-compaction `heap_used` minus the previous cycle's post-compaction `heap_used`

#### Scenario: No observation on the very first GC cycle
- **WHEN** the first GC cycle of a process's lifetime completes
- **THEN** `sbmd.js.gc.alloc_churn_bytes` gains no observation, because there is no prior post-GC baseline to subtract against

### Requirement: Retained live-set gauge
The SBMD runtime SHALL maintain a gauge named `sbmd.js.heap.live_bytes` recording `heap_used` sampled at GC end (`is_end == 1`, after compaction) in `MQuickJsRuntimeMetrics::GCCallback`. Because transient garbage has just been reclaimed at this point, the sampled value equals the true retained live set — the one heap metric capable of distinguishing genuine retained-memory growth from the churn that `sbmd.js.heap.used_bytes` and `sbmd.js.heap.peak_bytes` cannot separate from reclaimable garbage under a lazy compacting GC. Sampling SHALL be opportunistic: it SHALL occur only when the engine drives a GC cycle on its own (or an explicit `JS_GC()` call in tests); the runtime SHALL NOT force an additional GC cycle solely to populate this gauge, so it has zero effect on `sbmd.js.heap.peak_bytes` or GC cadence. This gauge SHALL be compiled only when `BCORE_SBMD_GC_INSTRUMENTATION` is enabled (it depends on the `JS_SetGCCallback` API).

#### Scenario: Live set recorded after a GC cycle
- **WHEN** a GC cycle completes (`is_end == 1`)
- **THEN** `sbmd.js.heap.live_bytes` gauge records the post-compaction `heap_used` value

#### Scenario: Live set is bounded by the transient high-water mark
- **WHEN** `sbmd.js.heap.live_bytes` is sampled
- **THEN** its value SHALL be less than or equal to the concurrently recorded `sbmd.js.heap.peak_bytes` value

#### Scenario: Live set reveals a retained leak that peak/used cannot
- **WHEN** a driver retains a large allocation across many invocations (as opposed to transient, per-call churn)
- **THEN** the post-GC floor of `sbmd.js.heap.live_bytes` rises correspondingly, while `sbmd.js.heap.peak_bytes` may already be pinned near the arena limit regardless of whether the retained growth occurred

#### Scenario: Absent when GC instrumentation is disabled
- **WHEN** `BCORE_SBMD_GC_INSTRUMENTATION` is not enabled
- **THEN** `sbmd.js.heap.live_bytes` SHALL NOT appear in the telemetry dump

### Requirement: Driver registration total time tracking
The SBMD runtime SHALL record the total wall-clock time to discover, load, activate, and register all SBMD drivers across every configured SBMD directory as a single histogram observation named `sbmd.driver.registration.total_ms`, measured from the start of `SbmdFactory::RegisterDrivers` (before iterating the configured directory list) to the point every configured directory has been processed and the final `sbmd.driver.registered.count` is recorded. `SbmdFactory::RegisterDriversFromDirectory` runs once per semicolon-separated directory and SHALL NOT itself start or stop this timer.

#### Scenario: Registration total recorded once per startup, independent of directory count
- **WHEN** `SbmdFactory::RegisterDrivers` finishes processing every configured directory (whether one directory or several)
- **THEN** `sbmd.driver.registration.total_ms` contains exactly one observation covering the entire pass, independent of how many directories or individual drivers were involved

### Requirement: SBMD bundle load time tracking
The SBMD runtime SHALL record the one-time cost of loading the SBMD utilities bundle and injecting the capture function into the shared JS context as a histogram named `sbmd.driver.bundle_load_ms`. This SHALL be recorded exactly once, the first time `SbmdFactory::RegisterDriversFromDirectory` finds the runtime not yet ready (`runtimeReady == false`), and SHALL NOT be recorded again on subsequent calls once the runtime is ready.

#### Scenario: Bundle load recorded on first registration pass
- **WHEN** the SBMD runtime is initialized and its first driver-registration pass loads the utilities bundle
- **THEN** `sbmd.driver.bundle_load_ms` contains exactly one observation

#### Scenario: Bundle load not re-recorded on subsequent passes
- **WHEN** `SbmdFactory::RegisterDriversFromDirectory` is called again after the runtime is already ready
- **THEN** `sbmd.driver.bundle_load_ms` observation count SHALL NOT increase

### Requirement: GC instrumentation compile-time guard
The build SHALL fail at compile time with an explicit, actionable error if `BARTON_CONFIG_SBMD_GC_INSTRUMENTATION` is defined without the mquickjs GC-callback API (`JS_SetGCCallback` / `JS_GetGCRootCount`, added by `docker/patches/mquickjs/0003-add-gc-callback-and-root-count.patch`) being present. Patch `0003` SHALL define a `MQUICKJS_HAS_GC_CALLBACK` capability macro in `mquickjs.h` when applied. `MQuickJsRuntime.cpp` SHALL `#error` when `BARTON_CONFIG_SBMD_GC_INSTRUMENTATION` is defined and `MQUICKJS_HAS_GC_CALLBACK` is not, naming both the missing patch and the CMake option to disable as remediation.

#### Scenario: Compile error when instrumentation is enabled without the patch
- **WHEN** a build defines `BARTON_CONFIG_SBMD_GC_INSTRUMENTATION` against an mquickjs build that does not have patch `0003` applied
- **THEN** compilation SHALL fail with a `#error` naming the missing patch and the option to disable, instead of an unresolved-symbol link failure

#### Scenario: Clean compile when the patch is present
- **WHEN** `BARTON_CONFIG_SBMD_GC_INSTRUMENTATION` is defined and mquickjs patch `0003` (defining `MQUICKJS_HAS_GC_CALLBACK`) is applied
- **THEN** `MQuickJsRuntime.cpp` compiles without triggering the guard
