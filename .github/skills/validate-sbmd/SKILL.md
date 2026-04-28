---
name: validate-sbmd
description: Validate SBMD (Spec-Based Matter Driver) specification files. Use when the user has edited .sbmd files, wants to check schema conformance, verify embedded JavaScript syntax, or regenerate TypeScript stubs. Covers the validation script, stub generator, spec file locations, and automatic build-time validation.
license: Apache-2.0
compatibility: Requires the BartonCore Docker development container with Python 3 and a JavaScript engine (mquickjs or quickjs).
metadata:
  author: rdkcentral
  version: "1.0"
---

# Validate SBMD Specs

SBMD (Spec-Based Matter Drivers) are declarative YAML files with embedded JavaScript that map Matter protocol operations to BartonCore resources. Validation ensures schema conformance and JavaScript syntax correctness.

## Automatic Validation (During Build)

When `BCORE_MATTER_VALIDATE_SCHEMAS=ON` (the default), SBMD validation runs automatically as part of the build:

```bash
cmake --build build
```

This generates stubs from TypeScript definitions and validates all `.sbmd` files in one step. **This is the easiest way to validate.**

## Manual Validation

### Validate SBMD Spec Files

```bash
python3 scripts/ci/validate_sbmd_specs.py \
    core/deviceDrivers/matter/sbmd/sbmd-spec-schema.json \
    core/deviceDrivers/matter/sbmd/specs/*.sbmd \
    --stubs build/sbmd-stubs.json
```

This checks:
- YAML structure against the JSON schema
- Embedded JavaScript syntax using a JS engine

### Regenerate TypeScript Stubs

If TypeScript definition files have changed:

```bash
python3 scripts/ci/generate_sbmd_stubs.py \
    core/deviceDrivers/matter/sbmd/scriptCommon/sbmd-script.d.ts \
    build/sbmd-stubs.json
```

This regenerates `build/sbmd-stubs.json` from the TypeScript interface definitions in `sbmd-script.d.ts`. The stubs are used by the validator to check JavaScript against the expected API surface.

## File Locations

| Item | Location |
|------|----------|
| SBMD spec files | `core/deviceDrivers/matter/sbmd/specs/*.sbmd` |
| JSON schema | `core/deviceDrivers/matter/sbmd/sbmd-spec-schema.json` |
| TypeScript definitions | `core/deviceDrivers/matter/sbmd/scriptCommon/sbmd-script.d.ts` |
| Generated stubs | `build/sbmd-stubs.json` |
| Validation script | `scripts/ci/validate_sbmd_specs.py` |
| Stub generator | `scripts/ci/generate_sbmd_stubs.py` |
| JS embedding script | `scripts/embed-js-as-header.py` |

## Current SBMD Specs

| Spec File | Device Type |
|-----------|-------------|
| `light.sbmd` | Light (OnOff, LevelControl, ColorControl) |
| `door-lock.sbmd` | Door Lock |
| `contact-sensor.sbmd` | Contact Sensor |
| `occupancy-sensor.sbmd` | Occupancy Sensor |
| `air-quality-sensor.sbmd` | Air Quality Sensor |
| `water-leak-detector.sbmd` | Water Leak Detector |

## Error Recovery

If `python3` or validation dependencies are not found:

1. Check if `/.dockerenv` exists
2. If it does NOT exist, stop and tell the user: **"Validation tools are not available. Please run inside the BartonCore development container."**
3. If it does exist, ensure the project has been built at least once (stubs are generated during build)
