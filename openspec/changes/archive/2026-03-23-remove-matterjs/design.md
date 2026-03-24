## Context

BartonCore optionally integrates matter.js — a Node.js-based Matter protocol library — to provide type-safe TLV encoding/decoding via the `MatterClusters` global object in SBMD QuickJS scripts. This is gated behind `BCORE_MATTER_USE_MATTERJS=OFF` (default OFF) and involves:

1. A build pipeline: `scripts/build-matterjs-clusters.sh` clones the matter.js git repo, runs `npm install`, builds via TypeScript/esbuild, and produces a JS bundle
2. A CMake module (`BCoreMatterJsClusters.cmake`) that orchestrates the above and embeds the bundle as a C header (`MatterJsClustersEmbedded.h`)
3. A C++ loader class (`MatterJsClusterLoader`) that injects the bundle into QuickJS contexts
4. A `scriptType: "JavaScript+matterjs"` variant in the SBMD schema
5. Two device specs using this variant: `door-lock.sbmd` and `air-quality-sensor.sbmd`
6. Docker Node.js installation for the build pipeline
7. Documentation, test helpers, and openspec requirements

The built-in `SbmdUtils.Tlv` and `SbmdUtils.Response` helpers now handle all the same encoding patterns (primitives, structs, nullable types, enums) without the ~1 MB RAM overhead of loading matter.js into QuickJS. The matter.js integration is unused in production and adds build complexity.

## Goals / Non-Goals

**Goals:**
- Completely remove all matter.js code, build infrastructure, and configuration from the codebase
- Migrate the two matterjs SBMD specs (door-lock, air-quality-sensor) to use standard `SbmdUtils.Tlv` JavaScript
- Update all documentation and openspec specifications to reflect the removal
- Ensure the build compiles and tests pass without any matterjs references

**Non-Goals:**
- Modifying the `SbmdUtils` built-in library API
- Adding new SBMD device specs beyond migrating the existing two
- Changing the QuickJS or mquickjs engine integration
- Modifying the Matter SDK or Matter subsystem
- Changing any non-matterjs SBMD specs

## Decisions

### 1. Migrate specs in-place rather than rewriting from scratch

**Decision**: Rewrite the JavaScript in `door-lock.sbmd` and `air-quality-sensor.sbmd` to use `SbmdUtils.Tlv.*` and `SbmdUtils.Response.*`, then move them from `specs/matterjs/` to `specs/`.

**Rationale**: The YAML structure (bartonMeta, matterMeta, endpoints, resources) is identical — only `scriptType` and the JavaScript code within `script:` blocks differs. The existing specs are well-tested and correct in their Matter protocol mapping, so preserving the YAML and rewriting only the JS minimizes risk.

**Alternative considered**: Deleting the matterjs specs entirely. Rejected because door-lock and air-quality-sensor are valuable device types that should continue to be supported.

### 2. Delete files rather than deprecate

**Decision**: Completely delete `MatterJsClusterLoader.h/.cpp`, `BCoreMatterJsClusters.cmake`, `scripts/build-matterjs-clusters.sh`, `docs/SBMD_MATTERJS_INTEGRATION.md`, and the `specs/matterjs/` directory.

**Rationale**: These files are only meaningful when `BCORE_MATTER_USE_MATTERJS=ON`. Since we're removing the option entirely, keeping dead code adds confusion. Git history preserves everything if it's ever needed.

### 3. Remove `JavaScript+matterjs` from the schema enum

**Decision**: Reduce the `scriptType` enum in `sbmd-spec-schema.json` to `["JavaScript"]` only.

**Rationale**: With no specs using `JavaScript+matterjs` and no loader to support it, keeping the enum value would be misleading. If a future integration needs a new script type, it can add one.

### 4. Clean up `embed-js-as-header.py` default variable name

**Decision**: Leave `scripts/embed-js-as-header.py` mostly as-is — it is a general-purpose tool.  The variable name should no longer have a default and should instead be required. It is still used to embed SbmdUtils.

**Rationale**: The script is invoked with explicit arguments by CMake.  A default variable name does not make sense due to the impact to scripts that use it.

## Risks / Trade-offs

- **[Risk] Door-lock spec correctness after rewrite** → The door-lock spec has complex TLV encoding (LockOperation event decoding, conditional PIN encoding in lock/unlock commands with TlvObject/TlvField). The rewrite must carefully map these to `SbmdUtils.Tlv.encodeStruct`/`decodeStruct` with explicit tag/type schemas. **Mitigation**: Preserve the exact same Matter cluster IDs, attribute IDs, command IDs, and data types. Validate with existing unit tests.

- **[Risk] Breaking downstream builds that set `BCORE_MATTER_USE_MATTERJS=ON`** → Any CI or developer workflow explicitly enabling this flag will get a CMake error. **Mitigation**: The flag defaults to OFF and is not widely used. The error will be immediate and clear.

## Migration Plan

The removal is a single-step change with no phased rollout needed:
1. Delete files and remove CMake option/module
2. Update `scripts/embed-js-as-header.py` to remove the default value for variable name and make it required.
3. Rewrite and move the two SBMD specs
4. Update source code, tests, docs, and openspec specs
5. Build and run tests to confirm nothing references matterjs

**Rollback**: Revert the commit. All deleted files are recoverable from git history.
