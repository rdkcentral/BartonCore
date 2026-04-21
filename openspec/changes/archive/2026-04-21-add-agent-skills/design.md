## Context

BartonCore has comprehensive developer tooling — build scripts, test frameworks, Docker environment, debugging configurations, Matter device infrastructure — but all operational knowledge lives in human-oriented docs (`docs/DEBUGGING.md`, `docs/USING_BARTON_GUIDE.md`, etc.) and scattered across scripts, Dockerfiles, and VS Code configs. AI coding agents cannot efficiently discover or use this knowledge.

The Agent Skills open standard (agentskills.io) defines a portable format for packaging agent instructions. Skills are directories with a `SKILL.md` file containing YAML frontmatter (name, description) and markdown instructions. Agents discover skills via standard locations (`.github/skills/`, `.claude/skills/`, `.agents/skills/`) and load them progressively — frontmatter at startup, body on activation, referenced files on demand.

BartonCore already uses `.github/skills/` for OpenSpec workflow skills. The new operational skills will live alongside them.

```
.github/skills/
├── openspec-apply-change/      (existing - workflow)
├── openspec-archive-change/    (existing - workflow)
├── openspec-explore/           (existing - workflow)
├── openspec-propose/           (existing - workflow)
├── build/                      (new - operational)
├── run-unit-tests/             (new - operational)
├── run-integration-tests/      (new - operational)
├── matter-virtual-devices/     (new - operational)
├── validate-sbmd/              (new - operational)
├── format-code/                (new - operational)
└── debug/                      (new - operational)
```

## Goals / Non-Goals

**Goals:**
- Give agents precise, self-contained operational knowledge for 7 core development tasks
- Follow the Agent Skills open standard exactly (agentskills.io/specification)
- Skills work across any conformant agent (VS Code Copilot, Claude Code, Codex CLI, Gemini CLI, Cursor, etc.)
- Each skill is atomic — composability comes from agents chaining skills, not from composite skill files
- Skills use a consistent error-handling pattern: try first, check for Docker on tool-not-found failures

**Non-Goals:**
- No application code, build system, or infrastructure changes
- No AGENTS.md (separate effort)
- No composite/workflow skills
- No skills for trivial or niche operations (Docker, git hooks, simulated OTBR)
- No changes to existing OpenSpec skills
- No thread safety, GObject API, or backward compatibility considerations (this change adds only markdown files)

## Decisions

### 1. All skills as flat SKILL.md (no `scripts/`, `references/`, `assets/` subdirectories)

Each skill is a single `SKILL.md` file. The operations these skills describe are already implemented in existing scripts (`build.sh`, `py_test.sh`, etc.) — the skills just teach agents how to invoke them.

**Alternative considered**: Including helper scripts (e.g., a Docker-detection script). Rejected because the detection logic is a 1-line `/.dockerenv` check that's simpler inline than as a referenced script.

### 2. Try-then-diagnose error handling pattern (not upfront gating)

Skills instruct agents to attempt the operation directly. If a tool-not-found error occurs (e.g., `cmake: command not found`), the agent should then check for `/.dockerenv` and, if missing, tell the user to run inside the development container with a fatal bail-out.

**Alternative considered**: Upfront Docker detection (check `/.dockerenv` before running anything). Rejected because it makes skills overly cautious and prevents use in non-Docker environments that may have the tools installed.

### 3. Single debug skill with three workflow sections

One `debug` skill covers gdb (reference app), pdb (Python tests), and gdb+python3-gdb (native visibility in Python tests). The agent reads the whole skill and selects the appropriate workflow.

**Alternative considered**: Three separate debug skills. Rejected because the debugging intent is usually "debug X" and the agent benefits from seeing all options to pick the right approach. Three skills would also force the agent to pick before reading.

### 4. Self-contained skills (no cross-references to human docs)

Skills contain all the information an agent needs without referencing `docs/DEBUGGING.md` or other human documentation. This avoids extra file reads and keeps skills independent of doc structure changes.

**Alternative considered**: Skills that `[reference](../../docs/DEBUGGING.md)` human docs. Rejected because it creates a dependency chain, increases token usage (agent loads both), and human docs contain narrative content not optimized for agent consumption.

### 5. Skills conform to Agent Skills spec constraints

- `name` field: lowercase + hyphens, matches directory name, max 64 chars
- `description` field: max 1024 chars, describes what + when
- `SKILL.md` body: under 500 lines (progressive disclosure)
- Frontmatter: uses `license`, `compatibility`, and `metadata` optional fields for BartonCore context

## Risks / Trade-offs

**[Staleness]** → Skills reference exact commands, paths, and binary names that can drift as infrastructure evolves. Mitigation: keep skills focused on stable interfaces (`build.sh`, `ctest`, `pytest`) rather than implementation details. Review skills when build or test infrastructure changes.

**[Token budget]** → Seven skills add ~700 tokens of frontmatter to agent startup context (7 × ~100 tokens). Mitigation: this is well within acceptable limits and the progressive disclosure model ensures body content only loads when relevant.

**[Incomplete coverage]** → Starting with 7 atomic skills may not cover all common composite workflows (e.g., "build, start a light device, commission it via the reference app"). Mitigation: evaluate after initial use whether composite recipe skills are needed.

**[Docker assumption]** → Skills are written primarily for the Docker development environment. Users running natively may see different behavior. Mitigation: the try-then-diagnose pattern handles this gracefully — native environments that have the tools will work fine.
