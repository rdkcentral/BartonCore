## Why

BartonCore lacks formal, structured specifications for its components and features. Architecture knowledge lives in scattered docs, code comments, and tribal knowledge. This makes onboarding difficult, creates ambiguity around contracts between layers, and provides no baseline to validate changes against. Writing comprehensive specs now establishes a living reference that guides future development, review, and testing.

## What Changes

- Create in-depth specifications for each major BartonCore subsystem and capability
- Document the public API contract (GObject types, signals, resource model)
- Formalize the device driver interface and SBMD specification format
- Specify the protocol subsystem contracts (Matter, Zigbee, Thread)
- Document the core service internals (lifecycle, persistence, event system, communication monitoring)
- Specify the build system's modular configuration and dependency requirements

## Non-goals

- No code changes — this is purely a specification/documentation effort
- Not a user guide or tutorial — specs define *what* and *contracts*, not *how-to*
- Not exhaustive API reference docs — GIR/Doxygen generation handles that separately
- Not specifying future/planned features (OTA via DCL, telemetry, config backup) — only current behavior

## Capabilities

### New Capabilities

- `public-api`: BCoreClient interface, data model types (Device, Endpoint, Resource, Metadata), event hierarchy, provider interfaces, initialization parameters, error codes
- `resource-model`: URI-based resource addressing, resource types and modes, caching policies, metadata system, device class and endpoint profile contracts
- `device-drivers`: DeviceDriver struct interface, lifecycle callbacks, driver registration, native Zigbee driver catalog, Philips Hue driver
- `sbmd-system`: SBMD spec format and schema, parser pipeline, script engine (QuickJS + SbmdUtils), matter.js integration, read/write/execute/event mapper contracts, runtime data flow
- `matter-subsystem`: Matter SDK integration, commissioning orchestrator, device discovery, data cache subscriptions, persistent storage, access control, OTA provider, pluggable providers/delegates
- `zigbee-subsystem`: ZHAL hardware abstraction layer, Zigbee network management, device descriptor list (DDL), OTA firmware management, Zigbee-specific events
- `thread-subsystem`: OTBR D-Bus integration, Thread network backup/restore, NAT64 support, ephemeral key commissioning
- `core-services`: DeviceService orchestrator lifecycle, device driver manager, subsystem manager, communication watchdog, JSON database persistence, event producer/handler, discovery filters, storage monitoring
- `build-system`: CMake configuration options, feature flags, dependency versions, build targets, Docker integration, test infrastructure configuration

### Modified Capabilities

_(none — no existing specs to modify)_

## Impact

- **Affected layers**: All — API, core, drivers, subsystems, libs, build system, reference app
- **Artifacts produced**: 9 new spec files under `openspec/specs/`
- **CMake flags referenced**: `BCORE_ZIGBEE`, `BCORE_MATTER`, `BCORE_THREAD`, `BCORE_PHILIPS_HUE`, `BCORE_MATTER_USE_MATTERJS`, `BCORE_GEN_GIR`, `BCORE_BUILD_REFERENCE`, `BCORE_MATTER_ENABLE_OTA_PROVIDER`, `BCORE_TEST_COVERAGE`, `BCORE_BUILD_WITH_ASAN`
- **No code changes** — all output is OpenSpec specification markdown files
