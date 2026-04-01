## Why

Matter.js was integrated as an optional build-time dependency (`BCORE_MATTER_USE_MATTERJS`) to provide type-safe TLV encoding/decoding via the `MatterClusters` global object in SBMD scripts. This added significant complexity: a Node.js/npm build toolchain, a cloned matter.js repository, a JavaScript bundling pipeline, an embedded C header generation step, and ~1 MB of runtime RAM overhead per QuickJS context. The built-in `SbmdUtils.Tlv` helpers now cover the same encoding needs (including structs, nullable types, and enums), making the matter.js integration unnecessary. Removing it simplifies the build, reduces runtime footprint, and eliminates the Node.js dependency from the Docker development container.

## What Changes

- **BREAKING**: Remove the `BCORE_MATTER_USE_MATTERJS` CMake option and `BARTON_CONFIG_MATTER_USE_MATTERJS` compile definition
- **BREAKING**: Remove `scriptType: "JavaScript+matterjs"` support from the SBMD schema and runtime
- Remove the `MatterJsClusterLoader` C++ class and its integration in `SbmdScriptImpl`
- Remove the `BCoreMatterJsClusters.cmake` CMake module and all matterjs-conditional build logic in `core/CMakeLists.txt`
- Remove the `scripts/build-matterjs-clusters.sh` build script
- Delete the `docs/SBMD_MATTERJS_INTEGRATION.md` documentation file
- Migrate `core/deviceDrivers/matter/sbmd/specs/matterjs/door-lock.sbmd` and `air-quality-sensor.sbmd` to `core/deviceDrivers/matter/sbmd/specs/`, rewriting their scripts to use `SbmdUtils.Tlv` and `SbmdUtils.Response` instead of `MatterClusters`
- Remove the `matterjs/` specs subdirectory
- Remove `UsesMatterJs()` from `SbmdSpec.h`
- Remove `_is_matterjs_enabled`, `is_matterjs_enabled`, and `requires_matterjs` test helpers from `testing/conftest.py`
- Update `docs/SBMD.md` to remove matter.js references
- Update `openspec/specs/sbmd-system/spec.md` and `openspec/specs/build-system/spec.md` to remove matter.js requirements
- Update `config/cmake/options.cmake` to remove the option and its validation check
- Update unit tests referencing matterjs spec paths or features

## Non-goals

- Rewriting the `SbmdUtils.Tlv` or `SbmdUtils.Response` helper APIs — they are already sufficient
- Adding new device type specs beyond migrating the existing two
- Changing the QuickJS engine integration or the SBMD spec schema beyond removing the matterjs variant
- Modifying the Matter SDK integration or Matter subsystem

## Capabilities

### New Capabilities

_(none — this is a removal/simplification change)_

### Modified Capabilities

- `sbmd-system`: Remove the `JavaScript+matterjs` scriptType requirement, `MatterJsClusterLoader` loading behavior, and matterjs-specific spec handling. The two existing matterjs device specs (door-lock, air-quality-sensor) become standard `JavaScript` specs.
- `build-system`: Remove the `BCORE_MATTER_USE_MATTERJS` CMake option, the `BCoreMatterJsClusters` module, and npm/Node.js build-time dependency.

## Impact

- **Build system** (`config/cmake/`, `core/CMakeLists.txt`): CMake option removal, module deletion, conditional block cleanup
- **Core runtime** (`core/deviceDrivers/matter/sbmd/quickjs/`): `MatterJsClusterLoader` class deletion, `SbmdScriptImpl` simplification
- **SBMD schema** (`sbmd-spec-schema.json`): `scriptType` enum reduced to `["JavaScript"]`
- **SBMD specs** (`core/deviceDrivers/matter/sbmd/specs/`): Two specs migrated, subdirectory removed
- **Documentation** (`docs/`): One file deleted, one file updated
- **Testing** (`testing/conftest.py`, `core/test/`): Test helpers and matterjs-specific test cases removed/updated
- **OpenSpec specs** (`openspec/specs/`): Two existing specs updated to reflect removal
- **Scripts** (`scripts/`): `build-matterjs-clusters.sh` deleted
- **CMake feature flags**: `BCORE_MATTER_USE_MATTERJS` / `BARTON_CONFIG_MATTER_USE_MATTERJS` fully removed
