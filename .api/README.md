# ABI baseline

`libBartonCore.abi` is the [libabigail](https://sourceware.org/libabigail/) `abidw` XML dump of `libBartonCore.so` built from the most recent release tag. `scripts/quality/abi.sh` compares the current build against it with `abidiff` and reports `BLOCKED: no baseline` while the file is absent.

## Regenerating the baseline

Run from a clean checkout of the release tag, inside the builder container:

```sh
git worktree add /tmp/abi-baseline <release-tag>
cd /tmp/abi-baseline
./dockerw cmake --preset dev
./dockerw cmake --build build/dev --target BartonCore
./dockerw abidw --headers-dir api/c/public --out-file .api/libBartonCore.abi build/dev/core/libBartonCore.so
```

Copy the resulting `.api/libBartonCore.abi` into this directory on the branch being reviewed and commit it. Regenerate it after every release.
