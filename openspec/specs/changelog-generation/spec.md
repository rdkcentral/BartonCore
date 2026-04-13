## ADDED Requirements

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
