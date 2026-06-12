# Copilot Instructions for BartonCore

## Code Formatting
- Run `clang-format` from the repo root so it uses the repository's `.clang-format` configuration. All C/C++ code must conform.

## Naming Conventions (C/C++)
- **Member variables** (including static) use camelCase with NO leading or trailing underscores (e.g., `myVar`, `instance`, `initialized`, `memBuffer`).
- **Local variables** use camelCase with no underscores.
- **Static functions** and **member functions** use PascalCase (e.g., `Initialize`, `GetSharedContext`).
- **Free functions** use camelCase.
- **C API exception**: Code under `api/c/` follows the glib/GObject convention using `snake_case` for function names, parameters, and variables.

## Blank Line Style (C/C++)
These rules are **not fully enforceable by clang-format** -- `SeparateDefinitionBlocks: Always` in `.clang-format` handles blank lines between top-level definitions, but the within-function rules below must be applied manually.
- Always place a blank line before `if`, `for`, `while`, `switch`, and `return` statements, unless the preceding line is an opening brace or another control flow statement's opening line.
- Always place a blank line after a closing brace `}` unless it is followed by `else`, `catch`, or another closing brace.
