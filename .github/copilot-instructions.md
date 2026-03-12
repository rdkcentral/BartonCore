# Copilot Instructions for BartonCore

## Code Formatting
- Run `.clang-format` rules from the repo root. All C/C++ code must conform.

## Naming Conventions (C/C++)
- **Member variables** (including static) use camelCase with NO leading or trailing underscores (e.g., `myVar`, `instance`, `initialized`, `memBuffer`).
- **Local variables** use camelCase with no underscores.
- **Static functions** and **member functions** use PascalCase (e.g., `Initialize`, `GetSharedContext`).
- **Free functions** use camelCase.

## Blank Line Style (C/C++)
- Always place a blank line before `if`, `for`, `while`, `switch`, and `return` statements, unless the preceding line is an opening brace or another control flow statement's opening line.
- Always place a blank line after a closing brace `}` unless it is followed by `else`, `catch`, or another closing brace.
