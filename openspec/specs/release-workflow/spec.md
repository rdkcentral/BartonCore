## ADDED Requirements

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
