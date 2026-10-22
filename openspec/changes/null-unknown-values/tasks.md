## 1. BartonCommon prerequisites (separate repo; must also land in Zilker via XHCPE before BartonCore depends on new symbols)

- [ ] 1.1 Document NULL-input/failure semantics of `simpleProtectConfigData`/`simpleUnprotectConfigData` (2h)
- [ ] 1.2 Add `simpleProtectConfigDataEx`/`simpleUnprotectConfigDataEx` with `SimpleProtectStatus`; fix session leak at `simpleProtectConfig.c:74` (3h)
- [ ] 1.3 Unit tests for NULL input, failure, and round trip in `libs/config/c/test` (2h)

## 2. Storage and encryption (core)

- [ ] 2.1 `resourceToJSON`/`resourceFromJSON`: explicit `null`, skip encryption for NULL, fail closed on encryption failure, drop misleading warning, legacy plaintext migration (4h)
- [ ] 2.2 Handle `NULL` return from `resourceToJSON`/`resourcesToJSON` in all device save paths (2h)
- [ ] 2.3 Tests: fix `__wrap_simpleProtectConfigData` NULL handling in `jsonDatabaseTest.c`; add NULL/""/value and encryption-failure round-trip tests (3h)
- [ ] 2.4 Metadata JSON: `metadataToJSON` explicit `null`; `metadataFromJSON` accept NULL and `""` (2h)
- [ ] 2.5 Tests for metadata round trip including legacy files (2h)
- [ ] 2.6 System properties: `stringHashMapToJson` explicit `null`; `loadSystemProperties` accept `null`; Get uses `stringHashMapContains`; GetAll consistent (3h)
- [ ] 2.7 Tests for system-property round trip; port disabled `deviceServiceTest.c:308-312` case (2h)

## 3. Core semantics

- [ ] 3.1 `deviceServiceWriteResourceNoPattern` passes NULL through to drivers and logs driver rejection (1h) + core test that writes NULL to every writable resource of each reference device class and asserts no crash (3h)
- [ ] 3.2 `setMetadata` accept NULL (`stringCompare`, `strdupOpt`); `jsonDatabaseSaveMetadata` `strdupOpt`; `deviceServiceSetMetadata` return real result; add delete (4h)
- [ ] 3.3 Tests for 3.2 including metadata-updated event on value↔NULL (3h)
- [ ] 3.4 `deviceServiceGetDevicesByMetadata` existence matches NULL-valued entries (1h) + test (1h)
- [ ] 3.5 Add `deviceServiceDeleteSystemProperty` and `deviceServiceDeleteMetadata` (2h) + tests (2h)
- [ ] 3.6 `icInitialResourceValues` callers use `Has*` before `Get*` (2h) + tests (2h)

## 4. Public API (api/c) — separate from implementation above

- [ ] 4.1 Add `RESOURCE_NOT_FOUND`, `METADATA_NOT_FOUND`, `BCoreSystemPropertyError` enums (1h)
- [ ] 4.2 `b_core_client_read_resource`/`read_metadata` set not-found errors (2h) + tests (2h)
- [ ] 4.3 `b_core_client_read_system_property`, `delete_system_property`, `delete_metadata`; allow NULL in `write_resource`/`write_metadata`/`set_system_property` (3h) + tests (3h)
- [ ] 4.4 `(nullable)` annotations on value getters/properties/event props; regenerate GIR and verify (2h)
- [ ] 4.5 Reference app prints `<unknown>` (`reference/src/coreCategory.c:305,350`) (1h)

## 5. Drivers (BCORE_ZIGBEE, BCORE_PHILIPS_HUE, BCORE_MATTER, BCORE_ONVIF)

- [ ] 5.1 Philips Hue NULL guards (`philipsHueDeviceDriver.c:142,345,382,387,463`) (2h) + tests (2h)
- [ ] 5.2 `zigbeeDriverCommon.c:2122` NULL guard (1h) + test (1h)
- [ ] 5.3 Audit Onvif/Matter/SBMD `writeResource` for NULL `newValue` and `previousValue`; decide accept vs reject per resource (3h) + tests (2h)
- [ ] 5.5 SBMD write path: pass explicit JS `null` as `args.resource.input` for NULL writes (`SbmdHandlerInvoker.cpp:148-157` currently omits it) (2h) + tests (2h)
- [ ] 5.4 `MatterDeviceDriver.cpp:1545` and other `IsNull()` collapses → NULL (3h) + tests (2h)

## 6. SBMD (BCORE_MATTER)

- [ ] 6.1 `GetNullableStringProp`; result structs to `std::optional<std::string>` (4h)
- [ ] 6.2 `SpecBasedMatterDeviceDriver.cpp` and `SbmdHandlerInvoker.cpp` pass NULL through; missing `value` → error (3h)
- [ ] 6.3 Unit tests for null/missing/"" (3h)
- [ ] 6.4 Audit shipped `.sbmd` scripts for `value: ""`/`undefined` returns that mean unknown (3h)
- [ ] 6.5 Integration test with Matter virtual device reporting null attribute (requires Docker) (4h)

## 7. Documentation

- [ ] 7.1 `docs/USING_BARTON_GUIDE.md` "Value unknown" section (2h)
- [ ] 7.2 `docs/SBMD.md` `value: null` contract (1h)
- [ ] 7.3 Architecture doc: encryption fail-closed (1h)
- [ ] 7.4 CHANGELOG breaking-behavior note for not-found errors (1h)
