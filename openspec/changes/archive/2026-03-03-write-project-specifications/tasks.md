## 1. Public API Specification

- [x] 1.1 Review specs/public-api/spec.md against api/c/public/ headers for accuracy and completeness
- [x] 1.2 Verify all GObject type names, signal names, and property constants match source code
- [x] 1.3 Validate provider interface contracts (TokenProvider, NetworkCredentialsProvider, PropertyProvider) against headers

## 2. Resource Model Specification

- [x] 2.1 Review specs/resource-model/spec.md against core/src/ resource handling for accuracy
- [x] 2.2 Verify URI addressing scheme, resource types, and mode bitmask values against source code
- [x] 2.3 Validate device class definitions and endpoint profile contracts against implementation

## 3. Device Drivers Specification

- [x] 3.1 Review specs/device-drivers/spec.md against api/c/public/barton/ DeviceDriver struct definition
- [x] 3.2 Verify lifecycle callback signatures and driver registration flow against source code
- [x] 3.3 Cross-check the native Zigbee driver catalog (8 drivers) against core/deviceDrivers/ contents

## 4. SBMD System Specification

- [x] 4.1 Review specs/sbmd-system/spec.md against core/deviceDrivers/matter/sbmd/ implementation
- [x] 4.2 Verify SBMD YAML schema fields (bartonMeta, matterMeta, reporting, endpoints, mappers) against parser code
- [x] 4.3 Validate QuickJS engine integration and SbmdUtils library functions against source code
- [x] 4.4 Cross-check the SBMD spec catalog (5 specs) against core/deviceDrivers/matter/sbmd/specs/ contents

## 5. Matter Subsystem Specification

- [x] 5.1 Review specs/matter-subsystem/spec.md against core/src/subsystems/matter/ implementation
- [x] 5.2 Verify commissioning orchestrator flow and pluggable provider/delegate interfaces against source
- [x] 5.3 Validate Matter data cache, persistent storage, and OTA provider contracts against C++ code

## 6. Zigbee Subsystem Specification

- [x] 6.1 Review specs/zigbee-subsystem/spec.md against core/src/subsystems/zigbee/ and ZHAL interfaces
- [x] 6.2 Verify ZHAL callback signatures, message types, and status codes against header files
- [x] 6.3 Validate Zigbee network management operations (init, backup, restore, channel change) against source

## 7. Thread Subsystem Specification

- [x] 7.1 Review specs/thread-subsystem/spec.md against core/src/subsystems/thread/ implementation
- [x] 7.2 Verify OTBR D-Bus interface names and method signatures against source code
- [x] 7.3 Validate conditional compilation guards (BCORE_THREAD flag) and Thread credential flow

## 8. Core Services Specification

- [x] 8.1 Review specs/core-services/spec.md against core/src/ orchestrator and manager implementations
- [x] 8.2 Verify DeviceService two-phase lifecycle, subsystem readiness gate, and driver manager against source
- [x] 8.3 Validate communication watchdog timers, JSON database schema, and event producer contracts against code

## 9. Build System Specification

- [x] 9.1 Review specs/build-system/spec.md against top-level CMakeLists.txt and config/cmake/ modules
- [x] 9.2 Verify all 16 feature flags, their defaults, and dependency relationships against CMake source
- [x] 9.3 Validate dependency version requirements and build target definitions against CMake configuration

## 10. Cross-Specification Validation

- [x] 10.1 Verify cross-references between specs are consistent (e.g., public-api ↔ resource-model, device-drivers ↔ sbmd-system)
- [x] 10.2 Ensure terminology and naming conventions are uniform across all 9 spec files
- [x] 10.3 Confirm all CMake flag references are consistent between build-system spec and subsystem specs
