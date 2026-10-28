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
- [x] 3.4 Reframe `matter-door-lock-critical-events` → `device-class-door-lock` (retitle + repurpose; requirements unchanged)
- [x] 3.5 Reframe `matter-sensor-critical-events` → `device-class-sensor` (retitle + repurpose; drop the two driver-version requirements)

## 4. Cross-cutting consolidation

- [x] 4.1 Create `agent-skills` from the 7 `agent-skill-*` specs
- [x] 4.2 Create `release-process` from `changelog-generation` + `release-workflow`
- [x] 4.3 Create `integration-testing` from `matter-test-infrastructure` + `matterjs-virtual-device-framework` + `matterjs-door-lock-device` + `matter-thermostat-testing` + `python-sideband-client` + `onvif-mock-device`

## 5. Camera reconciliation

- [x] 5.1 Rename `camera-session-lifecycle` → `device-class-camera`; update purpose to cover SBMD and native drivers and reference both protocol specs
- [x] 5.2 Rename `onvif-camera-driver` → `onvif-camera-protocol` (retitle + repurpose; requirement bodies unchanged)

## 6. Verify

- [x] 6.1 Delete all consumed source spec directories
- [x] 6.2 Run `openspec validate --specs --strict` — all specs valid, exit 0 (43 → 27)
- [x] 6.3 Submit as a standalone PR (rebase onto `main` after #258 merges)
