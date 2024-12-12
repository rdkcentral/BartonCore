# BartonCore

BartonCore is a comprehensive IoT device service framework designed to manage and interact with various devices over various underlying protocols and transports.

## Table of Contents

- [BartonCore](#bartoncore)
  - [Table of Contents](#table-of-contents)
  - [Introduction](#introduction)
  - [Quickstart](#quickstart)
  - [Dependencies](#dependencies)
  - [Development](#development)
  - [Known Issues](#known-issues)

## Introduction

BartonCore produces a resource sensitive C/C++ based library that represents a local IoT device management and access service.
A client of the library would typically expose the provided functionality to a local or cloud based service or user interface.
These clients interface with the supported devices through a resource model that abstracts the underlying protocols and transports.
This abstraction allows for the seamless extension of new protocols, transports, and device types through a pluggable extension design.

The library provides device facilities for:
 - discovery
 - pairing (including secure credentialling)
 - configuration
 - firmware management
 - control
 - eventing

Included protocols and transports:
 - Matter
 - Zigbee
 - Thread

Additional features include:
- Configuration backup and restore (coming soon)
- Telemetry (coming soon)
- Proactive fault detection

For a more detailed discussion of the architecture and design, see [docs/device-service-design.md](docs/device-service-design.md).

## Quickstart

```bash
# Check out the code and build with Docker
git clone https://github.com/rdkcentral/BartonCore.git
cd BartonCore
./dockerw ./build.sh

# Run the reference app which includes interactive CLI
./dockerw build/reference/brtn-ds-reference
```
## Dependencies

This project uses Docker to facilitate development and also be a record of dependencies. Please see [docker/Dockerfile](docker/Dockerfile) for details.

BartonCore is modular, so the actual dependencies will change based on included functionality. See [config/cmake/options.cmake](config/cmake/options.cmake) for the available configuration options.

## Development

Visual Studio Code is the preferred and supported development tool. This project includes devcontainer support and other vscode specific configuration to make it quick and easy to get started developing and debugging.  See [docs/DEBUGGING.md](docs/DEBUGGING.md) for more information on debugging.

See [CONTRIBUTING.md](CONTRIBUTING.md) for information on contributing to this project.

## Known Issues

- Zigbee support requires an implementation of the ZHAL (Zigbee Hardware Abastraction Layer). An implementation for Silabs hardware will be available soon.
- Adding custom device drivers is not officially supported by the public interfaces. This will be addressed soon.
