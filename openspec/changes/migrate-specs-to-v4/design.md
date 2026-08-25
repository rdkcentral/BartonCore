## Context

OpenSpec v4 (CLI `1.4.1`, schema `spec-driven`) validates the *structure* of spec documents, not code. All 35 specs under `openspec/specs/` fail `openspec validate --specs --strict`:

| Bucket | Count | Symptom |
|--------|-------|---------|
| Delta-format (`## ADDED`/`## MODIFIED Requirements`) | 34 | No title, no `## Purpose`, no `## Requirements` |
| Structured but non-normative (`sbmd-runtime-observability`) | 1 | One requirement lacks `SHALL`/`MUST` |

Root cause: these main specs were populated with the delta syntax that belongs *inside* a change proposal, instead of the flattened main-spec form that `openspec archive` normally produces. An audit of the 34 delta specs found 32 pure `ADDED` and 2 with a `MODIFIED` section (`sbmd-script-execution-limits`, `sbmd-system`); neither MODIFIED spec contains a duplicate requirement name, so flattening is a safe, lossless transform.

```
   DELTA (authored form)                 MAIN SPEC (v4 validate expects)
   ┌──────────────────────────┐          ┌────────────────────────────┐
   │ ## ADDED Requirements     │  ──────▶ │ # <Capability Title>       │
   │ ### Requirement: Foo      │ flatten  │ ## Purpose                 │
   │ #### Scenario: …          │          │ <prose — hand written>     │
   │ ## MODIFIED Requirements  │          │ ## Requirements            │
   │ ### Requirement: Bar      │          │ ### Requirement: Foo / Bar │
   └──────────────────────────┘          │ #### Scenario: …           │
                                          └────────────────────────────┘
```

Node.js 22 is already installed in the builder image, so the `openspec` npm CLI can be added there. `docker/version` is currently `2.14`.

## Goals / Non-Goals

**Goals:**
- Every spec passes `openspec validate --specs --strict` (green baseline).
- Validation is enforced automatically (build image + CI + pre-commit) so structure cannot silently regress.
- Preserve every existing requirement's meaning verbatim during the reformat.

**Non-Goals:**
- Consolidating/renaming specs or reducing the flat directory count — deferred to a stacked follow-up (`consolidate-specs`).
- Subdirectory organization — unsupported by OpenSpec discovery.
- Any change to requirement semantics, runtime behavior, or the public API.

## Decisions

**D1. Flatten in place; treat `MODIFIED` and `ADDED` identically.**
Because these files are main specs (not deltas being archived), both `## ADDED Requirements` and `## MODIFIED Requirements` collapse into one `## Requirements` section. The audit confirmed no duplicate requirement names, so no content is lost. *Alternative considered:* run each spec back through a synthetic `openspec archive`; rejected as more complex and error-prone than a direct rewrite, and archive expects a change context these specs never had.

**D2. Write `## Purpose` by hand per spec.**
A script can insert an empty `## Purpose` to satisfy the validator, but that defeats the intent. Each purpose is authored from the spec's existing content. *Alternative:* auto-generate from the first requirement — rejected as low-value boilerplate.

**D3. Install `openspec` in the builder image, pinned; bump the image version.**
Provides one source of truth for the validation rules across dev and CI (mirrors how `cocogitto` is provisioned). Pin to the version used locally today (`1.4.1`). *Alternative:* `npx openspec` in CI only — rejected because dev and CI could drift and every run would re-download.

```
   docker/Dockerfile (after Node 22 stage)
   ┌──────────────────────────────────────────────┐
   │ RUN npm install -g openspec@<pinned>          │
   └──────────────────────────────────────────────┘
   docker/version:  2.14 → 2.17   (2.15/2.16 taken by parallel branches)
```

**D4. CI validates all specs, strict, on pull_request.**
New workflow `.github/workflows/validate-openspec.yaml` runs `openspec validate --specs --strict --no-interactive` inside the builder image and fails on any error. Validate *all* specs (not just changed) to enforce the whole-repo invariant. *Alternative:* changed-specs-only — rejected; a full strict run is ~0.2s and whole-set validation catches cross-spec issues.

**D5. Pre-commit hook runs the same command unconditionally.**
The full strict run of all 35 specs is ~190ms (dominated by Node startup); scoping to changed files would save ~5ms while adding path→spec-id mapping complexity and a correctness risk. Wire it through the existing `hooks/` mechanism.

**D6. Vendor the `gh-stack` skill.**
`gh skill install` is unavailable here, so the skill (`SKILL.md` + `references/{commands,stack-design,troubleshooting}.md`, MIT-licensed) is copied from `github/gh-stack` into `.github/skills/gh-stack/`. It ships in this PR because it is the prerequisite tooling for the stacked follow-up PR.

## Risks / Trade-offs

- **Reformatting silently alters a requirement's wording.** → Diff each spec's requirement/scenario text before-and-after; only structural headers and the new `## Purpose` should change. Validation + review catch structural mistakes; a careful diff catches semantic ones.
- **Builder image version collision with parallel branches (2.15 webrtc, 2.16 testSpeedup).** → Bump to 2.17; reconcile at merge time if another branch also claims it.
- **CI depends on the new builder image being published.** → CI must reference the bumped image tag; if unpublished, the validate job fails fast with a clear "openspec: not found".
- **Pinned openspec drifts from the latest CLI.** → Acceptable; the pin is the source of truth and is bumped deliberately alongside the image version.

## Migration Plan

1. Vendor the `gh-stack` skill (already staged in the worktree).
2. Rewrite the 34 delta specs (flatten + title + `## Purpose`); reword the one non-normative requirement in `sbmd-runtime-observability`.
3. Add pinned `openspec` to `docker/Dockerfile`; bump `docker/version` to 2.17.
4. Add the CI validation workflow and the pre-commit hook.
5. Run `openspec validate --specs --strict` — must be fully green — then open PR #1. The `consolidate-specs` PR stacks on top via `gh stack`.

Rollback is trivial: the change is documentation/tooling only; reverting the branch restores the prior specs and removes the workflow/hook with no runtime effect.

## Open Questions

- Final pinned `openspec` version — default to `1.4.1` (matches local) unless a newer release is preferred.
- Confirm the exact builder image tag CI should consume once `docker/version` is bumped.
