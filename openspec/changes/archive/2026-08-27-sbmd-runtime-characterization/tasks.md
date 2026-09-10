## 1. Heap metric corrections

- [x] 1.1 In `MQuickJsRuntimeMetrics::GCCallback`, capture pre-compaction `heap_used` at GC start (`is_end == 0`) via `JS_GetMemoryUsage` and fold it into `sbmd.js.heap.peak_bytes` through a new `RecordPeakCandidate(size_t)` method that only advances the gauge when the new reading exceeds the previously recorded peak.
- [x] 1.2 In the same callback, capture post-compaction `heap_used` at GC end (`is_end == 1`) and record it as a new gauge `sbmd.js.heap.live_bytes` (created in `MQuickJsRuntimeMetrics`'s constructor, guarded by `BARTON_CONFIG_SBMD_GC_INSTRUMENTATION`). No forced GC is introduced; sampling is opportunistic on engine-driven cycles only.
- [x] 1.3 Remove `sbmd.driver.load.heap_bytes`: delete the histogram handle, its creation, and `RecordDriverLoadSuccess`'s `std::optional<double> heapDelta` parameter from `SbmdFactoryMetrics`; remove the `usageBefore`/`usageAfter` `JS_GetMemoryUsage` calls and heap-delta computation from `SbmdFactory::RegisterDriversFromDirectory`; update the `#else` stub class to match the new signature.
- [x] 1.4 Update `openspec/specs/sbmd-runtime-observability/spec.md`'s "Driver load cost tracking" requirement and remove its "Load heap cost recorded per driver" scenario to match the code removal (already done directly, ahead of this change's archive).

## 2. Driver load timing

- [x] 2.1 Add `sbmd.driver.registration.total_ms` histogram to `SbmdFactoryMetrics`; record it in `SbmdFactory::RegisterDriversFromDirectory` from a `steady_clock::now()` taken before file discovery to a `steady_clock::now()` taken after `sbmd.driver.registered.count` is recorded.
- [x] 2.2 Add `sbmd.driver.bundle_load_ms` histogram to `SbmdFactoryMetrics`; record it exactly once, timing the `SbmdBundleLoader::LoadBundle` + capture-function-injection block guarded by `!runtimeReady`.

## 3. GC instrumentation compile-time guard

- [x] 3.1 Add a `MQUICKJS_HAS_GC_CALLBACK` macro definition to `mquickjs.h` in `docker/patches/mquickjs/0003-add-gc-callback-and-root-count.patch`, alongside the existing `JS_SetGCCallback` / `JS_GetGCRootCount` declarations.
- [x] 3.2 In `MQuickJsRuntime.cpp`, add `#if defined(BARTON_CONFIG_SBMD_GC_INSTRUMENTATION) && !defined(MQUICKJS_HAS_GC_CALLBACK)` / `#error` naming the missing patch and the CMake option to disable, placed after the mquickjs header include.
- [x] 3.3 Verify all three mquickjs patches (`0001`, `0002`, `0003`) still `git apply --check` cleanly against the pinned commit after the macro addition.

## 4. Report writer grading fixes

- [x] 4.1 Add `sbmd.js.heap.live_bytes` grading to `_assess()` in `sbmd_report_writer.py`: `ok` under 50% of arena, `watch` at ≥50%, `concern` at ≥75%; leave `peak_bytes`/`free_bytes`/`used_bytes` as `info`.
- [x] 4.2 Retarget the OOM pytest gate in `sbmd_heap_gc_mutex_metrics_test.py` from `test_heap_peak_stays_below_oom_threshold` (graded `peak_bytes < 75%`) to `test_live_set_stays_below_oom_threshold` (graded `live_bytes < 75%`, skipped when `live_bytes` is absent).
- [x] 4.3 Rewrite `_assess()`'s `sbmd.handler.heap_delta_bytes` branch to find the maximum observed value per `(driver, op_type[, resource_id])` series and grade the worst series' maximum against arena-scaled `HEAP_DELTA_WATCH_FRAC` (0.008) / `HEAP_DELTA_CONCERN_FRAC` (0.03) constants, replacing the prior scenario-wide-average grading; include the offending series' attributes in the assessment note.
- [x] 4.4 Add a `fault_injection` context flag threaded through `_render_metric` / `_render_metrics_map` / `_render_scenario`; when set, downgrade a `concern`-graded `sbmd.handler.outcome` non-success observation to `info` with an "expected — fault-injection scenario" annotation.
- [x] 4.5 Add `test_live_bytes_recorded_after_gc` (asserts `live_bytes` present, in range, and `<= peak_bytes` after GC pressure) and `test_heavy_transient_allocation_is_not_retained` (drives heavy transient allocation via the `gcPressure` test resource, asserts the post-GC `live_bytes` floor does not drift beyond tolerance) to `sbmd_heap_gc_mutex_metrics_test.py`.
- [x] 4.6 Add `test_live_set_floor_flat_under_sustained_load` (`@pytest.mark.slow`): samples the post-GC `live_bytes` floor at intervals across a sustained write soak and asserts the early-window floor and late-window floor stay within a small drift tolerance.
- [x] 4.7 Thread `sbmd.js.heap.live_bytes` through `format_heap_snapshot` (`sbmd_metrics_helpers.py`) and through the `collect()` metric lists and `heap_progression` CSV columns in `sbmd_metrics_report_test.py`.

## 5. Scale test suite

- [x] 5.1 Add `BARTON_EXTRA_SBMD_DIRS` environment-variable hook to `BaseEnvironmentOrchestrator.__init__`: append its value (if set) to `_sbmd_dirs` after the production and test-spec directories are configured; no-op when unset.
- [x] 5.2 Create `testing/observability/sbmd_scale_metrics_test.py` with a shared `_exercise_all_runtime_metrics(client, driven, deferred, scenario, extra_context)` helper: drives a healthy-load phase (one worker thread per `driven` op-closure, plus a deferred-command worker) followed by a fault-injection phase (deferred timeout + max-depth triggers), and asserts the full runtime metric family (mutex, handler, exception, heap, all `deferred.*`).
- [x] 5.3 Add `test_retained_memory_vs_device_count`: commissions `SBMD_SCALE_DEVICES` (default 20) devices of one driver, samples `live_bytes` after each commission, asserts the floor stays flat and under the arena OOM threshold.
- [x] 5.4 Add `test_all_runtime_metrics_under_concurrent_device_load`: commissions `SBMD_SCALE_DEVICES` devices plus a deferred device, builds one write-op closure per device, and calls the shared helper.
- [x] 5.5 Add module-level per-driver-type op-closure factories (`_light_op`, `_lock_op`, `_thermostat_op`, `_sideband_op`) and `test_all_runtime_metrics_across_heterogeneous_drivers`: commissions one each of light / door-lock / thermostat / temperature-sensor / humidity-sensor plus a deferred device, drives each concurrently via its natural invocation path (write, execute, or matter.js sideband push), and calls the shared helper.
- [x] 5.6 Add `_stub_spec()` (generates a minimal, schema-valid, inert SBMD driver spec) and a `hundred_driver_environment` fixture (writes `SBMD_SCALE_DRIVERS`, default 100, generated stub specs to a temp directory and sets `BARTON_EXTRA_SBMD_DIRS` before constructing the orchestrator).
- [x] 5.7 Add `test_driver_load_scales_with_stub_drivers`: asserts `registered.count`, `load.failure == 0`, `gc_roots` bounded, and heap occupancy bounded — scoped to load/registry/footprint metrics only.
- [x] 5.8 Retire two unsound predecessor tests that tried to read runtime metrics (`handler.*`, dispatch overhead) off the 100-stub registry — the stubs match no real device and their handlers are never invoked, so those assertions proved nothing.
- [x] 5.9 Exclude `testing/observability/` from the default CI integration run (`scripts/ci/run_integration_tests.sh`) with a comment pointing to `run_observability_suite.py` for on-demand runs; confirm the two pre-existing tracked observability tests outside that directory are unaffected.

## 6. DEBUG_GC build-flip tooling

- [x] 6.1 Create `testing/gcflip.sh` with `on` / `off` / `status` subcommands: build each DEBUG_GC variant of mquickjs once (clone the pinned commit, copy the patched `CMakeLists.txt`, strip the `DEBUG_GC` define for the `off` variant, apply patches `0001`–`0003`, build), cache the resulting `libmquickjs.a` under `~/.cache/bartoncore-gcflip/` (overridable via `GCFLIP_CACHE_DIR`), and on each invocation swap the cached archive into `/usr/local/lib/libmquickjs.a` and relink + install BartonCore (skippable via `GCFLIP_NO_RELINK=1`).
- [x] 6.2 Implement `status` by checksumming the installed archive and comparing against both cached variants (no separate state file); report `unknown` if neither matches.
- [x] 6.3 Add a `--debug-gc {on,off,both}` flag to `run_observability_suite.py` (default `off`): flips via `gcflip.sh` before running, restores `on` in a `finally` block for `off`/`both`, and for `both` runs the suite once per state into separate report directories (`.metrics-reports-debug_gc_{on,off}/`) via a new `_run_suite(extra, report_dir)` helper that also honors `SBMD_REPORT_DIR`.
- [x] 6.4 Update `sbmd_report_writer.py`'s `REPORT_DIR` to read from the `SBMD_REPORT_DIR` environment variable (falling back to the existing default), so the runner can route each `--debug-gc both` leg to its own directory.
- [x] 6.5 Create `testing/observability/README.md` documenting how to run the suite (`run_observability_suite.py` and its flags, including that DEBUG_GC off is the default and expected way to run these tests), where the report artifacts land, and how to read the verdict/status legend and the metrics-worth-knowing table.

## 7. Validation

- [x] 7.1 Run the full observability suite under DEBUG_GC off (`run_observability_suite.py --no-build --no-deps`) and confirm all scenarios, including the new concurrent-device, heterogeneous-driver, and driver-scale scenarios, pass with an overall `✅ OK` verdict.
- [x] 7.2 Confirm the full C/C++ unit test suite and the non-`observability/` Python integration suite still pass after the metric removals/additions.
- [x] 7.3 Confirm all three mquickjs patches still apply cleanly against the pinned commit and that both DEBUG_GC variants build and install successfully via `gcflip.sh`.
- [x] 7.4 Restore the dev-default DEBUG_GC-on build after validation.

## 8. Follow-on (not part of this change; tracked as open goals)

- [ ] 8.1 Investigate a sound, isolation-based per-driver retained-footprint sweep (commission one driver at a time, read the settled `live_bytes` floor, subtract the shared idle baseline) as a dev-time, static answer to per-driver memory attribution — distinct from, and not a substitute for, mixed-workload production attribution.
- [ ] 8.2 Decide whether a persisted per-`(driver, op_type)` `heap_delta` baseline (to catch sub-ceiling regressions) is worth the maintenance cost of keeping it current in the repository.
- [ ] 8.3 Investigate demand-side / throughput metrics (inbound event rate, a saturation ceiling for the single-threaded JS engine) — identified as the largest remaining blind spot for field observability versus lab characterization.
- [ ] 8.4 Investigate rate-windowed and cross-run-trend metrics, since all current metrics are cumulative-since-init with no periodic rate derivation or historical comparison across suite runs.
- [ ] 8.5 Decide whether to pursue matter.js patching to raise the heterogeneous-scenario's distinct-active-driver ceiling above the current ~5–7 real device types (blocked today: no generic/parameterizable device type, no `--device-type` flag).
- [ ] 8.6 Open product decision: whether to ship `BCORE_SBMD_GC_INSTRUMENTATION` (and mquickjs patch `0003`) to the production gateway build for on-hardware profiling.
