# Documentation

## Building and Developing Barton

- Documentation about standard build & development flows using
  [Visual Studio Code](https://code.visualstudio.com/) can be found in
  [the vscode development guide](./VSCODE_DEVELOPMENT.md)
- A guide to debugging Barton can be found in [the debugging guide](./DEBUGGING.md)

## Using Barton in Your Project Guide

- A guide to integrating Barton to new projects can be found in [the using Barton guide](./USING_BARTON_GUIDE.md)

## Style Guide

- Documentation about coding style is documented in
  [the style guide](./STYLE_GUIDE.md)

## Third Party Tools

### Docker

Some tools and utilities are dependent on third party tools, such as Docker.

[Docker](https://www.docker.com) is an excellent way to have stable build
environments that don't pollute the host OS. It is also much easier to maintain
stability across multiple host environments. Install stable version of
[Docker Desktop](https://www.docker.com/products/docker-desktop) relevant to
your native OS (macOS or Windows). Once installed, you can run docker commands
from the shell/terminal.

### Visual Studio Code

The recommended (and only supported) IDE for Barton is Visual Studio Code. When combined with a Docker backed devcontainer, new developers have the highest chance of successful working with Barton in the shortest amount of time.  Take a look at [the vscode development guide](docs/VSCODE_DEVELOPMENT.md) for more information.

## FAQs

Frequently asked questions can be found in [the FAQ](docs/FAQ.md).
