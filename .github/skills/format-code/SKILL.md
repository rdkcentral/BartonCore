---
name: format-code
description: Format C/C++ code in BartonCore. Use when the user wants to format source files, understand formatting conventions, or fix formatting issues. Covers clang-format usage, the diff-only rule, manual blank-line conventions that clang-format cannot enforce, and the pre-commit hook.
license: Apache-2.0
compatibility: Requires clang-format (pre-installed in the BartonCore Docker development container).
metadata:
  author: rdkcentral
  version: "1.0"
---

# Format Code

## Running clang-format

Always run from the repository root so it picks up the `.clang-format` configuration:

```bash
clang-format -i <file>
```

Example:

```bash
clang-format -i core/src/deviceService.c
```

To check formatting without modifying (useful for verification):

```bash
clang-format --dry-run --Werror <file>
```

## The Diff-Only Rule

**Only format files you have modified.** Do not reformat entire files or directories that are not part of your current change. The pre-commit hook enforces formatting on staged files automatically — there is no need to bulk-format.

## Manual Blank-Line Conventions

These rules are NOT enforced by clang-format and must be applied manually:

### Blank line BEFORE control flow statements

Always place a blank line before `if`, `for`, `while`, `switch`, and `return` statements, **unless** the preceding line is:
- An opening brace `{`
- Another control flow statement's opening line

```c
int result = compute();

if (result < 0)
{
    handleError();
}

return result;
```

NOT:

```c
int result = compute();
if (result < 0)  // missing blank line before if
{
    handleError();
}
return result;  // missing blank line before return
```

### Blank line AFTER closing braces

Always place a blank line after a closing brace `}`, **unless** it is followed by:
- `else`
- `catch`
- Another closing brace `}`

```c
if (condition)
{
    doSomething();
}
else
{
    doOther();
}

nextStatement();
```

## Pre-Commit Hook

A pre-commit hook at `hooks/pre-commit` automatically runs clang-format on staged files at commit time. Install hooks with:

```bash
./hooks/install.sh
```

The hook calls `hooks/clang-format-hooks/git-pre-commit-format` which formats only the staged diff.

## Key clang-format Settings

The `.clang-format` configuration uses:
- Allman brace style (braces on their own line)
- 4-space indentation
- 120-column line width
- `SeparateDefinitionBlocks: Always` (blank lines between top-level definitions)

## C API Exception

Code under `api/c/` follows glib/GObject conventions using `snake_case` for function names, parameters, and variables instead of the camelCase/PascalCase used elsewhere.

## Error Recovery

If `clang-format` is not found:

1. Check if `/.dockerenv` exists
2. If it does NOT exist, stop and tell the user: **"clang-format is not available. Please run inside the BartonCore development container."**
3. If it does exist, the tool may need to be installed — check with `apt list --installed 2>/dev/null | grep clang-format`
