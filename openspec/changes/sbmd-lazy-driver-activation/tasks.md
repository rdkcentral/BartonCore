## 1. SbmdDriver: split claim stub from active registration

- [x] 1.1 Keep the claim metadata (name, `matter.deviceTypes`, `vendorId`, `productId`, device class, `filePath`) resident on `SbmdDriver`'s `SbmdRegistration` in every state; while inactive the registration is reduced to just those claim fields (no separate stub type — the claim accessors read the reduced registration directly).
- [x] 1.2 Change `SbmdDriver::Activate()` to read the spec content from `filePath` on disk (instead of the retained in-memory `source`), parse, build the full `SbmdRegistration` and dispatch tables, and root handler JSValues; fail activation cleanly (log, return false) on file read/parse error.
- [x] 1.3 Change `SbmdDriver::Deactivate()` to additionally release the `source` string and the heavy `SbmdRegistration` collections (endpoints, resources, aliases, handlers) back to the claim-stub state, in addition to releasing GC roots and clearing dispatch tables.
- [x] 1.4 Stop retaining `source` after load; ensure the claim stub is the only resident state while inactive. Keep `Activate()`/`Deactivate()` idempotent.
- [x] 1.5 On activation, validate the re-read spec's claim-identity fields (name, device types, vendor/product IDs, device class) against the claim stub; on mismatch (or read/parse failure) fail activation, log, and abort the bind.
- [x] 1.6 Update `SbmdDriverTest.cpp`: assert stub metadata is available while inactive; assert `source`/registration are released after deactivation; assert activation re-reads from disk; add a deactivate→reactivate round-trip test. (unit)

## 2. SbmdFactory: register drivers inactive at startup

- [x] 2.1 Remove the eager `Activate()` call in `SbmdFactory::RegisterDriversFromDirectory`; load each driver, capture claim metadata, then leave it in the inactive claim-stub state.
- [x] 2.2 Ensure the driver is still constructed, wrapped in `SpecBasedMatterDeviceDriver`, and registered with `MatterDriverFactory`/`deviceDriverManager` (claim path unchanged).
- [x] 2.3 Adjust factory metrics: split the current "load+activate" duration into a load-only metric; move activation timing to the per-activation metric added in section 5.
- [x] 2.4 Update `SbmdFactoryTest.cpp`: assert drivers are registered inactive at startup and hold only the claim stub; assert claiming metadata is present without activation. (unit) — the factory unit test does not drive the real `SbmdFactory`, so inactive-at-startup is asserted at integration level in `sbmd_lazy_activation_test.py::test_drivers_inactive_at_startup` (real startup path), and the claim-stub state is covered at the `SbmdDriver` level in 1.6.

## 3. SpecBasedMatterDeviceDriver: activate on device bind

- [x] 3.1 Re-point the claim-time metadata accessors (`GetSupportedDeviceTypes`, `GetSupportedVendorId`, `GetSupportedProductId`, `IsVendorSpecificDriver`, device class) to read from the `SbmdDriver` claim stub so they work while inactive. (Claim metadata is retained in the shrunk registration, so the existing accessors work unchanged.)
- [x] 3.2 Activate the driver at the top of `SpecBasedMatterDeviceDriver::AddDevice` (under `MQuickJsRuntime::GetMutex()`), before any `GetRegistration()`/dispatch access; abort the bind if activation fails.
- [x] 3.3 Audit all `GetRegistration()`/dispatch-table callers in `SpecBasedMatterDeviceDriver` to confirm they only run after activation (dispatch entry points already gate on `IsActivated()`).
- [x] 3.4 Add `SbmdDispatchTest`/driver-level coverage that exercises claim→bind→read/write/execute/dispatch against a driver that started inactive. (unit) — covered at integration level: existing device tests (temperature, thermostat, door lock, light, ikea) all pass with drivers starting inactive, and `sbmd_lazy_activation_test.py` asserts commissioning activates the driver.

## 4. MatterDeviceDriver: deactivate on last-device removal

- [x] 4.1 Add a protected virtual hook (e.g. `OnLastDeviceRemoved()`) to `MatterDeviceDriver`, invoked from `DeviceRemoved` after erasing the device under `devicesMutex` when the `devices` map becomes empty; base default is a no-op.
- [x] 4.2 Override the hook in `SpecBasedMatterDeviceDriver` to `Deactivate()` the driver under the JS mutex; ensure outstanding deferred operations for the removed device are settled/cancelled before deactivation. Coordinate with concurrent binds: `AddDevice` activates before inserting into the device map, so a per-driver in-flight-bind counter is held across the whole of `AddDevice`, and the hook deactivates only when the driver is still activated, no bind is in flight, and the device map is still empty when rechecked under `devicesMutex` — closing the race where a stale empty-map observation would shed state out from under an in-progress bind.
- [x] 4.3 Verify the driver stays active while any device remains and deactivates only on the last removal (per-driver count). (unit) — covered at integration level: `sbmd_lazy_activation_test.py::test_removal_deactivates_driver` commissions then removes a device (via `client.remove_device`) and asserts the deactivation counter increments and the active-count gauge returns to zero.
- [x] 4.4 Add unit coverage for the last-device-removed → deactivate path and multi-device stay-active path. (unit) — last-device-removed path covered by `test_removal_deactivates_driver`; multi-device stay-active path covered by `test_driver_stays_active_until_last_device_removed`; `SbmdDriverTest` covers the `Deactivate` state shedding.
- [x] 4.5 Cover deferred-operation settlement on last-device removal: a pending deferred op is failed/settled and the driver deactivated, and a late response is handled safely. (integration) — `sbmd_lazy_activation_test.py::test_deferred_op_settled_when_last_device_removed` withholds the device's command response (via the `armToggleHang`/`releaseToggle` side-band on `DeferredCmdTestDevice`), removes the device while the op is parked, and asserts the blocked caller returns **with a failure result (`ok=False`)**, the driver deactivates, and Barton stays healthy after a late response.
- [x] 4.6 Reject a bind whose re-read spec changed the device-class version, an endpoint profile version, or the set of endpoint profiles (versions the base driver caches at construction for commissioning/reconfiguration). — implemented in `SpecBasedMatterDeviceDriver::ActivatedSpecMatchesCachedVersions`, which also rejects a `profileVersion` outside the `uint8` range and compares the complete cached and re-read profile sets so a removed profile cannot leave a stale cached version; the version-change branches are covered by `sbmd_lazy_activation_test.py::test_bind_rejected_when_device_class_version_changes` and `::test_bind_rejected_when_profile_version_changes` (patch the spec on disk, commission, assert the device is not added and the activation rolls back); normal commissioning (unchanged versions) is unaffected, verified by the rest of the integration suite.

## 5. Observability

- [x] 5.1 Add metrics under `BARTON_CONFIG_SBMD_METRICS`: attributed activate/deactivate event counters (driver-stem attribute) and a current active-driver count as a set-on-change `ObservabilityGauge` (mirroring `registeredDriversGauge`); split the `sbmd.driver.load.duration_ms` metric into load-only plus per-activation duration.
- [x] 5.2 Update `SbmdObservabilityTest.cpp` and/or add assertions for the new metrics. (unit) — activation metrics are recorded in the device-lifecycle path, so they are asserted at integration level in `sbmd_lazy_activation_test.py` (activation counter, active-count gauge, and the `sbmd.driver.activation.duration_ms` histogram: >= 1 observation with a positive sum after commissioning).
- [x] 5.3 Update `testing/test/sbmd_load_metrics_test.py` for the load-vs-activation metric split; add an integration assertion that idle drivers report inactive and activate on commissioning. (integration, Docker) — added `testing/test/sbmd_lazy_activation_test.py`.

## 6. Verification

- [x] 6.1 Run the SBMD unit tests (`SbmdDriverTest`, `SbmdFactoryTest`, `SbmdDispatchTest`, `SbmdHandlerInvokerTest`, `sbmdPrerequisitesTest`, `SbmdObservabilityTest`) and fix regressions. (unit)
- [x] 6.2 Run the SBMD integration tests to confirm claiming, endpoint resolution, resource binding, dispatch, and deferred operations are unaffected end-to-end. (integration, Docker) — full suite: 62 passed.
- [x] 6.3 Manually verify (or via test instrumentation) that at steady state with no matching devices, unused drivers hold only the claim stub — no source text, registration, JSValues, or dispatch tables. (covered by `SbmdDriverTest` shrink/stub assertions)
- [x] 6.4 Run `clang-format` on all changed C/C++ files and confirm the pre-commit hook passes.
