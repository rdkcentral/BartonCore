# Debugging Guide
- [Debugging Guide](#debugging-guide)
  - [Introduction](#introduction)
  - [Barton Reference App](#barton-reference-app)
    - [VSCode](#vscode)
    - [CLI](#cli)
  - [CMake Tests](#cmake-tests)
    - [VSCode](#vscode-1)
  - [Integration Tests](#integration-tests)

## Introduction
This guide provides an introduction on how to debug the barton reference app, unit tests, and any integration tests in the BartonCore project. This guide will show what you can do in VSCode as well as via command-line. It assumes you are working in the docker environment provided; any other environment is not supported for development.

## Barton Reference App
The Barton Reference App provides a CLI for interactively interfacing with Barton Device Service.

### VSCode
One can debug the Barton Reference App with the `(gdb) Reference App` debug configuration. A terminal will automatically open ready to accept input from the user.

### CLI
Refer to the `(gdb) Reference App` debug configuration in launch.json to determine what command to run in your terminal.

## CMake Tests
Barton unit tests are all known to CMake.

### VSCode
There are two ways to run CMake test targets.

The first way is to:
1. Navigate to the CMake panel.
2. Ensure configuration is done in IDE (as opposed to via CLI) by pressing `Delete Cache and Reconfigue`. This is only necessary if there is a cache divergence from running CMake in a terminal (unlikely).
3. Select either the test target or `all` in the `Build` section.
4. Select the desired test in the `Debug` section.
5. Press the debug button in the bottom ribbon (this will build your `Build` target first if necessary).

The second way is to:
1. Complete steps 1-3 of the previous method.
2. Navigate to the `Testing` panel.
3. Under `BartonCore`, hover over the test you want to debug and press the Debug button.

## Integration Tests
Our Python integration tests have some divergence in debuggability.

As a prerequisite, ensure you have run the CMake install task. In VSCode, this can be done in the Command Pallette. This will install the barton shared library, generated gir/typelib files, and generated python stubs (.pyi file) for development code-completion.

To debug the python side of programs running Barton, you can use pdb for this. In VSCode, this can be accomplished via:
1. Navigating to the python test/file in question.
2. Locating the Play button in the top right actions ribbon.
3. Click the carrot button and select `Python Debugger: Debug using launch.json`
4. Select `Python Debugger: Current File` debug configuration.

Again, this will not allow you do place breakpoints or step through Barton api/core code; only the python code.

To debug Barton api/core code in a python program:
1. From a terminal, run `gdb python3`
2. Answer yes to any prompts about downloading debug information.
3. Set any breakpoints in C code. This includes Barton api/core, or any libraries it depends on. These will not be loaded yet so gdb will not autocomplete these. You can, instead, run the program (step 4), set breakpoints with auto-complete after running the program, then re-run the program to hit those breakpoints.
4. Run your program via `run <path-to-python-file>`.
