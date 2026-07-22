#!/usr/bin/env python3
# ------------------------------ tabstop = 4 ----------------------------------
#
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2026 Comcast Cable Communications Management, LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0
#
# ------------------------------ tabstop = 4 ----------------------------------

"""
SBMD v4 Specification Validator

Validates .sbmd.js driver files against the SBMD v4 JSON Schema.

The validator uses Node.js to evaluate each .sbmd.js file in a sandbox,
extract the SbmdDriver() registration object as JSON (with functions
serialised as `true`), then validates the resulting JSON against the
schema using jsonschema.

The schema argument can be a single JSON schema file or a directory of
versioned schemas (resolved as sbmd-spec-schema-v{version}.json).

Usage:
    validate_sbmd_specs.py <schema_file_or_dir> <sbmd_file> [<sbmd_file> ...]

Example:
    validate_sbmd_specs.py schema/ specs/light.sbmd.js specs/door-lock.sbmd.js
    validate_sbmd_specs.py schema/sbmd-spec-schema-v5.0.json specs/*.sbmd.js
"""

import sys
import os
import json
import argparse
import subprocess
import shutil
from pathlib import Path
from typing import Optional

try:
    import jsonschema
    from jsonschema import Draft202012Validator
except ImportError:
    print(
        "ERROR: jsonschema is required. Install with: pip install jsonschema",
        file=sys.stderr,
    )
    sys.exit(2)

# Directory containing this script — used to locate the extraction harness.
SCRIPT_DIR = Path(__file__).resolve().parent
EXTRACTOR_SCRIPT = SCRIPT_DIR / "sbmd_extract_registration.js"

# Cache of compiled JSON schema validators: {schema_path: Draft202012Validator}
_validators: dict[str, Draft202012Validator] = {}


def find_node() -> Optional[str]:
    """Find the Node.js executable."""
    node = shutil.which("node")
    if node:
        return node

    for candidate in ["/usr/bin/node", "/usr/local/bin/node"]:
        if os.path.isfile(candidate) and os.access(candidate, os.X_OK):
            return candidate

    return None


def load_schema(schema_path: str) -> dict:
    """Load and return the JSON schema."""
    with open(schema_path, "r") as f:
        return json.load(f)


def resolve_schema_for_version(
    schema_arg: str, schema_version: str
) -> Optional[str]:
    """
    Resolve the schema file path for a given schemaVersion.

    If schema_arg is a file, use it directly.
    If schema_arg is a directory, search for sbmd-spec-schema-v{version}.json.
    """
    if os.path.isfile(schema_arg):
        return schema_arg

    if os.path.isdir(schema_arg):
        filename = f"sbmd-spec-schema-v{schema_version}.json"
        for candidate in Path(schema_arg).rglob(filename):
            return str(candidate)

    return None


def extract_registration(
    sbmd_file: str, node_path: str
) -> tuple[Optional[dict], Optional[str]]:
    """
    Extract the SbmdDriver() registration object from a .sbmd.js file.

    Returns (registration_dict, None) on success, or (None, error_msg) on failure.
    """
    try:
        result = subprocess.run(
            [node_path, str(EXTRACTOR_SCRIPT), sbmd_file],
            capture_output=True,
            text=True,
            timeout=10,
        )
    except subprocess.TimeoutExpired:
        return None, "Extraction timed out"
    except Exception as e:
        return None, f"Extraction error: {e}"

    if result.returncode != 0:
        stderr = result.stderr.strip() if result.stderr else "Unknown error"
        return None, f"Extraction failed: {stderr}"

    stdout = result.stdout.strip()
    if not stdout:
        return None, "Extraction produced no output"

    try:
        data = json.loads(stdout)
    except json.JSONDecodeError as e:
        return None, f"Invalid JSON from extraction: {e}"

    return data, None


def validate_against_schema(
    reg_data: dict, validator: Draft202012Validator
) -> list[str]:
    """
    Validate a registration object against the schema.
    Returns a list of error messages, empty if valid.
    """
    errors = []
    for error in validator.iter_errors(reg_data):
        path = (
            " -> ".join(str(p) for p in error.absolute_path)
            if error.absolute_path
            else "(root)"
        )
        errors.append(f"  Schema: {path}: {error.message}")
    return errors


def collect_sbmd_files(paths: list[str]) -> list[str]:
    """Collect .sbmd.js files, warning on non-matching files."""
    sbmd_files = []
    for p_str in paths:
        p = Path(p_str)
        if p.is_file() and p.name.endswith(".sbmd.js"):
            sbmd_files.append(str(p))
        elif p.is_file():
            print(
                f"WARNING: Skipping non-.sbmd.js file: {p_str}",
                file=sys.stderr,
            )
        else:
            print(f"WARNING: Not a file: {p_str}", file=sys.stderr)
    return sorted(sbmd_files)


def validate_sbmd_file(
    sbmd_file: str,
    schema_arg: str,
    node_path: str,
    quiet: bool,
) -> int:
    """
    Validate a single .sbmd.js file.

    Returns the number of errors found.
    """
    # Step 1: Extract registration via Node.js
    reg_data, extract_error = extract_registration(sbmd_file, node_path)

    if extract_error:
        print(f"FAIL: {sbmd_file}")
        print(f"  {extract_error}")
        return 1

    # Step 2: Resolve schema version
    schema_version = reg_data.get("schemaVersion", "")
    schema_path = resolve_schema_for_version(schema_arg, schema_version)

    if not schema_path:
        print(f"FAIL: {sbmd_file}")
        print(
            f"  Schema: No schema found for schemaVersion "
            f"'{schema_version}' in {schema_arg}"
        )
        return 1

    # Step 3: Get or create validator
    if schema_path not in _validators:
        try:
            schema = load_schema(schema_path)
            _validators[schema_path] = Draft202012Validator(schema)
        except FileNotFoundError:
            print(
                f"ERROR: Schema file not found: {schema_path}",
                file=sys.stderr,
            )
            return 1
        except json.JSONDecodeError as e:
            print(
                f"ERROR: Invalid JSON in schema: {schema_path}: {e}",
                file=sys.stderr,
            )
            return 1

    validator = _validators[schema_path]

    # Step 4: Validate
    errors = validate_against_schema(reg_data, validator)

    if errors:
        print(f"FAIL: {sbmd_file}")
        for error in errors:
            print(error)
    elif not quiet:
        print(f"OK: {sbmd_file}")

    return len(errors)


def main():
    parser = argparse.ArgumentParser(
        description="Validate SBMD v4 .sbmd.js specification files against "
        "the JSON schema"
    )
    parser.add_argument(
        "schema", help="Path to the JSON schema file or schema directory"
    )
    parser.add_argument(
        "specs", nargs="+", help="Path(s) to .sbmd.js files to validate"
    )
    parser.add_argument(
        "-q", "--quiet", action="store_true", help="Only show errors"
    )
    args = parser.parse_args()

    # Find Node.js
    node_path = find_node()
    if not node_path:
        print(
            "ERROR: Node.js (node) not found. Required for .sbmd.js extraction.",
            file=sys.stderr,
        )
        return 1

    # Validate schema argument exists
    if not os.path.exists(args.schema):
        print(
            f"ERROR: Schema path not found: {args.schema}", file=sys.stderr
        )
        return 1

    # Verify extractor script exists
    if not EXTRACTOR_SCRIPT.is_file():
        print(
            f"ERROR: Extractor script not found: {EXTRACTOR_SCRIPT}",
            file=sys.stderr,
        )
        return 1

    # Collect .sbmd.js files
    sbmd_files = collect_sbmd_files(args.specs)
    if not sbmd_files:
        print("ERROR: No .sbmd.js files found", file=sys.stderr)
        return 1

    if not args.quiet:
        schema_mode = "directory" if os.path.isdir(args.schema) else "file"
        print(
            f"Validating {len(sbmd_files)} SBMD file(s) "
            f"(schema {schema_mode})..."
        )

    # Validate each file
    total_errors = 0
    for sbmd_file in sbmd_files:
        total_errors += validate_sbmd_file(
            sbmd_file, args.schema, node_path, args.quiet
        )

    # Summary
    if total_errors > 0:
        print(
            f"\nValidation FAILED: {total_errors} error(s) in "
            f"{len(sbmd_files)} file(s)"
        )
        return 1
    else:
        if not args.quiet:
            print(
                f"\nValidation PASSED: {len(sbmd_files)} file(s) "
                f"validated successfully"
            )
        return 0


if __name__ == "__main__":
    sys.exit(main())
