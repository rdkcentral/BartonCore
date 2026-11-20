## ADDED Requirements

### Requirement: Observability-suite auto-tagging
Every test collected from `testing/observability/` SHALL be automatically tagged with an `observability_suite` pytest marker, applied by a path-based collection hook rather than requiring per-test annotation. This tag SHALL be the basis for CI test selection, so that adding a new file to the directory does not require remembering to annotate it for correct CI inclusion/exclusion.

#### Scenario: A new test file is automatically tagged
- **WHEN** a new test function is added to any file under `testing/observability/`
- **THEN** that test carries the `observability_suite` marker without any explicit annotation in the test file

### Requirement: CI-gating marker for build-independent tests
A test whose assertions do not depend on the mquickjs `DEBUG_GC` compile-time state (i.e. not a timing/latency-bucket assertion that only holds under the production-representative build) and that does not require heavy, multi-subprocess fixtures MAY be explicitly annotated with a `ci_gate` pytest marker. CI's integration test run SHALL select tests via a marker expression equivalent to "run everything except `observability_suite`-tagged tests that are not also `ci_gate`-tagged," rather than excluding `testing/observability/` by path.

#### Scenario: A ci_gate-marked test runs in the default CI invocation
- **WHEN** the CI integration test script runs with its default test selection
- **THEN** every test marked both `observability_suite` and `ci_gate` executes as part of that single invocation

#### Scenario: An un-marked observability test is excluded from CI
- **WHEN** a test is tagged `observability_suite` but not `ci_gate`
- **THEN** CI's default test selection does not execute it, while a full observability-suite run still does

#### Scenario: CI test selection remains a single pytest invocation
- **WHEN** CI's integration test script runs
- **THEN** both the pre-existing `testing/` coverage and the newly-selected `ci_gate` subset of `testing/observability/` execute within one `pytest` invocation, producing one consolidated result summary

### Requirement: Per-test report contribution via shared fixture
A test MAY opt into contributing its own check's result to the consolidated metrics report by declaring an `sbmd_report` pytest marker (naming a scenario and the metric names relevant to that check) and requesting the shared `sbmd_report` fixture, without restructuring the test's existing body or assertions. On teardown, if report emission is enabled for the invocation, the fixture SHALL determine the test's own pass/fail outcome and record it — together with a fresh sample of the named metrics — as that test's entry in the consolidated report, using the existing report-writer functions unchanged.

#### Scenario: A passing check is recorded with its own result
- **WHEN** a test marked `sbmd_report` passes, under an invocation with report emission enabled
- **THEN** the consolidated report contains an entry for that test's scenario recording a passing status, distinct from any other scenario's independently-sampled value for the same metric name

#### Scenario: A failing check is recorded with its own result
- **WHEN** a test marked `sbmd_report` fails its assertion, under an invocation with report emission enabled
- **THEN** the consolidated report contains an entry for that test's scenario recording a failing status and the assertion's failure message

#### Scenario: Report emission is skipped when disabled
- **WHEN** a test marked `sbmd_report` runs under an invocation with report emission disabled (the default for CI's `ci_gate` selection)
- **THEN** no report file is read, written, or locked as a result of that test running

### Requirement: Pass/fail roll-up in the consolidated report
The human-readable `sbmd_metrics.txt` report SHALL include a summary line, at or near the top of the report, stating how many of the tests that contributed an `sbmd_report`-tagged result passed, failed, and were skipped for that invocation. A skipped test SHALL be recorded with a skipped status and counted as skipped, never as failed.

#### Scenario: Roll-up reflects the invocation's actual results
- **WHEN** a suite invocation runs N tests tagged `sbmd_report`, of which M pass, F fail, and S are skipped
- **THEN** the report's summary line states M passed, F failed, and S skipped

#### Scenario: A skipped check is not reported as a failure
- **WHEN** a test tagged `sbmd_report` skips (for example because GC instrumentation is not built in)
- **THEN** its report entry records a skipped status with the skip reason, and the roll-up does not count it as failed

### Requirement: Checked-in sanity ceilings for build-independent report metrics
A report-only test (one with no prior hard assertions) MAY assert that a metric confirmed independent of the mquickjs `DEBUG_GC` compile-time state stays under an explicit sanity ceiling. The ceiling SHALL be a named source constant documented with the real baseline measurement and multiplier it was derived from (not computed from the same run it checks, and not cached or auto-updated by any CI process), and SHALL be changed only via a reviewed code change. This treatment SHALL NOT be applied to any metric whose magnitude depends on `DEBUG_GC` state — including every duration/timing histogram and `sbmd.js.heap.peak_bytes`, which carries a `DEBUG_GC`-dependent byte offset — until a per-build baseline mechanism exists.

#### Scenario: A build-independent count exceeding its ceiling fails the check
- **WHEN** `sbmd.js.gc_roots` is sampled in a report test and exceeds the checked-in ceiling constant
- **THEN** that test fails, rather than silently accepting any observed value

#### Scenario: A deliberately-triggered counter is excluded from its sibling's ceiling
- **WHEN** the deferred-profile report test's `runawayToggle`/`timeoutToggle` resources deliberately trip the re-arm cap as part of the scenario's design
- **THEN** the `sbmd.deferred.depth` sanity ceiling applies only to the `toggle` resource, not to the resources intentionally exercising the cap

#### Scenario: A ceiling is never computed from the same run it checks
- **WHEN** a sanity-ceiling assertion runs
- **THEN** it compares the observed value against a constant already present in source control before that run started, never against a value derived from that same run's own measurement

#### Scenario: A timing metric does not receive this treatment
- **WHEN** a report-only test's metric is a duration/timing histogram, or `sbmd.js.heap.peak_bytes`
- **THEN** no sanity ceiling is added for it until a per-`DEBUG_GC`-build baseline mechanism is designed separately
