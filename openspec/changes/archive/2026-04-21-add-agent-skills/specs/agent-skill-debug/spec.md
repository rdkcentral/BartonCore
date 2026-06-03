## ADDED Requirements

### Requirement: Debug skill SKILL.md conforms to Agent Skills spec
The `debug` skill SHALL be located at `.github/skills/debug/SKILL.md`. The frontmatter SHALL include `name: debug`, a `description` field explaining the skill covers debugging BartonCore with gdb and pdb, and `compatibility` noting the BartonCore Docker development container.

#### Scenario: Valid frontmatter
- **WHEN** the SKILL.md file is parsed
- **THEN** the YAML frontmatter SHALL contain `name: debug` matching the directory name
- **AND** the `description` SHALL mention debugging, gdb, pdb, reference app, and integration tests

### Requirement: Debug skill documents gdb for the reference app
The skill SHALL explain how to debug the reference app with gdb. The skill SHALL document the binary location (`build/reference/barton-core-reference`), common command-line flags (`-b` for SBMD specs path, `-z` to disable Zigbee, `-t` to disable Thread, `-m` to disable Matter), and how to start a gdb session. The skill SHALL note that the VS Code launch configurations (`(gdb) Reference App` variants) provide an alternative.

#### Scenario: Agent debugs the reference app with gdb
- **WHEN** the agent needs to debug the reference app
- **THEN** the skill SHALL show how to launch `gdb build/reference/barton-core-reference` and pass arguments
- **AND** the skill SHALL mention available VS Code launch configs as an alternative

### Requirement: Debug skill documents pdb for integration tests
The skill SHALL explain how to debug Python integration tests using pdb. The skill SHALL show how to insert breakpoints (`breakpoint()` or `import pdb; pdb.set_trace()`) and how to run pytest with debugger support (`./testing/py_test.sh testing/test/<test_file>.py -s --no-header`). The skill SHALL note that the VS Code Python debugger can also be used via the Testing panel.

#### Scenario: Agent debugs an integration test with pdb
- **WHEN** the agent needs to debug a Python integration test
- **THEN** the skill SHALL show how to insert a breakpoint and run the test with `-s` flag
- **AND** the skill SHALL mention VS Code Testing panel as an alternative

### Requirement: Debug skill documents gdb with python3-gdb for native visibility
The skill SHALL explain how to use gdb with python3 to debug C/C++ code exercised by Python integration tests. The skill SHALL document the workflow: run `gdb python3`, answer yes to debug info download prompts, set C breakpoints (may require running first for symbol loading), then execute the test via `run -m pytest testing/test/<test_file>.py`. The skill SHALL note that if the build uses ASAN, the agent must run `set env LD_PRELOAD=<path-to-libasan.so>` in gdb before `run`. The skill SHALL note the command to find libasan: `gcc -print-file-name=libasan.so`.

#### Scenario: Agent debugs native code via Python tests
- **WHEN** the agent needs to debug C/C++ code triggered by a Python integration test
- **THEN** the skill SHALL show the gdb+python3 workflow step by step
- **AND** the skill SHALL include the ASAN LD_PRELOAD setup if needed

### Requirement: Debug skill documents ASAN considerations
The skill SHALL explain that the development build enables Address Sanitizer (ASAN) by default. The skill SHALL document that `testing/py_test.sh` handles ASAN preloading automatically for pytest runs, and that for gdb sessions the agent must manually preload libasan using `set env LD_PRELOAD` before running. The skill SHALL note that `ASAN_OPTIONS=verify_asan_link_order=0` is set in the container environment.

#### Scenario: Agent understands ASAN in debugging context
- **WHEN** the agent reads the debug skill
- **THEN** it SHALL understand that ASAN is enabled by default in dev builds
- **AND** it SHALL know how to handle ASAN when debugging

### Requirement: Debug skill includes error recovery pattern
The skill SHALL instruct the agent that if `gdb` is not found, the agent SHALL check for `/.dockerenv`. If absent, the agent SHALL inform the user they need to run inside the development container. The skill SHALL note that the Docker container has `SYS_PTRACE` capability enabled for gdb to work.

#### Scenario: gdb not found outside Docker
- **WHEN** `gdb` is not found
- **AND** `/.dockerenv` does not exist
- **THEN** the agent SHALL stop and tell the user to run inside the development container
