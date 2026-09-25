# Release Process

## Purpose

Specifies the project's release process: changelog generation across releases and the release-workflow safeguards (branch guard against non-main releases, annotated tags, and pushing the version commit to main).

## Requirements

### Requirement: Changelog entries are prepended across releases
The release workflow SHALL produce a `CHANGELOG.md` where each new release entry is prepended
above previous entries, preserving the full release history in the committed file.

#### Scenario: Second release after initial
- **WHEN** a release is performed and `CHANGELOG.md` already contains entries from prior releases
- **THEN** the new version's entry SHALL appear above the existing entries
- **THEN** all prior entries SHALL remain intact

### Requirement: Full historical changelog exists
The repository SHALL contain changelog entries for all semver releases from 1.0.0 through the
current version, generated from conventional commit history.

#### Scenario: Historical entries present
- **WHEN** a user views `CHANGELOG.md`
- **THEN** there SHALL be a section for each semver tag (1.0.0, 1.1.0, 2.0.0, 2.1.0, 2.2.0, 2.3.0, 3.0.0, 3.1.0, 3.1.1, 4.0.0)
- **THEN** entries from 2.0.0 onward SHALL contain conventional commit details (type, scope, summary, hash, author)

#### Scenario: Pre-conventional-commits releases
- **WHEN** a user views changelog entries for versions prior to 2.0.0 (1.0.0, 1.1.0)
- **THEN** those entries SHALL at minimum contain the version header and date

### Requirement: Branch guard prevents non-main releases
The release workflow SHALL refuse to run when triggered on any branch other than `main`.

#### Scenario: Triggered on main
- **WHEN** a maintainer triggers the release workflow via `workflow_dispatch` on the `main` branch
- **THEN** the release job SHALL proceed normally

#### Scenario: Triggered on a feature branch
- **WHEN** a maintainer triggers the release workflow via `workflow_dispatch` on a branch other than `main`
- **THEN** the release job SHALL be skipped with a clear indication that it only runs on `main`

### Requirement: Tags are annotated
The release workflow SHALL produce annotated git tags (not lightweight) for each release.

#### Scenario: Tag created during release
- **WHEN** the release workflow completes the bump step
- **THEN** the resulting tag SHALL be an annotated tag with message `chore(version): <version>`
- **THEN** `git cat-file -t <version>` SHALL return `tag` (not `commit`)

### Requirement: Version commit pushed to main
The release workflow SHALL push the version commit to the `main` branch so the tag
is reachable from the branch lineage.

#### Scenario: Successful release
- **WHEN** the release workflow completes successfully
- **THEN** the version commit (containing the changelog update) SHALL be the tip of `main`
- **THEN** the annotated tag SHALL point to that commit

#### Scenario: Main has advanced during release
- **WHEN** the release workflow attempts to push the version commit but `main` has moved forward since checkout
- **THEN** the push SHALL fail (non-fast-forward)
- **THEN** the workflow SHALL NOT force-push
