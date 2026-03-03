## Context

BartonCore is a mature IoT device management library (~122K lines of C/C++) with architecture documentation spread across multiple markdown files, code comments, and header files. There are no formal, structured specifications that define component contracts, behavioral requirements, or testable scenarios. This change creates comprehensive specs as OpenSpec artifacts — purely a documentation effort with no code changes.

```
┌──────────────────────────────────────────────────────────────────────┐
│                    SPECIFICATION COVERAGE MAP                       │
├──────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  ┌─────────────┐    specs/public-api/spec.md                        │
│  │ PUBLIC API  │    specs/resource-model/spec.md                    │
│  └──────┬──────┘                                                     │
│         │                                                            │
│  ┌──────┴──────┐    specs/core-services/spec.md                     │
│  │    CORE     │                                                     │
│  └──┬─────┬────┘                                                     │
│     │     │                                                          │
│  ┌──┴──┐ ┌┴────────┐  specs/device-drivers/spec.md                  │
│  │ DD  │ │SUBSYSTEM │  specs/sbmd-system/spec.md                    │
│  └─────┘ └─┬──┬──┬─┘  specs/matter-subsystem/spec.md               │
│            │  │  │     specs/zigbee-subsystem/spec.md                │
│            │  │  │     specs/thread-subsystem/spec.md                │
│            M  Z  T                                                   │
│                                                                      │
│  ┌─────────────┐    specs/build-system/spec.md                      │
│  │ BUILD/TEST  │                                                     │
│  └─────────────┘                                                     │
│                                                                      │
└──────────────────────────────────────────────────────────────────────┘
```

## Goals / Non-Goals

**Goals:**
- Document the current behavior of every major BartonCore component as testable specs
- Establish a baseline specification that future changes can reference and modify
- Cover all layers: public API, resource model, core services, drivers, subsystems, build system
- Write specs that are concrete and testable — each scenario maps to a potential test case

**Non-Goals:**
- No code changes whatsoever — this is specification-only
- Not specifying planned/future features (OTA DCL, telemetry, config backup)
- Not a tutorial or user guide
- Not replacing existing docs — specs complement the architecture doc and SBMD docs
- Not generating API reference docs (GIR handles that)

## Decisions

### Decision 1: Nine spec files matching proposal capabilities

Each top-level component/feature area gets its own spec file. This keeps specs focused and independently maintainable. The alternative — a single monolithic spec — would be unwieldy for review and future modification.

Spec files:
1. `public-api` — BCoreClient, data model, events, providers, init params
2. `resource-model` — URI addressing, types, modes, device classes, endpoint profiles
3. `device-drivers` — DeviceDriver interface, lifecycle, registration, native driver catalog
4. `sbmd-system` — SBMD format, parser, script engine, mapper contracts
5. `matter-subsystem` — CHIP SDK integration, commissioning, discovery, cache, providers
6. `zigbee-subsystem` — ZHAL HAL, network mgmt, DDL, firmware, Zigbee events
7. `thread-subsystem` — OTBR integration, backup/restore, NAT64, ephemeral keys
8. `core-services` — Orchestrator, driver manager, subsystem manager, watchdog, database, events
9. `build-system` — CMake flags, dependencies, targets, Docker, test config

### Decision 2: Spec requirements based on current codebase behavior

Specs document *what the system currently does*, derived from reading the source code. This avoids aspirational specs that don't match reality. Future changes will use MODIFIED/ADDED operations against this baseline.

### Decision 3: WHEN/THEN scenario format for testability

Every requirement includes at least one scenario with WHEN/THEN conditions. This makes specs directly usable as test case definitions and provides concrete acceptance criteria.

## Risks / Trade-offs

- **[Specs may drift from code]** → Mitigated by basing all specs on current source code analysis. Future code changes should update specs via OpenSpec change proposals.
- **[Large scope — 9 spec files]** → Mitigated by keeping each spec focused on its component boundary. Specs can be written independently.
- **[Incomplete coverage of edge cases]** → Acceptable for baseline specs. Edge cases can be added as they surface during development and testing.

## Open Questions

_(none — this is a documentation effort with clear scope)_
