## 1. Governance

- [x] 1.1 Add the spec-organization governance rule to `openspec/config.yaml` (context section + `rules.proposal` + `rules.specs`): specs are per capability or per device class, never per driver

## 2. SBMD framework consolidation

- [x] 2.1 Create `sbmd-runtime` from `sbmd-system` + `sbmd-v4-runtime` + `sbmd-script-execution-limits`
- [x] 2.2 Create `sbmd-mappers` from `sbmd-resource-prerequisites` + `sbmd-seed-from-attribute`
- [x] 2.3 Create `sbmd-endpoint-resolution` from `device-type-endpoint-resolution` + `endpoint-cluster-fallback`
- [x] 2.4 Rename `vendor-product-claiming` to `sbmd-claiming`

## 3. Device-class reframe

- [x] 3.1 Reframe `sbmd-v4-light-driver` → `device-class-light` (class contract; drop driver-file and integration-test requirements)
- [x] 3.2 Reframe `matter-thermostat-sbmd` → `device-class-thermostat` (retitle + repurpose; preserve resource/binding requirements)
- [x] 3.3 Reframe `temperature-humidity-sbmd-drivers` → `device-class-environmental-sensor` (rewrite to class-contract level; drop per-driver-file and test requirements)

## 4. Cross-cutting consolidation

- [x] 4.1 Create `agent-skills` from the 7 `agent-skill-*` specs
- [x] 4.2 Create `release-process` from `changelog-generation` + `release-workflow`
- [x] 4.3 Create `matter-testing` from `matter-test-infrastructure` + `matterjs-virtual-device-framework` + `matterjs-door-lock-device` + `matter-thermostat-testing` + `python-sideband-client`

## 5. Verify

- [x] 5.1 Delete all consumed source spec directories
- [x] 5.2 Run `openspec validate --specs --strict` — all specs valid, exit 0 (35 → 20)
- [ ] 5.3 Submit PR #2 stacked on #258 via `gh stack`
