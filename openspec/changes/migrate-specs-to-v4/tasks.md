## 1. Tooling setup

- [x] 1.1 Vendor the `gh-stack` agent skill into `.github/skills/gh-stack/` (`SKILL.md` + `references/{commands,stack-design,troubleshooting}.md`) from `github/gh-stack`, since `gh skill install` is unavailable in this environment

## 2. Migrate specs to v4 structure

- [x] 2.1 Flatten the `agent-skill-*` specs (build, debug, format-code, integration-tests, matter-devices, unit-tests, validate-sbmd): merge delta headers into `## Requirements`, add a `# Title` and a hand-written `## Purpose`
- [x] 2.2 Flatten the `sbmd-*` specs (sbmd-system, sbmd-resource-prerequisites, sbmd-script-execution-limits, sbmd-seed-from-attribute, sbmd-v4-light-driver, sbmd-v4-runtime); for `sbmd-script-execution-limits` and `sbmd-system`, collapse both `## MODIFIED` and `## ADDED` sections into one `## Requirements`
- [x] 2.3 Flatten the `matter*`/`matterjs-*` specs (matter-subsystem, matter-test-infrastructure, matter-thermostat-sbmd, matter-thermostat-testing, matterjs-door-lock-device, matterjs-virtual-device-framework)
- [x] 2.4 Flatten the remaining capability specs (build-system, changelog-generation, core-services, device-drivers, device-type-endpoint-resolution, endpoint-cluster-fallback, observability-metrics, public-api, python-sideband-client, release-workflow, resource-model, temperature-humidity-sbmd-drivers, thread-subsystem, vendor-product-claiming, zigbee-subsystem)
- [x] 2.5 Reword requirements that lack a `SHALL`/`MUST` keyword: `sbmd-runtime-observability` (Subsystem metrics initialization) and `device-drivers` (Config restore / System event / Additional driver callbacks); add missing scenarios to `device-drivers` and `public-api` requirements
- [x] 2.6 Verify no requirement or scenario text changed meaning during reformatting (structural-only diff review per spec)

## 3. Build image provisioning

- [x] 3.1 Add a pinned `openspec` CLI install to `docker/Dockerfile` after the Node.js 22 stage (`npm install -g openspec@1.4.1`)
- [x] 3.2 Bump `docker/version` (2.14 → 2.17)
- [ ] 3.3 Rebuild the builder image and confirm `openspec --version` reports the pinned version inside it

## 4. Enforcement

- [x] 4.1 Add `.github/workflows/validate-openspec.yaml` running `openspec validate --specs --strict --no-interactive` over all specs on `pull_request`, failing on any error (Apache-2.0 header, mirroring existing workflow style)
- [x] 4.2 Add a `pre-commit` hook (wired through `hooks/`) that runs `openspec validate --specs --strict` and aborts the commit on failure
- [x] 4.3 Confirm strict validation returns non-zero on a deliberately-malformed spec (the command the hook and CI both run)

## 5. Verify

- [x] 5.1 Run `openspec validate --specs --strict` at repo root — all 35 specs valid, exit 0
- [x] 5.2 Run `openspec validate migrate-specs-to-v4 --type change --strict` to confirm this change's own artifacts validate
- [ ] 5.3 Open PR #1 (`gh stack`), leaving `consolidate-specs` as the stacked follow-up
