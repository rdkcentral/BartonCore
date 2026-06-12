## 1. Release Workflow — Branch Guard

- [x] 1.1 Add `if: github.ref == 'refs/heads/main'` condition to the release job in `.github/workflows/release.yaml`
- [x] 1.2 Add `branch_whitelist = ["main"]` to `cog.toml` for defense in depth

## 2. Release Workflow — Annotated Tags

- [x] 2.1 Add a step after `cocogitto-action` that deletes the lightweight tag and recreates it as annotated: `git tag -d "$VERSION" && git tag -a "$VERSION" -m "chore(version): $VERSION" HEAD`

## 3. Release Workflow — Push Version Commit to Main

- [x] 3.1 Replace the existing `git push origin $VERSION` step with a combined step that pushes both the version commit (`HEAD:main`) and the annotated tag
- [x] 3.2 Remove the separate `cog changelog --at` step that overwrites `CHANGELOG.md` (the bump step's committed changelog is the source of truth; the GitHub Release body can use `steps.release.outputs.stdout` or a `cog changelog --at` piped to a variable instead)

## 4. Changelog — Verify Prepend Behavior

- [x] 4.1 Verify that `cog bump` with the current `cog.toml` prepends new entries to an existing `CHANGELOG.md` rather than overwriting (no config change expected; just confirm behavior)

## 5. Changelog — Regenerate Full History

- [x] 5.1 Install cocogitto in the dev container (`cargo install cocogitto` or use a prebuilt binary)
- [x] 5.2 Run `cog changelog` to generate full changelog from all semver tags (1.0.0 through 4.0.0)
- [x] 5.3 Review the generated output — confirm entries exist for each version and are formatted by the `remote_extended` template
- [x] 5.4 Commit the regenerated `CHANGELOG.md`

## 6. Verification

- [x] 6.1 Review the final `release.yaml` for correctness: branch guard, annotated tag conversion, commit+tag push, GitHub Release body
- [x] 6.2 Verify `cog.toml` changes (branch_whitelist) are valid
