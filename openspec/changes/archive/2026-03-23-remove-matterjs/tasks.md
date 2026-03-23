## 1. CMake & Build System Removal

- [x] 1.1 Remove `BCORE_MATTER_USE_MATTERJS` option definition and its mquickjs compatibility check from `config/cmake/options.cmake`
- [x] 1.2 Delete the CMake module `config/cmake/modules/BCoreMatterJsClusters.cmake`
- [x] 1.3 Remove all matterjs-related blocks from `core/CMakeLists.txt`: the `include(BCoreMatterJsClusters)`, matterjs spec file glob, `bcore_matterjs_build_clusters()` call, `MatterJsClustersEmbedded.h` embedding, matterjs header dependencies, and matterjs-specific install logic
- [x] 1.4 Delete the build script `scripts/build-matterjs-clusters.sh`
- [x] 1.5 Remove default value for `scripts/embed-js-as-header.py` and make the option required

## 2. C++ Source Code Removal

- [x] 2.1 Delete `core/deviceDrivers/matter/sbmd/quickjs/MatterJsClusterLoader.h` and `MatterJsClusterLoader.cpp`
- [x] 2.2 Remove `#include "MatterJsClusterLoader.h"` and `MatterJsClusterLoader::LoadBundle()`/`GetSource()` calls from `core/deviceDrivers/matter/sbmd/quickjs/SbmdScriptImpl.cpp`
- [x] 2.3 Remove `UsesMatterJs()` method from `core/deviceDrivers/matter/sbmd/SbmdSpec.h`
- [x] 2.4 Remove `"JavaScript+matterjs"` from the `scriptType` enum in `core/deviceDrivers/matter/sbmd/sbmd-spec-schema.json`

## 3. SBMD Spec Migration

- [x] 3.1 Rewrite `door-lock.sbmd` to use `scriptType: "JavaScript"` with `SbmdUtils.Tlv` and `SbmdUtils.Response` helpers, and move from `specs/matterjs/` to `specs/`
- [x] 3.2 Rewrite `air-quality-sensor.sbmd` to use `scriptType: "JavaScript"` with `SbmdUtils.Tlv` and `SbmdUtils.Response` helpers, and move from `specs/matterjs/` to `specs/`
- [x] 3.3 Delete the now-empty `core/deviceDrivers/matter/sbmd/specs/matterjs/` directory

## 4. Docker Configuration

- [x] 4.1 Remove Node.js 22.x and npm installation from `docker/Dockerfile.devcontainer` — SKIPPED: Node.js is required for openspec CLI, not just matter.js

## 5. Test Updates

- [x] 5.1 Remove `_is_matterjs_enabled()`, `is_matterjs_enabled()`, and `requires_matterjs` from `testing/conftest.py`
- [x] 5.2 Update `core/test/src/sbmdParserTest.cpp` to remove matterjs spec paths and matterjs-related comments; update door-lock test to reference `specs/door-lock.sbmd` instead of `specs/matterjs/door-lock.sbmd`
- [x] 5.3 Run unit tests to verify all pass without matterjs references

## 6. Documentation Updates

- [x] 6.1 Delete `docs/SBMD_MATTERJS_INTEGRATION.md`
- [x] 6.2 Update `docs/SBMD.md` to remove all matter.js references (scriptType warning, JavaScript+matterjs documentation, link to SBMD_MATTERJS_INTEGRATION.md)

## 7. OpenSpec Specification Updates

- [x] 7.1 Update `openspec/specs/sbmd-system/spec.md`: remove matter.js integration and engine compatibility requirements, update spec catalog to show all specs using JavaScript
- [x] 7.2 Update `openspec/specs/build-system/spec.md`: remove `BCORE_MATTER_USE_MATTERJS` from feature flag catalog table
