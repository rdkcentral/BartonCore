## 1. Core types (internal API)

- [ ] 1.1 Add `core/deviceDrivers/matter/sbmd/dal/DataKey.{h,cpp}` (`Namespace`, `AttributePath`, `ResourcePath`, `NamedKey`, `DataKey`, `DataKeyHash`) and `VersionedValue.h` (`Value`, `ValueHandle`, `VersionedValue`, `Entry`, `MakeString`, `MakeTlv`) per design D3/D4 (~2h)
- [ ] 1.2 Unit tests `core/test/src/SbmdDalKeyTest.cpp`: equality/hash across namespaces, no cross-namespace collision, value immutability (~1h)

## 2. DataStore

- [ ] 2.1 Implement `DeviceRegistry` and `DataStore` (`Get`, `Find`, `Put`, `Hydrate`, `Invalidate`, `EraseDevice`, observer, debug owner assertion) per D5 (~3h)
- [ ] 2.2 Unit tests `core/test/src/SbmdDalStoreTest.cpp`: monotonic per-key versions, same-value suppression, tombstone versioning, lazy TTL expiry bumps version, `EraseDevice`, captured handle survives write (~3h)

## 3. Ingress queue

- [ ] 3.1 Implement `IngressQueue` per D6 with injectable poster/owner predicate (~2h)
- [ ] 3.2 Unit tests: FIFO, coalesced drain scheduling, inline apply on owner after draining, poster failure recovery; multi-thread stress test (run under TSan in Docker) (~3h)

## 4. Data sources

- [ ] 4.1 Implement `DataSource`, `AttributeCacheSource` (via `MatterDevice::GetCachedAttributeData`), `ResourceSource` (`deviceServiceGetResourceById`/`updateResource` with thread-local echo suppression), `PersistentDataSource` (`deviceServiceGetMetadata`/`deviceServiceSetMetadata`, `sbmd.` prefix) per D7 (~4h)
- [ ] 4.2 Unit tests with mocked deviceService/MatterDevice: hydration, write-through, echo suppression, absent values (~3h)

## 5. DataAccessLayer façade

- [ ] 5.1 Implement `DataAccessLayer` (`Read`, `Write`, `ReconcileAttribute`, `Apply`, metrics) per D9 (~3h)
- [ ] 5.2 Unit tests `core/test/src/SbmdDalTest.cpp`: hydrate-on-miss, write-through + version bump, `ttl==0` delete, reconciliation of each `ReconcileEvent` kind (~3h)

## 6. Core service reconciliation hook (C, core/src)

- [ ] 6.1 Add internal `core/src/sbmdDataObserver.{h,c}` (`sbmdDataObserverSet`, `sbmdDataObserverNotify`, atomic callback pointer) per D8 (~1h)
- [ ] 6.2 Call `sbmdDataObserverNotify` from `updateResource` (on value change only), `deviceServiceSetMetadata` (`sbmd.` keys only), and device removal in `core/src/deviceService.c` (~2h)
- [ ] 6.3 CMocka/GTest coverage: notify fires on change, not on same value, not for non-`sbmd.` metadata, no-op without observer (~2h)

## 7. SBMD runtime integration

- [ ] 7.1 Construct and bind the DAL in SBMD runtime init (inject `SystemLayer().ScheduleLambda` and owner predicate; install the C observer); wire sources into the build under `BCORE_MATTER` (~3h)
- [ ] 7.2 Re-plumb `MakeAttrFetcher`, `MakeResFetcher`, `MakePersistFetcher`, `MakeTransientFetcher`, `MakeTransientSetter`, and `SbmdHandlerInvoker::ExecuteOps` writes through the DAL; remove `transientStore`/`TransientEntry`; call `ReconcileAttribute` in `HandleAttributeReport` (~4h)
- [ ] 7.3 Update `TestableSpecBasedMatterDeviceDriver` / `SbmdDriverTestBase.h`; run all existing SBMD unit suites unchanged (`SbmdDriverTest`, `SbmdHandlerInvokerTest`, `SbmdResultExecutorTest`, `SbmdCameraWebrtcTest`, `sbmdPrerequisitesTest`) (~3h)
- [ ] 7.4 Export `DalMetrics` via existing SBMD metrics/observability path; unit test metric counters (~2h)

## 8. Integration verification (Docker)

- [ ] 8.1 Run the full pytest integration suite in Docker (`testing/test/`) to confirm no driver-visible behavior change (~2h, requires Docker)

## 9. Documentation

- [ ] 9.1 Add `docs/SBMD.md` §2.4 "Data Access Layer" and `core/deviceDrivers/matter/sbmd/dal/README.md` (namespaces, versions, ingress, hook points, open questions) (~2h)
