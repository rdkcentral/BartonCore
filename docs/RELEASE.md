# Release Process

## Overview

BartonCore uses [Conventional Commits](https://www.conventionalcommits.org/) and
[Cocogitto](https://docs.cocogitto.io/) to automate the release process,
including semantic versioning, changelog generation, and GitHub Release creation.

### Conventional Commits

All commits and PR titles must follow the
[Conventional Commits](https://www.conventionalcommits.org/) specification.
See [CONTRIBUTING.md](../CONTRIBUTING.md) for the full commit message guidelines
and allowed types.

### Cocogitto

[Cocogitto](https://docs.cocogitto.io/) is a CLI tool and set of GitHub Actions
for managing conventional commit workflows. It can validate commit messages,
automatically determine semver bumps, generate changelogs, and create releases.

In this project we use it to:

- **Validate PR titles** — The `check-pr-title` workflow uses `cog verify` to
  enforce conventional commit format on PR titles.
- **Automate releases** — The `release` workflow uses `cog bump --auto` to
  analyze commits since the last tag, determine the next version, update
  `CHANGELOG.md`, and create a version tag.

Configuration lives in `cog.toml` at the repo root.

## Performing a Release

### Prerequisites

- You must have write access to the repository
- There must be unreleased conventional commits on `main` since the last tag

### Pre-flight Check (Optional)

You can optionally verify there are commits to release before triggering:

```bash
cog bump --dry-run --auto
```

This prints the next version without making changes. If it errors, either there
are no commits to release or some commits don't follow the conventional commit
format.

You can also inspect unreleased commits with:

```bash
cog log
```

### Trigger the Release

1. Go to the repository on GitHub
2. Navigate to **Actions** → **Release** workflow
3. Click **Run workflow**
4. Ensure the branch is set to `main`
5. Click the green **Run workflow** button

### What Happens Automatically

See [`.github/workflows/release.yaml`](../.github/workflows/release.yaml) for
the full workflow. At a high level:

1. **Checkout** — Full history is cloned using a deploy key
2. **Version bump** — `cocogitto-action` runs `cog bump --auto`, which:
   - Determines the next version from commit types
   - Updates `CHANGELOG.md`
   - Creates a version bump commit
3. **Tag** — An annotated tag is created for the new version
4. **Push** — The version commit and tag are pushed to `main`
5. **GitHub Release** — A release is created with the changelog for that version as the body

### Troubleshooting

| Problem | Cause | Fix |
|---------|-------|-----|
| Workflow fails at "Semver release" | No conventional commits since last tag | Merge a PR with a conventional commit title first |
| Workflow doesn't run | Branch isn't `main` | The workflow has a guard: `if: github.ref == 'refs/heads/main'` |
| Non-conventional commits on `main` | PR title wasn't validated | The `check-pr-title` workflow should catch this on PRs |
