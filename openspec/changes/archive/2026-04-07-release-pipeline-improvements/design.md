## Context

The release workflow (`.github/workflows/release.yaml`) is triggered manually via `workflow_dispatch`.
It uses `cocogitto-action@v3` with `release: true` which internally runs `cog bump --auto`. This:

1. Calculates the next semver version from conventional commits since the latest tag
2. Appends a changelog section to `CHANGELOG.md`
3. Creates a version commit (`chore(version): X.Y.Z`)
4. Creates a **lightweight** git tag on that commit

The workflow then pushes only the tag via a deploy key and creates a GitHub Release using
`softprops/action-gh-release`. The version commit itself is never pushed to any branch.

```
Current flow:
                                               ┌─ tag pushed (lightweight)
                                               │
main:  ──A──B──C──                             │
                  └── cog bump ──▶ D (version) ─┤
                                               │
                                               └─ commit D is ORPHANED
                                                  (not on any branch)
```

During the 4.0.0 release, this resulted in the tag pointing to a commit not reachable from `main`,
requiring a manual merge PR (#199) to bring it back.

The repository has semver tags from 1.0.0 through 4.0.0 but `CHANGELOG.md` only contains the 4.0.0
entry. Conventional commits have been used since 2.0.0.

Cocogitto configuration (`cog.toml`) uses a custom changelog template at
`config/cocogitto/templates/remote_extended` with GitHub remote integration.

## Goals / Non-Goals

**Goals:**
- Version commit and tag land on `main` in a single workflow run, no follow-up PR needed
- Tags are annotated (carry author, date, message) instead of lightweight
- Workflow cannot accidentally be triggered on non-`main` branches
- `CHANGELOG.md` accumulates entries across releases (prepend, not overwrite)
- Full historical changelog for all semver releases from 1.0.0 through 4.0.0

**Non-Goals:**
- Changing trigger to automatic on merge (stays `workflow_dispatch`)
- Upgrading `cocogitto-action` from v3 to v4
- Modifying the changelog template itself
- Generating entries for non-semver tags (`docker-builder-*`, `release-0.9*`, `zilkerStable*`)
- No GObject API, threading, or application code changes involved

## Decisions

### 1. Branch guard via workflow-level condition

**Decision**: Add an `if` condition to the release job that checks `github.ref == 'refs/heads/main'`.

**Alternatives considered**:
- `branch_whitelist` in `cog.toml` — only guards `cog bump`, not the workflow itself; someone
  could still trigger on the wrong branch and get a confusing failure.
- Branch filter on `workflow_dispatch` — `workflow_dispatch` doesn't support branch filters in the
  `on:` block the way `push:` does; must be done at the job or step level.

**Rationale**: Job-level `if` gives a clear, fast failure with an obvious message. Combining with
`cog.toml`'s `branch_whitelist` adds defense in depth.

### 2. Convert lightweight tag to annotated post-bump

**Decision**: After the `cocogitto-action` step, delete the lightweight tag locally and recreate
it as an annotated tag before pushing.

```yaml
- name: Convert to annotated tag
  run: |
    VERSION="${{ steps.release.outputs.version }}"
    git tag -d "$VERSION"
    git tag -a "$VERSION" -m "chore(version): $VERSION" HEAD
```

**Alternatives considered**:
- Use `post_bump_hooks` in `cog.toml` — works but is fragile (fights the tool's own tagging),
  runs inside `cog bump` where failures stash changes.
- Set `disable_bump_commit` and handle everything manually — too much to maintain.

**Rationale**: The workflow already has a seam between "cog creates" and "we push." Inserting the
conversion there is clean, local, and doesn't affect `cog.toml` for local developer usage.

### 3. Push version commit to `main` alongside tag

**Decision**: Push `HEAD:main` and the tag in the same workflow step.

```yaml
- name: Push version commit and tag
  run: |
    git push origin HEAD:main
    git push origin "${{ steps.release.outputs.version }}"
```

```
Desired flow:
                                               ┌─ tag pushed (annotated)
                                               │
main:  ──A──B──C──D──  ◀── commit pushed ─────┤
                  │                            │
                  └── D = version commit ──────┘
```

**Rationale**: The deploy key already has write access to `main`. Direct push bypasses branch
protection status checks, which is appropriate for a maintainer-initiated release workflow that
already passed CI on the commits being tagged.

### 4. Changelog prepend behavior via cocogitto defaults

**Decision**: Rely on cocogitto's default behavior where `cog bump` prepends new version entries
to the existing `CHANGELOG.md`. Verify the current `cog.toml` has `path = "CHANGELOG.md"` and
the template is configured — both are already in place. No `cog.toml` changes needed for this.

**Rationale**: `cog bump` already prepends rather than overwrites when `CHANGELOG.md` exists. The
current workflow's `cog changelog --at $VERSION > CHANGELOG.md` step (which does overwrite) runs
*after* the bump and is only used for the GitHub Release body — it doesn't affect the committed
file. The committed `CHANGELOG.md` is produced by the bump step itself.

### 5. One-time historical changelog regeneration

**Decision**: Install cocogitto in the dev container, run `cog changelog` to generate the full
history, and commit the result. This is a one-time manual operation done outside the workflow.

**Rationale**: `cog changelog` (without `--at`) generates entries for all tags. The custom
`remote_extended` template will format them consistently with the 4.0.0 entry. Pre-2.0.0 commits
may not be conventional-commits-compliant, so those entries may be sparse — but they'll at least
have the tag boundary markers.

## Risks / Trade-offs

- **[Deploy key bypasses branch protection]** → Acceptable: the workflow is `workflow_dispatch`
  only, requiring maintainer access. The branch guard condition adds a software check.
- **[Tag deletion window]** → Between `git tag -d` and `git tag -a`, the tag briefly doesn't
  exist locally. No risk since nothing is pushed until both operations complete.
- **[Pre-2.0.0 changelog entries may be sparse]** → Acceptable: having stub entries with tag
  boundaries is better than no history. Entries can be manually curated post-generation.
- **[Race condition if main advances during release]** → The `git push origin HEAD:main` will
  fail if `main` has advanced since checkout. This is actually a desirable safety behavior —
  the maintainer can re-run the workflow. No force-push should ever be used.
