## Why

The release workflow (`release.yaml`) has several issues discovered during the 4.0.0 release:
the version commit created by `cog bump` is not pushed to `main` (leaving the tag dangling off
the branch lineage), tags are lightweight instead of annotated, the workflow lacks branch guards,
and the repository has no historical CHANGELOG entries prior to 4.0.0.

## What Changes

- **Annotated tags**: After `cocogitto-action` creates a lightweight tag, the workflow will delete
  it and recreate it as an annotated tag before pushing.
- **Push version commit to main**: The workflow will `git push origin HEAD:main` so the changelog
  commit (and tag) are part of the `main` lineage, eliminating the need for follow-up merge PRs.
- **Branch guard**: Add a condition so the workflow can only run against the `main` branch,
  preventing accidental releases from feature branches.
- **Comprehensive CHANGELOG**: Ensure `cog.toml` is configured so that `cog bump` prepends new
  entries to the existing `CHANGELOG.md` rather than overwriting it.
- **Regenerate full CHANGELOG history**: Use `cog changelog` to generate entries for all prior
  semver releases (1.0.0 through 3.1.1) and check the result into the repository.

## Non-goals

- Changing the trigger model — the workflow remains `workflow_dispatch` (manual) by design.
- Upgrading from `cocogitto-action@v3` to `v4` (can be a separate follow-up).
- Generating changelog entries for non-semver tags (e.g., `docker-builder-*`, `release-0.9*`).
- Modifying BartonCore application code, APIs, or build flags.

## Capabilities

### New Capabilities
- `release-workflow`: Covers the annotated tag conversion, branch guard, and pushing the version
  commit to main within the GitHub Actions release workflow.
- `changelog-generation`: Covers cocogitto configuration for comprehensive, prepended changelog
  entries and the one-time historical regeneration of prior release entries.

### Modified Capabilities
<!-- No existing spec-level requirements are changing. -->

## Impact

- `.github/workflows/release.yaml` — workflow changes (branch guard, tag conversion, commit push)
- `cog.toml` — potential configuration adjustments for changelog behavior
- `CHANGELOG.md` — will be regenerated with full history from 1.0.0 through current
