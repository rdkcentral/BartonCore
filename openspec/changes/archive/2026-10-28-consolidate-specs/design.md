## Context

This change follows PR #258 (`migrate-specs-to-v4`), which made all specs pass `openspec validate --specs --strict`. That left the *organization* problem untouched: 43 flat spec directories (35 migrated, `spec-validation` from #258, plus 5 camera/ONVIF specs and 2 Matter critical-event specs since added on `main`) of inconsistent scope. The forcing function is scale — SBMD device drivers are expected to grow to 50–100+, and OpenSpec discovery is flat (one directory deep), so neither per-driver specs nor subdirectories are viable.

Key facts grounding the design:
- Each `.sbmd.js` driver declares exactly one `deviceClass`; the device-class set is bounded (~8: light, doorLock, thermostat, sensor, environmentalSensor, airQualitySensor, + planned windowCovering, presence, lightController).
- Concrete drivers are already validated by `scripts/ci/validate_sbmd_v4_specs.py` against a JSON schema at build time — a mechanism independent of OpenSpec.

```
   drivers        │ 10 ───────────────▶ 100+      (scales freely)
   .sbmd.js files │ core/deviceDrivers/matter/sbmd/specs/*.sbmd.js
   governed by    │ validate_sbmd_v4_specs.py + JSON schema   ← not OpenSpec
   ───────────────┼───────────────────────────────
   device classes │ ~8 ───────────────▶ ~10-15     (bounded)
   OpenSpec specs │ per capability + per device class (bounded ~15-25)
```

## Goals / Non-Goals

**Goals:**
- Bound OpenSpec spec count by capabilities and device classes, not drivers.
- Consolidate the existing 43 specs into coherent, consistently-scoped capabilities.
- Encode the organizing principle so future changes cannot regress into per-driver sprawl.
- Keep `openspec validate --specs --strict` green throughout.

**Non-Goals:**
- Net-new capability documentation for undocumented device classes (airQualitySensor), or filling gaps in partially documented ones (doorLock, sensor).
- Any requirement-meaning change; any code/driver/API change.

## Decisions

**D1. A spec is a capability or a device class — never a driver.**
The driver *is* its `.sbmd.js` file, governed by the schema validator. Encoding this in `openspec/config.yaml` (context + `rules.proposal` + `rules.specs`) makes it a standing constraint the propose/apply skills inject. *Alternative:* rely on reviewer discipline — rejected; it does not scale and is what produced the current sprawl.

**D2. Merge fine-grained framework/cross-cutting specs by concatenating requirements.**
`sbmd-runtime`, `sbmd-mappers`, `sbmd-endpoint-resolution`, `sbmd-claiming`, `agent-skills`, `release-process`, and `integration-testing` are lossless concatenations of their sources' `## Requirements` under one new title + purpose. Requirement bodies are byte-preserved.

**D3. Reframe per-driver specs to device-class specs at the class-contract level.**
`device-class-light`, `device-class-thermostat`, and `device-class-environmental-sensor` describe what the *class* guarantees (endpoint profile, resources, Matter cluster/attribute bindings). Requirements that asserted a driver *file* exists, or that integration tests pass, are dropped — those are owned by the `.sbmd.js` files, the schema validator, and the `integration-testing` spec respectively. The `environmentalSensor` spec is rewritten (its source was driver- and test-centric); light and thermostat retain their accurate resource/binding requirements verbatim.

**D4. Do not fabricate specs for undocumented classes.**
`airQualitySensor` has no OpenSpec coverage. Rather than author rushed normative specs from driver files, it remains governed by its driver + tooling; a class-contract spec can be added later per D1 when warranted. `doorLock` and `sensor` are covered only by the critical-event specs archived from `sbmd-matter-critical-events`; those are reframed into `device-class-door-lock` and `device-class-sensor` (see D7) without authoring additional requirements.

**D5. Camera specs are scoped by device class and by streaming protocol, not by driver.**
`device-class-camera` (←`camera-session-lifecycle`) owns the protocol-agnostic `ep/camera` contract and now states that camera drivers may be SBMD or native. Streaming protocols are a bounded set, so each protocol endpoint keeps its own capability spec: `webrtc-signaling-endpoint` (unchanged) and `onvif-camera-protocol` (←`onvif-camera-driver`, retitled and repurposed as the ONVIF protocol capability; requirement bodies byte-preserved). D3's reframe that drops driver-detail requirements does not apply here: the ONVIF driver is native, so there is no `.sbmd.js` file or schema validator to own that detail, and dropping it would change meaning. `camera-stream-reference-command` is a reference-app capability and is unchanged.

**D6. Test infrastructure is one capability regardless of protocol.**
`matter-testing` is renamed `integration-testing` and absorbs `onvif-mock-device` by lossless concatenation, the same way the per-device matter.js virtual devices were absorbed. A mock per device type is test tooling, not a capability that warrants its own spec.

**D7. Per-driver critical-event specs become device-class specs.**
`matter-door-lock-critical-events` and `matter-sensor-critical-events` (archived from `sbmd-matter-critical-events`) are reframed per D3 into `device-class-door-lock` and `device-class-sensor`: retitled and repurposed, behavior requirements byte-preserved. The two "SBMD driver version is 2" requirements (contact sensor, water leak detector) are dropped because driver versions are owned by the `.sbmd.js` files. The door lock endpoint `profileVersion` requirement is kept because it governs reconfiguration behavior that clients can observe.

## Risks / Trade-offs

- **Reframing environmental-sensor could alter a stated fact.** → Requirements were rewritten directly from the source spec's technical content (clusters 0x0402/0x0405, int16/uint16, null sentinels 0x8000/0xFFFF); scenarios preserve the exact values.
- **Partial device-class coverage (6 of ~7 classes, some only partially) looks inconsistent.** → Acceptable: the model and governance are established; the other classes were never documented as specs, so nothing is lost, and D1 makes completing them a routine follow-up.
- **Merged specs are larger.** → Still coherent (one capability each) and far more navigable than 43 fragments; strict validation keeps them well-formed.

## Migration Plan

1. Add the governance rule to `openspec/config.yaml`.
2. Concatenate the framework/cross-cutting merges (scripted, lossless).
3. Reframe the three device-class specs; delete consumed source directories.
4. Run `openspec validate --specs --strict` — must be green (27/27).
5. Commit and submit as a standalone PR once #258 has merged, rebased onto `main`.

Rollback is trivial — documentation/organization only; reverting the branch restores the prior layout with no runtime effect.

## Open Questions

- Do we want a class-contract spec for `airQualitySensor`, and fuller class contracts for `doorLock` and `sensor` beyond critical events, now or as follow-ups? (This change defers them.)
