## Why

Developers working with BartonCore rely on AI coding agents to build, test, debug, and interact with Matter devices. Today, every agent session starts from scratch — the agent must discover build scripts, understand test infrastructure, find debugging workflows, and figure out environment requirements through trial and error. This wastes time and produces inconsistent results. The Agent Skills open standard (agentskills.io) provides a portable, agent-agnostic format for packaging operational knowledge that agents can load on demand.

## What Changes

- Add 7 new Agent Skills (SKILL.md files) to `.github/skills/` following the open standard spec:
  - `build`: Building BartonCore — `build.sh` convenience layer, incremental `cmake --build`, CMake flag awareness, the `build.sh → cmake → build-matter.sh` hierarchy
  - `run-unit-tests`: Running C/C++ unit tests via `ctest` — filtering, ASAN awareness, test framework context (CMocka, Google Test)
  - `run-integration-tests`: Running Python integration tests via `pytest`/`py_test.sh` — prerequisite chain, ASAN preloading, marker-based filtering, matter.js dependencies
  - `matter-virtual-devices`: Working with Matter test devices — pre-built sample apps in Docker (`chip-lighting-app`, `chip-lock-app`, `thermostat-app`, `contact-sensor-app`, `chip-tool`), matter.js virtual devices and sideband API, creating custom devices
  - `validate-sbmd`: Validating SBMD specification files — schema + JS syntax validation, stub generation from TypeScript definitions
  - `format-code`: Code formatting — `clang-format` from repo root, the diff-only rule, manual blank-line rules that clang-format cannot enforce
  - `debug`: Debugging workflows — gdb for the reference app, pdb for Python integration tests, gdb with python3-gdb for native code visibility in Python tests
- Each skill is self-contained (no dependency on human-oriented docs) and follows progressive disclosure (frontmatter for discovery, body for instructions, referenced files for detail)
- All skills include a consistent error-handling pattern: attempt the operation first, and on tool-not-found failures check for `/.dockerenv` to suggest running inside the development container

## Non-goals

- No changes to application code, build system, test infrastructure, or Docker configuration
- No AGENTS.md file (separate effort)
- No composite/workflow skills that chain multiple operations (starting atomic, evaluating later)
- No skills for Docker operations (`dockerw`), git hooks, or simulated OTBR (too niche or trivial)
- No skills for real hardware device testing workflows (not yet fleshed out)
- No changes to existing OpenSpec skills

## Capabilities

### New Capabilities
- `agent-skill-build`: Skill for building BartonCore — covers build.sh, cmake, build-matter.sh hierarchy and CMake flags
- `agent-skill-unit-tests`: Skill for running unit tests — covers ctest, test filtering, ASAN, framework context
- `agent-skill-integration-tests`: Skill for running integration tests — covers pytest, py_test.sh, prerequisites, markers
- `agent-skill-matter-devices`: Skill for Matter virtual devices — covers sample apps, matter.js devices, sideband API, chip-tool
- `agent-skill-validate-sbmd`: Skill for SBMD validation — covers schema validation, JS syntax checking, stub generation
- `agent-skill-format-code`: Skill for code formatting — covers clang-format, diff-only rule, blank-line conventions
- `agent-skill-debug`: Skill for debugging — covers gdb, pdb, gdb+python3-gdb workflows

### Modified Capabilities

_(none — this change adds documentation-only artifacts with no requirement changes to existing capabilities)_

## Impact

- **Files added**: 7 new `SKILL.md` files under `.github/skills/`
- **No code changes**: All additions are markdown instruction files
- **No build impact**: No CMake changes, no new dependencies
- **Agent discovery**: Any Agent Skills-compatible tool (VS Code Copilot, Claude Code, Codex CLI, Gemini CLI, Cursor, etc.) will automatically discover and offer these skills
- **Maintenance**: Skills reference exact commands and paths that may need updating when build scripts, test infrastructure, or Docker images change
