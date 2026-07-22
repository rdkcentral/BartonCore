---
name: validate-sbmd
description: Validate SBMD (Spec-Based Matter Driver) v4 specification files. Use when the user has edited .sbmd.js files, wants to check schema conformance, or verify driver structure. Covers the validation script, JSON schema, spec file locations, and automatic build-time validation.
license: Apache-2.0
compatibility: Requires the BartonCore Docker development container with Python 3, Node.js, and the jsonschema Python package.
metadata:
  author: rdkcentral
  version: "2.0"
---

# Validate SBMD Specs

SBMD v4 drivers are `.sbmd.js` JavaScript files that register a driver via `SbmdDriver({...})`. Validation ensures the registration object conforms to the JSON schema.

## Automatic Validation (During Build)

When `BCORE_MATTER_VALIDATE_SCHEMAS=ON` (the default), SBMD validation runs automatically as part of the build:

```bash
cmake --build build
```

The `validate_sbmd_specs` target uses Node.js to extract each driver's registration object and validates it against the JSON schema. **This is the easiest way to validate.**

## Manual Validation

### Validate SBMD Spec Files

```bash
python3 scripts/ci/validate_sbmd_v4_specs.py \
    core/deviceDrivers/matter/sbmd/schema \
    core/deviceDrivers/matter/sbmd/specs/*.sbmd.js
```

This:
1. Uses Node.js to evaluate each `.sbmd.js` file in a sandbox
2. Extracts the `SbmdDriver()` registration object as JSON (functions → `true`)
3. Validates the JSON against `sbmd-spec-schema-v5.0.json`

The validator evaluates each `.sbmd.js` file, extracts the `SbmdDriver()` registration object, and validates it against the JSON schema.

## File Locations

| Item | Location |
|------|----------|
| SBMD spec files | `core/deviceDrivers/matter/sbmd/specs/*.sbmd.js` |
| JSON schema | `core/deviceDrivers/matter/sbmd/schema/sbmd-spec-schema-v5.0.json` |
| TypeScript definitions (editor tooling only — not used by the validator) | `core/deviceDrivers/matter/sbmd/scriptCommon/sbmd-script.d.ts` |
| Validation script | `scripts/ci/validate_sbmd_v4_specs.py` |
| Extraction harness | `scripts/ci/sbmd_extract_registration.js` |

## Discovering Available SBMD Specs

To see the current set of specs in the repository, list the directory contents:

```bash
ls core/deviceDrivers/matter/sbmd/specs/
```

## Error Recovery

If `python3` or validation dependencies are not found:

1. Check if `/.dockerenv` exists
2. If it does NOT exist, stop and tell the user: **"Validation tools are not available. Please run inside the BartonCore development container."**
3. If it does exist, ensure the validator's dependencies are installed: Node.js (used to extract the `SbmdDriver()` registration object) and the `jsonschema` Python package (used to validate it against the schema)
