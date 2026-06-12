## 1. Build skill

- [x] 1.1 Create `.github/skills/build/SKILL.md` with frontmatter (`name: build`, description, license, compatibility, metadata) and body covering: `build.sh` → `cmake` → `build-matter.sh` hierarchy, incremental rebuilds via `cmake --build build`, CMake flag reference (`BCORE_ZIGBEE`, `BCORE_MATTER`, `BCORE_THREAD`, etc.), dev profile at `config/cmake/platforms/dev/linux.cmake`, and error recovery pattern
- [x] 1.2 Verify `build` skill: confirm frontmatter `name` matches directory, description is under 1024 chars, body is under 500 lines

## 2. Unit test skill

- [x] 2.1 Create `.github/skills/run-unit-tests/SKILL.md` with frontmatter (`name: run-unit-tests`, description, license, compatibility, metadata) and body covering: `ctest --output-on-failure --test-dir build`, test filtering via `ctest -R <pattern>`, listing tests via `ctest -N`, test frameworks (CMocka for C, Google Test for C++), test source locations (`core/test/`, `libs/device/test/`, `api/c/test/`), and error recovery pattern
- [x] 2.2 Verify `run-unit-tests` skill: confirm frontmatter, line count, and command accuracy

## 3. Integration test skill

- [x] 3.1 Create `.github/skills/run-integration-tests/SKILL.md` with frontmatter (`name: run-integration-tests`, description, license, compatibility, metadata) and body covering: `./testing/py_test.sh testing/` as primary command, `py_test.sh` ASAN preloading, test filtering (file path, `-k`, `-m`), prerequisite chain (build, install, dbus, npm ci), `scripts/ci/run_integration_tests.sh` as all-in-one, pytest config in `pyproject.toml`, and error recovery pattern
- [x] 3.2 Verify `run-integration-tests` skill: confirm frontmatter, line count, and command accuracy

## 4. Matter virtual devices skill

- [x] 4.1 Create `.github/skills/matter-virtual-devices/SKILL.md` with frontmatter (`name: matter-virtual-devices`, description, license, compatibility, metadata) and body covering: pre-built sample apps (`chip-lighting-app`, `chip-lock-app`, `thermostat-app`, `contact-sensor-app`), `chip-tool` usage (pairing, cluster commands, attribute reads), matter.js virtual devices (`MatterLight`, `MatterDoorLock`, sideband API), creating custom virtual devices, and error recovery pattern
- [x] 4.2 Verify `matter-virtual-devices` skill: confirm frontmatter, line count, and command accuracy

## 5. SBMD validation skill

- [x] 5.1 Create `.github/skills/validate-sbmd/SKILL.md` with frontmatter (`name: validate-sbmd`, description, license, compatibility, metadata) and body covering: `validate_sbmd_specs.py` usage, `generate_sbmd_stubs.py` usage, spec file locations (`core/deviceDrivers/matter/sbmd/specs/`), automatic validation during build, and error recovery pattern
- [x] 5.2 Verify `validate-sbmd` skill: confirm frontmatter, line count, and command accuracy

## 6. Format code skill

- [x] 6.1 Create `.github/skills/format-code/SKILL.md` with frontmatter (`name: format-code`, description, license, compatibility, metadata) and body covering: `clang-format -i <file>` from repo root, diff-only rule (only format files you modified), manual blank-line conventions (before control flow, after closing braces), pre-commit hook at `hooks/pre-commit`, and error recovery pattern
- [x] 6.2 Verify `format-code` skill: confirm frontmatter, line count, and command accuracy

## 7. Debug skill

- [x] 7.1 Create `.github/skills/debug/SKILL.md` with frontmatter (`name: debug`, description, license, compatibility, metadata) and body covering three workflows: (1) gdb for reference app (`build/reference/barton-core-reference`, flags `-b`, `-z`, `-t`, `-m`, VS Code launch configs), (2) pdb for integration tests (`breakpoint()`, `pytest -s`), (3) gdb+python3-gdb for native visibility (`gdb python3`, debug info prompts, `run -m pytest`, ASAN `LD_PRELOAD` via `set env`), ASAN considerations, and error recovery pattern
- [x] 7.2 Verify `debug` skill: confirm frontmatter, line count, and command accuracy
