## Why

The flat `openspec/specs/` list had grown to 43 specs of wildly inconsistent granularity — some describe an entire subsystem, others a single device driver. With spec-based Matter drivers (SBMD) expected to scale to 50–100+, a per-driver spec model would explode the flat namespace: OpenSpec spec discovery is exactly one directory deep, so subfolders are not an option (nested `spec.md` files are silently ignored). We need an organization whose spec count is bounded by capabilities and device classes, not by the driver catalog.

## What Changes

- **Governance rule (the durable fix):** encode in `openspec/config.yaml` that OpenSpec specs are authored per capability or per device class — never per concrete driver. A new supported device is added as a `.sbmd.js` driver file governed by the SBMD schema validator (`scripts/ci/validate_sbmd_v4_specs.py`), not as a new spec. This keeps the spec count bounded (~15–25) as drivers scale to 100+.
- **Consolidate 43 → 27 specs** (requirement meaning preserved):
  - **SBMD framework**: `sbmd-runtime` (←`sbmd-system` + `sbmd-v4-runtime` + `sbmd-script-execution-limits`), `sbmd-mappers` (←`sbmd-resource-prerequisites` + `sbmd-seed-from-attribute`), `sbmd-endpoint-resolution` (←`device-type-endpoint-resolution` + `endpoint-cluster-fallback`), `sbmd-claiming` (←`vendor-product-claiming`).
  - **Device classes**: `device-class-light` (←`sbmd-v4-light-driver`), `device-class-thermostat` (←`matter-thermostat-sbmd`), `device-class-environmental-sensor` (←`temperature-humidity-sbmd-drivers`), `device-class-door-lock` (←`matter-door-lock-critical-events`), `device-class-sensor` (←`matter-sensor-critical-events`, dropping its two driver-version requirements) — reframed from per-driver to class-contract level.
  - **Cross-cutting**: `agent-skills` (←7 `agent-skill-*`), `release-process` (←`changelog-generation` + `release-workflow`), `integration-testing` (←`matter-test-infrastructure` + `matterjs-virtual-device-framework` + `matterjs-door-lock-device` + `matter-thermostat-testing` + `python-sideband-client` + `onvif-mock-device`).
  - **Camera**: `device-class-camera` (←`camera-session-lifecycle`, purpose updated to cover SBMD and native drivers), `onvif-camera-protocol` (←`onvif-camera-driver`, retitled as the ONVIF protocol capability); `webrtc-signaling-endpoint` and `camera-stream-reference-command` unchanged.
- The device-class reframes **drop** requirements that were about a driver *file* existing or about integration tests passing; those concerns are now owned by the `.sbmd.js` files + the SBMD schema validator + the `integration-testing` spec.

## Capabilities

### New Capabilities
<!-- This change reorganizes existing capabilities into consolidated/renamed specs; it does not introduce new system behavior. The consolidated spec files (sbmd-runtime, device-class-*, agent-skills, etc.) carry forward existing requirements verbatim (or, for device-class reframes, at the class-contract level). No delta specs are authored because no requirement's meaning changes. -->

### Modified Capabilities
<!-- None. Requirement semantics are unchanged; only spec file organization and (for device classes) framing change. -->

## Impact

- **Specs**: `openspec/specs/` goes 43 → 27 directories.
- **Config**: `openspec/config.yaml` gains the spec-organization governance rule (context + proposal/specs rules) that the propose/apply skills inject.
- **No** code, build, CI, API, or runtime impact — documentation/organization only. The `spec-validation` CI gate from the base change (PR #258) keeps all 27 specs strictly valid.

## Non-goals

- Authoring brand-new device-class specs for classes that had no prior OpenSpec coverage (`airQualitySensor`), or extending `device-class-door-lock` and `device-class-sensor` beyond the critical-event requirements they inherit. Their drivers exist as `.sbmd.js` files under the SBMD validator; a class-contract spec can be added later per the governance rule when warranted.
- Changing any requirement's meaning, or altering drivers, tooling, or the public API.
- Subdirectory organization of specs — proven unsupported by OpenSpec discovery.
