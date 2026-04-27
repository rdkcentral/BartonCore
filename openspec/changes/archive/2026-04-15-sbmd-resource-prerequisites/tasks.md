## 1. Data Model — SbmdAlias, SbmdPrerequisite, SbmdMatterMeta

- [x] 1.1 ~~Add `SbmdPrerequisite` struct to `SbmdSpec.h`: `mapperRef` (`std::optional<std::string>`), `clusterId` (`uint32_t`), `attributeIds` (`std::vector<uint32_t>`)~~ → Revised: remove `mapperRef`; struct is `{uint32_t clusterId; std::vector<uint32_t> attributeIds;}` only — all resolution is at parse time
- [x] 1.2 ~~Add `prerequisites` field to `SbmdResource` in `SbmdSpec.h`: `std::optional<std::vector<SbmdPrerequisite>>`~~ → Revised: changed to plain `std::vector<SbmdPrerequisite>`; parse-time enforcement uses a local bool
- [x] 1.3 Add `SbmdAlias` struct to `SbmdSpec.h`: `std::string name`, `std::optional<SbmdAttribute> attribute`, `std::optional<SbmdEvent> event`
- [x] 1.4 Add `std::vector<SbmdAlias> aliases` field to `SbmdMatterMeta` in `SbmdSpec.h`

## 2. Parser Tests (write before implementation)

- [x] 2.1 ~~`test_prerequisiteFromReadMapper`: verify `mapperRef = "read"`~~ → Revised: `test_prerequisiteFromAttributeAlias`: resource with `prerequisites: [{alias: measuredValue}]` and an attribute alias `measuredValue` in `matterMeta.aliases` — verify `prerequisites` vector has one entry with `clusterId` and `attributeIds` resolved from the alias
- [x] 2.2 ~~`test_prerequisiteFromEventMapper`: verify `mapperRef = "event"`~~ → Revised: `test_prerequisiteFromEventAlias`: resource with `prerequisites: [{alias: lockOp}]` and an event alias `lockOp` — verify `clusterId` resolved, empty `attributeIds`
- [x] 2.3 ~~`test_prerequisiteExplicitClusterOnly`~~ → Removed: explicit `clusterId` form no longer exists; replace with `test_readMapperResolvesFromAlias`: alias in `matterMeta.aliases`, `mapper.read.alias: <name>` — verify `readAttribute` is populated from alias
- [x] 2.4 ~~`test_prerequisiteExplicitClusterAndAttribute`~~ → Removed: alias form replaces this; covered by 2.1
- [x] 2.5 Add test `test_prerequisiteNone`: `prerequisites: none` — verify `prerequisites` holds an empty vector (not `nullopt`)
- [x] 2.6 Add test `test_prerequisiteMissingOnReadMapper`: resource with `mapper.read` and no `prerequisites` — verify `ParseString()` returns `nullptr`
- [x] 2.7 Add test `test_prerequisiteMissingOnEventMapper`: resource with `mapper.event` and no `prerequisites` — verify `ParseString()` returns `nullptr`
- [x] 2.8 ~~Add test `test_prerequisiteNotRequiredForWriteMapper`: resource with only `mapper.write`, no `prerequisites` — verify parsing succeeds~~ → Revised: design changed to require `prerequisites` on all resources; test now asserts parsing FAILS
- [x] 2.9 ~~`test_prerequisiteInvalidBothForms`~~ → Revised: `test_prerequisiteUnknownAlias`: prerequisite entry with `alias: nonExistentName` — verify `ParseString()` returns `nullptr`
- [x] 2.10 TDD process step — moot; all parser tests pass
- [x] 2.11 Covered by `test_prerequisiteFromEventMapper`, which verifies `SbmdMapper.event` is populated from an event alias
- [x] 2.12 Covered by `test_sbmdParserInvalidAttributeType`, which verifies an alias with both `attribute` and `event` is rejected

## 3. Parser Implementation

- [x] 3.1 ~~Add `ParsePrerequisites()` static method~~ → Revised: update `ParsePrerequisites()` signature to accept `const std::vector<SbmdAlias> &aliases`; replace `from`/`clusterId` form handling with alias lookup
- [x] 3.2 ~~Implement `ParsePrerequisites()` handling `from`/`clusterId` forms~~ → Revised: parse `alias: <name>` key; look up alias; resolve to `clusterId`+`attributeIds`; reject unknown alias names
- [x] 3.3 ~~In `SbmdParser::ParseResource()`, call `ParsePrerequisites()` when `prerequisites` node is present; enforce `prerequisites` present on read/event resources~~ → Revised: enforcement extended to all resources regardless of mapper type
- [x] 3.4 Run parser tests — all new tests should now pass
- [x] 3.5 Verify no regressions in existing parser tests
- [x] 3.6 Add `ParseAlias(const YAML::Node &node, SbmdAlias &alias)` static method to `SbmdParser.h` and `SbmdParser.cpp`; implement: require `name`, require exactly one of `attribute` or `event`
- [x] 3.7 In `ParseMatterMeta()`, parse `aliases` sequence using `ParseAlias()`
- [x] 3.8 Update `ParseYamlNode()` to pass `spec->matterMeta.aliases` to `ParseResource()` and `ParseEndpoint()`
- [x] 3.9 Update `ParseEndpoint()` and `ParseMapper()` signatures to accept `const std::vector<SbmdAlias> &aliases`
- [x] 3.10 In `ParseMapper()`, handle `read.alias: <name>` — look up alias → if attribute alias, populate `readAttribute`; handle `event.alias: <name>` — look up alias → if event alias, populate `mapper.event`; remove `read.attribute:` inline block and `event.event:` inline block support

## 4. Runtime Tests (write before implementation)

- [x] 4.1 `test_resourceSkippedWhenClusterAbsent`: explicit `SbmdPrerequisite{clusterId: 0x0405}` in test — verify resource skipped
- [x] 4.2 `test_resourceRegisteredWhenClusterPresent`: cache has cluster; verify resource configured
- [x] 4.3 `test_resourceSkippedWhenAttributeAbsent`: cache has cluster but not attribute; verify resource skipped
- [x] 4.4 `test_resourceRegisteredWhenClusterAndAttributePresent`: cache has both; verify resource configured
- [x] 4.5 ~~`test_mapperRefFromRead`~~ → Revised: test `test_aliasResolvedPrerequisiteRegisters`: resource with `SbmdPrerequisite{clusterId: 0x0405, attributeIds: [0x0000]}` (already-resolved form); cache has cluster+attribute; verify resource configured — validates that `CheckPrerequisites()` works with pre-resolved prerequisites
- [x] 4.6 `test_prerequisiteNoneAlwaysRegisters`: resource with `prerequisites: none`; cache empty; verify resource configured
- [x] 4.7 `test_allPrerequisitesMustBeSatisfied`: resource with two prerequisites; only first satisfied; verify resource skipped
- [x] 4.8 TDD process step — moot; all runtime tests pass
- [x] 4.9 ~~Add test `RequiredResourceUnmetPrerequisitesFailsCommissioning`~~ → Renamed: `UnmetPrerequisitesGateRegardlessOfOptionality` — tests that unmet prerequisites gate the resource regardless of whether it is optional or required (gates in both cases; behavior differs only in whether commissioning continues)

## 5. Runtime Implementation

- [x] 5.1 Add static method `CheckPrerequisites(const SbmdResource &resource, const MatterDevice &device) -> bool` declaration to `SpecBasedMatterDeviceDriver.h`
- [x] 5.2 ~~Implement `CheckPrerequisites()` with `mapperRef` resolution block~~ → Revised: remove `mapperRef` resolution block from `CheckPrerequisites()`; prerequisites are already resolved at parse time — iterate `clusterId`/`attributeIds` directly
- [x] 5.3 In `AddDevice()`, call `CheckPrerequisites()` with optional/required branching — already done
- [x] 5.4 Run runtime tests — all new tests should now pass
- [x] 5.5 Verify no regressions in existing driver tests
- [x] 5.6 Confirmed: name is still appropriate — resources in this map are always optional; required resources with unmet prerequisites abort without being tracked

## 6. JSON Schema Update

- [x] 6.1 ~~Add `prerequisite` $def with `from`/`clusterId` forms~~ → Revised: update `prerequisite` $def to alias-only form: `{type: object, required: ["alias"], properties: {alias: {type: string}}}`
- [x] 6.2 Add `prerequisites` property to the `resource` $def _(done — shape unchanged)_
- [x] 6.3 Run schema validation against existing (soon-to-be-updated) spec files
- [x] 6.4 Add `alias` $def to `$defs`: `{type: object, required: ["name"], properties: {name: {type: string}, attribute: {$ref: "#/$defs/attribute"}, event: {$ref: "#/$defs/event"}}, oneOf: [{required: ["attribute"]}, {required: ["event"]}]}`
- [x] 6.5 Add `aliases` property to `matterMeta` $def: `{type: array, items: {$ref: "#/$defs/alias"}}`
- [x] 6.6 Update `readMapper` $def: replace `attribute:` + `oneOf[attribute|command]` with `alias: <name>` (the `alias` key, type string); keep `command:` as-is for read-via-command resources; update `oneOf` accordingly
- [x] 6.7 Update `eventMapper` $def: replace `event: {$ref: event}` with `alias: <name>` (string)
- [x] 6.8 Re-run schema validation against updated spec files to confirm all pass

## 7. SBMD Spec File Updates

- [x] 7.1 ~~`air-quality-sensor.sbmd`: add `prerequisites: [{from: read}]`~~ → Revised: add `matterMeta.aliases` for all 5 air-quality resources; switch prerequisites and mapper metadata to alias form
- [x] 7.2 ~~Audit other `.sbmd` files and add `prerequisites` fields~~ → Revised: update all other `.sbmd` files to use `matterMeta.aliases`; switch prerequisites and mapper metadata to alias form
- [x] 7.3 Run schema validation to confirm all updated specs pass (re-validate after schema updated in 6.8)

## 8. Build and Verification

- [x] 8.1 Build with `BCORE_MATTER=ON` and confirm compilation succeeds (requires Docker)
- [x] 8.2 Run full unit test suite (`ctest --test-dir build`) and confirm all tests pass including new ones — 193/193 passed
- [x] 8.3 Run integration tests (`scripts/ci/run_integration_tests.sh`) to confirm no regressions (requires Docker)
