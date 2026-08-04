## Context

This change stacks on top of PR #258 (`migrate-specs-to-v4`), which made all 35 specs pass `openspec validate --specs --strict`. That left the *organization* problem untouched: 35 flat spec directories of inconsistent scope. The forcing function is scale — SBMD device drivers are expected to grow to 50–100+, and OpenSpec discovery is flat (one directory deep), so neither per-driver specs nor subdirectories are viable.

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
- Consolidate the existing 35 specs into coherent, consistently-scoped capabilities.
- Encode the organizing principle so future changes cannot regress into per-driver sprawl.
- Keep `openspec validate --specs --strict` green throughout.

**Non-Goals:**
- Net-new capability documentation for undocumented device classes (doorLock, sensor, airQualitySensor).
- Any requirement-meaning change; any code/driver/API change.

## Decisions

**D1. A spec is a capability or a device class — never a driver.**
The driver *is* its `.sbmd.js` file, governed by the schema validator. Encoding this in `openspec/config.yaml` (context + `rules.proposal` + `rules.specs`) makes it a standing constraint the propose/apply skills inject. *Alternative:* rely on reviewer discipline — rejected; it does not scale and is what produced the current sprawl.

**D2. Merge fine-grained framework/cross-cutting specs by concatenating requirements.**
`sbmd-runtime`, `sbmd-mappers`, `sbmd-endpoint-resolution`, `sbmd-claiming`, `agent-skills`, `release-process`, and `matter-testing` are lossless concatenations of their sources' `## Requirements` under one new title + purpose. Requirement bodies are byte-preserved.

**D3. Reframe per-driver specs to device-class specs at the class-contract level.**
`device-class-light`, `device-class-thermostat`, and `device-class-environmental-sensor` describe what the *class* guarantees (endpoint profile, resources, Matter cluster/attribute bindings). Requirements that asserted a driver *file* exists, or that integration tests pass, are dropped — those are owned by the `.sbmd.js` files, the schema validator, and the `matter-testing` spec respectively. The `environmentalSensor` spec is rewritten (its source was driver- and test-centric); light and thermostat retain their accurate resource/binding requirements verbatim.

**D4. Do not fabricate specs for undocumented classes.**
`doorLock`, `sensor`, and `airQualitySensor` had no prior OpenSpec coverage. Rather than author rushed normative specs from driver files, they remain governed by their drivers + tooling; class-contract specs can be added later per D1 when warranted.

## Risks / Trade-offs

- **Reframing environmental-sensor could alter a stated fact.** → Requirements were rewritten directly from the source spec's technical content (clusters 0x0402/0x0405, int16/uint16, null sentinels 0x8000/0xFFFF); scenarios preserve the exact values.
- **Partial device-class coverage (3 of ~6 classes) looks inconsistent.** → Acceptable: the model and governance are established; the other classes were never documented as specs, so nothing is lost, and D1 makes completing them a routine follow-up.
- **Merged specs are larger.** → Still coherent (one capability each) and far more navigable than 35 fragments; strict validation keeps them well-formed.

## Migration Plan

1. Add the governance rule to `openspec/config.yaml`.
2. Concatenate the framework/cross-cutting merges (scripted, lossless).
3. Reframe the three device-class specs; delete consumed source directories.
4. Run `openspec validate --specs --strict` — must be green (20/20).
5. Commit and submit as PR #2 stacked on #258 via `gh stack`.

Rollback is trivial — documentation/organization only; reverting the branch restores the prior layout with no runtime effect.

## Open Questions

- Do we want class-contract specs for `doorLock`, `sensor`, and `airQualitySensor` now, or as follow-ups? (This change defers them.)
