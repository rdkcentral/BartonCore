# Contributing

If you would like to contribute code to this project, you can do so through GitHub by forking the repository
and sending a pull request.
Before your code is accepted into the project, you must sign the RDK Contributor License Agreement (CLA).

-   [Commit Message Guidelines](#commit-message-guidelines)
    -   [Commit Message Format](#commit-message-format)

## Commit Message Guidelines

> This specification is heavily inspired by the [Angular commit message format](https://github.com/angular/angular/blob/main/CONTRIBUTING.md#-commit-message-format).

The Barton project mandates strict guidelines for formatting git commit messages,
following the [Conventional Commits Specification](https://www.conventionalcommits.org/en/v1.0.0/).
Observing these guidelines results in clear and easily navigable commit histories.

### Commit Message Format

Each commit message consists of a header, a body and a footer that follows the following structure.
**NOTE**: the blank lines between each section are purposeful and mandatory for every commit.

```
# Header
<type>(<scope>): <subject>

[optional body]

[optional footer(s)]
```

#### Header Overview

The header is mandatory and is composed of three sections:

1. `type` - required
2. `scope` - optional
3. `subject` - required

The total length should try to be 50 characters or less.

##### Header: Type

Must be one of the following.

-   `build:` Modifications impacting the build system or external dependencies
-   `chore:` Other changes that don't modify `src` or `test` files
-   `ci:` Changes to our CI configuration files and scripts
-   `docs:` Documentation only changes
-   `feat:` A new feature
-   `fix:` A bug fix
-   `refactor:` A code change that neither fixes a bug nor adds a feature
-   `revert:` A commit that reverts a previous commit
-   `style:` Changes that do not affect the meaning of the code (white-space, formatting, missing semi-colons, etc)
-   `test:` Adding missing tests or correcting existing tests

##### Header: Scope

Scopes are optional and can range from device drivers and subsystems to documentation files,
or they can be omitted entirely. The committer should use their best judgment to determine
whether to include a scope, and what the scope should be.

**NOTE:** You can append a `!` after the type or scope (if applicable) to convey a `BREAKING CHANGE`.

##### Header: Subject

The subject is mandatory and contains a succinct description of the change.

#### Body

The body is optional, but should only be left out if the reasoning behind the change can be
easily derived from the header and does not require additional context and/or explanation.

If the body is included, the content of the body should explain the reason for the change.

Each line of the body should not exceed 72 characters.

#### Footer

The footer is optional and may include details about breaking changes and deprecations.
It is also the appropriate section to reference GitHub issues, Jira tickets, and other PRs
that this commit closes or relates to. References should be identified with the text `Refs:`.

The Breaking Change section should begin with `BREAKING CHANGE:` followed by a summary of
the change, a blank line, and a detailed description that includes migration instructions.

#### Reverting Commits

When a commit reverts another commit, it should begin with `revert:` followed by the header
of the commit being undone.

The body of the commit message should contain:

-   Details about the SHA of the commit being undone in the format: `This reverts commit <SHA>`
-   A clear explanation of why the reversion is necessary.

#### Examples

-   Commit message with no body:

```
docs(readme): add table of contents
```

-   Commit message with no scope

```
build: add clang-format to Dockerfile

Ensure clang-format is available in the development environment by
installing during image build.
```

-   A simple refactor

```
refactor(ui): rename updateInteractiveElements to updateUIComponents

Enhance code clarity. The original name no longer accurately reflects
the function's purpose.
```

-   Commit message with scope, multi-paragraph body and footer

```
feat(matter): add basic Matter device driver core

Include very basic versions of lights, door locks, and window coverings.

Also add the capability to commission these devices from device utility
via the 'commissionDevice' (or 'cd') command.

Refs: ad98f3a2b8
```

-   Commit message with scope, and both `!` and `BREAKING CHANGE` footer

```
feat(api)!: update add function to accept an optional third parameter

BREAKING CHANGE: The add function now accepts an optional third
parameter `c`.

The function signature has changed from `add(a, b)` to `add(a, b, c=0)`.
This change allows for more flexible addition operations but requires
updates to existing code that calls the `add` function.

Migration instructions:
- Update all calls to `add(a, b)` to include the third parameter if
  needed, or ensure the default value is acceptable.
- Example:
  # Old usage
  result = add(1, 2)

  # New usage
  result = add(1, 2, 3)

Refs: JIRA-1234
```

-   [More Examples](https://www.conventionalcommits.org/en/v1.0.0/#examples)
