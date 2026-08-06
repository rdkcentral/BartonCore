## Why

OpenSpec v4's `openspec validate` enforces a structural contract for specs (a `## Purpose` section, a `## Requirements` section, and at least one `#### Scenario:` per requirement). All 35 checked-in specs under `openspec/specs/` were authored under the older delta format and currently fail `openspec validate --specs --strict` (35/35 failing). Without a green baseline and automated enforcement, spec structure silently drifts and validation provides no guardrail.

## What Changes

- Migrate all 35 specs in `openspec/specs/` to the v4 main-spec structure so `openspec validate --specs --strict` passes:
  - Flatten 34 delta-format specs (`## ADDED Requirements` / `## MODIFIED Requirements` → a single `## Requirements`), and add a top-level title plus a `## Purpose` section to each.
  - Reword the one requirement in `sbmd-runtime-observability` that lacks a `SHALL`/`MUST` keyword.
- Install the `openspec` CLI in the Docker builder image (pinned version) and bump the image version, so validation runs identically for developers and in CI.
- Add a CI workflow that runs `openspec validate --specs --strict` over all specs and fails the build on any error.
- Add a `pre-commit` hook that runs the same strict validation locally before each commit.
- Install the `gh-stack` agent skill under `.github/skills/gh-stack/` (the `gh skill install` command is unavailable in this environment), enabling the stacked-PR workflow this effort uses.

## Capabilities

### New Capabilities
- `spec-validation`: Specs MUST conform to the OpenSpec v4 structure and pass `openspec validate --specs --strict`. This invariant is enforced at three points: the Docker builder image ships a pinned `openspec` CLI, a CI workflow validates all specs on every pull request, and a pre-commit hook validates before each local commit.

### Modified Capabilities
<!-- None. The 34 spec rewrites are format-only migrations that preserve every existing requirement's meaning verbatim; no requirement semantics change, so no delta specs are required for them. -->

## Impact

- **Specs**: all 35 `openspec/specs/*/spec.md` files rewritten (structure only; requirement text preserved).
- **Build image**: `docker/Dockerfile` gains a pinned `openspec` install (Node.js 22 is already present); `docker/version` bumped (2.14 → 2.17, chosen to avoid collision with in-flight builder bumps on parallel branches). Consumers must repull the builder image.
- **CI**: new `.github/workflows/validate-openspec.yaml`.
- **Hooks**: new strict-validation `pre-commit` hook wired through `hooks/`.
- **Tooling**: new `.github/skills/gh-stack/` skill (MIT-licensed, vendored from `github/gh-stack`).
- **No** C/C++/CMake code is affected and **no** CMake feature flags are relevant; there is no runtime, library, or public-API impact.

## Non-goals

- Consolidating or renaming specs to reduce the ever-growing flat list of varying-scope directories — deferred to a stacked follow-up PR (`consolidate-specs`) built on top of this change.
- Organizing specs into subdirectories — proven unsupported (OpenSpec spec discovery is flat, one directory level deep; nested `spec.md` files are silently ignored).
- Changing the meaning of any existing requirement, introducing new device/product functionality, or altering the public API.
- Scoping the pre-commit hook to only changed specs — unnecessary, since a full strict run of all specs completes in ~0.2s (Node startup dominated).
