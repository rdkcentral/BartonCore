## 1. Transient Data

- [ ] 1.1 Implement main-loop-confined `TransientDataStore` (no mutex, injectable clock, `Set`/`Get` with lazy eviction and remaining TTL/`Remove`/`Increment`/`PurgeDevice`, debug main-loop assertion) in `core/deviceDrivers/matter/sbmd/dal/plugins/` (≤3h)
- [ ] 1.2 Unit tests `SbmdDalTransientStoreTest.cpp`: before/at/after expiry, last write wins, delete, purge, increment from absent/expired, non-numeric counter rejected (≤2h)
- [ ] 1.3 Implement `TransientPlugin` + binding with `Atomic` capability: TTL-required writes, prior read exposes remaining TTL for D8 rollback, `AllocateSequence`, batch-wide conditional check before apply (`ConditionFailed`), `PurgeDevice` (≤4h)
- [ ] 1.4 Unit tests `SbmdDalTransientPluginTest.cpp` through a real `DalTransaction`: sequences survive abort, interleaved `createSession`-style allocations are distinct, conditional match/mismatch/expected-absent, nothing applied on mismatch, rollback restores remaining TTL (≤3h)

## 2. Resources, Metadata, Properties

- [ ] 2.1 Implement `ResourceMetadataPlugin` (Resource and Metadata scopes; path parsing; `resourceDestroy`/`free` of results; `updateResource`/`deviceServiceSetMetadata`; delete rejected; failure → `Internal`) (≤4h)
- [ ] 2.2 Unit tests `SbmdDalResourceMetadataPluginTest.cpp` with mocked device-service functions (≤3h)
- [ ] 2.3 Implement `PropertyPlugin` over `deviceServiceGetSystemProperty`/`deviceServiceSetSystemProperty` (≤2h)
- [ ] 2.4 Unit tests `SbmdDalPropertyPluginTest.cpp` (≤1h)

## 3. Matter Attribute Plugin

- [ ] 3.1 Implement `MatterAttributePlugin` (path parsing, `CachedReader` using `GetCachedAttributeData` + `EncodeTlvElementToBase64(reader, 256)`, device lookup under `devicesMutex`, read-only) — `BCORE_MATTER` (≤3h)
- [ ] 3.2 Unit tests `SbmdDalMatterAttributePluginTest.cpp`: parsing, fake reader, `NotWritable`, overlay precedence via a real `DalTransaction` (≤2h)
- [ ] 3.3 Register in `MatterDeviceDriver::AddDevice` after successful emplace; unregister (non-blocking) in `DeviceRemoved` (inside the existing `RunOnMatterSync`, before `devices.erase`) and in `Shutdown` (before `devices.clear()`); SBMD override posts `TransientPlugin::PurgeDevice` to the main loop on removal (≤4h)
- [ ] 3.4 Unit tests for registration/unregistration, abort-on-removal of bound transactions, and posted transient purge using the Matter driver test fixtures (≤3h)

## 4. Registration and Switch-over

- [ ] 4.1 Implement `SbmdDalPlugins` to register/unregister the property, resource, metadata, and transient plugins with SBMD driver startup/shutdown, before the orchestrator is constructed (≤2h)
- [ ] 4.2 Unit test registration order and clean unregistration at shutdown (no plugins left registered) (≤1h)
- [ ] 4.3 Configure the orchestrator with `DalSbmdDataAccess` (same shared `ThreadHop`) instead of `LegacySbmdDataAccess`; delete `SpecBasedMatterDeviceDriver::transientStore`, `TransientEntry`, `SetTransientData`, `GetTransientData`, `MakeTransientFetcher`, `MakeTransientSetter` (≤3h)
- [ ] 4.4 Delete `LegacySbmdDataAccess.{h,cpp}` entirely (no fallback), its unit tests, its CMake entries, and legacy fetchers left without callers; verify by build that no reference remains (≤2h)
- [ ] 4.5 Run the shared `SbmdDataAccess` contract test suite (owned by BARTON-423) against `DalSbmdDataAccess` with the real plugins and remove its `LegacySbmdDataAccess` target; port any remaining transient unit tests from the deleted code (≤3h)

## 5. Integration and Docs

- [ ] 5.1 Integration test (Matter test harness): commission a device, read an attribute through the DAL, remove the device, verify abort with `aborted` and plugin removal (≤4h, requires Docker)
- [ ] 5.2 Run the full SBMD unit and integration suites with `DalSbmdDataAccess`; confirm no driver-visible change (≤2h, requires Docker)
- [ ] 5.3 Update `docs/SBMD.md` with the plugin table (incl. `Atomic`), transient TTL semantics, sequence counters and conditional writes, and the attribute-write overlay (≤2h)
