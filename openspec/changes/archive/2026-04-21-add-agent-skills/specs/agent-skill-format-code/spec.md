## ADDED Requirements

### Requirement: Format code skill SKILL.md conforms to Agent Skills spec
The `format-code` skill SHALL be located at `.github/skills/format-code/SKILL.md`. The frontmatter SHALL include `name: format-code`, a `description` field explaining the skill covers C/C++ code formatting, and `compatibility` noting the BartonCore Docker development container.

#### Scenario: Valid frontmatter
- **WHEN** the SKILL.md file is parsed
- **THEN** the YAML frontmatter SHALL contain `name: format-code` matching the directory name
- **AND** the `description` SHALL mention clang-format, formatting, and C/C++

### Requirement: Format code skill documents clang-format usage
The skill SHALL instruct the agent to run `clang-format` from the repository root so it picks up the `.clang-format` configuration. The skill SHALL show the command to format a file: `clang-format -i <file>`.

#### Scenario: Agent formats a file
- **WHEN** the agent needs to format a C/C++ file
- **THEN** the skill SHALL instruct the agent to run `clang-format -i <file>` from the repo root

### Requirement: Format code skill documents the diff-only rule
The skill SHALL state that agents MUST only format code that is part of the current diff. Agents SHALL NOT reformat entire files they did not modify. The pre-commit hook enforces formatting on staged files automatically.

#### Scenario: Agent respects diff-only rule
- **WHEN** the agent has modified a C/C++ file
- **THEN** the agent SHALL only format the file it modified
- **AND** the agent SHALL NOT run clang-format on unmodified files

### Requirement: Format code skill documents manual blank-line rules
The skill SHALL document the blank-line conventions that clang-format cannot enforce: a blank line before `if`, `for`, `while`, `switch`, and `return` statements (unless preceded by an opening brace or another control-flow statement's opening line), and a blank line after a closing brace `}` (unless followed by `else`, `catch`, or another closing brace).

#### Scenario: Agent applies blank-line rules
- **WHEN** the agent writes or modifies C/C++ code
- **THEN** the agent SHALL apply the manual blank-line conventions

### Requirement: Format code skill documents pre-commit hook
The skill SHALL note that a pre-commit hook (`hooks/pre-commit`) automatically runs clang-format on staged files at commit time. The skill SHALL note that hooks are installed via `./hooks/install.sh`.

#### Scenario: Agent understands pre-commit integration
- **WHEN** the agent reads the format code skill
- **THEN** it SHALL know that formatting is enforced automatically at commit time

### Requirement: Format code skill includes error recovery pattern
The skill SHALL instruct the agent that if `clang-format` is not found, the agent SHALL check for `/.dockerenv`. If absent, the agent SHALL inform the user they need to run inside the development container.

#### Scenario: clang-format not found outside Docker
- **WHEN** `clang-format` is not found
- **AND** `/.dockerenv` does not exist
- **THEN** the agent SHALL stop and tell the user to run inside the development container
