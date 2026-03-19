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

#
# Created by Thomas Lea <thomas_lea@comcast.com> on 2/17/2026.
#

"""
SBMD Specification Validator

Validates .sbmd YAML files against the SBMD JSON Schema and validates
embedded JavaScript scripts using the configured JS engine (mquickjs or quickjs).

Usage:
    validate_sbmd_specs.py [--js-engine ENGINE] <schema_file> <sbmd_file> [<sbmd_file> ...]

Example:
    validate_sbmd_specs.py schema.json specs/light.sbmd specs/door-lock.sbmd
    validate_sbmd_specs.py --js-engine quickjs schema.json specs/light.sbmd
"""

import sys
import os
import json
import argparse
import subprocess
import tempfile
import shutil
from pathlib import Path

try:
    import yaml
except ImportError:
    print("ERROR: PyYAML is required. Install with: apt install python3-yaml", file=sys.stderr)
    sys.exit(2)

try:
    import jsonschema
    from jsonschema import Draft202012Validator, ValidationError
except ImportError:
    print("ERROR: jsonschema is required. Install with: apt install python3-jsonschema", file=sys.stderr)
    sys.exit(2)


def load_stubs(stubs_file: str) -> dict:
    """Load JavaScript stubs from a generated JSON file."""
    with open(stubs_file, 'r') as f:
        data = json.load(f)
    stubs = data.get('stubs')
    if not stubs:
        raise ValueError("No 'stubs' key found in stubs file")
    return stubs


def load_schema(schema_path: str) -> dict:
    """Load and return the JSON schema."""
    with open(schema_path, 'r') as f:
        return json.load(f)


def load_sbmd_file(sbmd_path: str) -> dict:
    """Load and return an SBMD YAML file as a dictionary."""
    with open(sbmd_path, 'r') as f:
        return yaml.safe_load(f)


def validate_spec(spec_data: dict, validator: Draft202012Validator, file_path: str) -> list:
    """
    Validate a spec against the schema.
    Returns a list of error messages, empty if valid.
    """
    errors = []
    for error in validator.iter_errors(spec_data):
        path = " -> ".join(str(p) for p in error.absolute_path) if error.absolute_path else "(root)"
        errors.append(f"  Schema: {path}: {error.message}")
    return errors


def collect_sbmd_files(paths: list) -> list:
    """Collect .sbmd files from the given paths, warning on non-.sbmd files."""
    sbmd_files = []
    for path in paths:
        p = Path(path)
        if p.is_file() and p.suffix == '.sbmd':
            sbmd_files.append(str(p))
        elif p.is_file():
            print(f"WARNING: Skipping non-.sbmd file: {path}", file=sys.stderr)
        else:
            print(f"WARNING: Not a file: {path}", file=sys.stderr)
    return sorted(sbmd_files)


def validate_js_syntax(script: str, stub_type: str, location: str, js_compiler_path: str, stubs: dict) -> list:
    """
    Validate JavaScript syntax using the configured JS compiler (mqjs or qjsc).
    Uses bytecode compilation as a validation step without executing code.
    Returns a list of error messages, empty if valid.
    """
    errors = []

    if not script or not script.strip():
        return errors  # Empty scripts handled by schema validation

    # Get the appropriate stub for this script type
    stub = stubs.get(stub_type, '')

    # Wrap the script in a function like the runtime does
    # This allows 'return' statements at the top level
    wrapped_script = f'''{stub}
(function() {{
{script}
}})();
'''

    # Write to temp file and validate with the configured JS compiler
    try:
        with tempfile.NamedTemporaryFile(mode='w', suffix='.js', delete=False) as f:
            f.write(wrapped_script)
            temp_path = f.name

        # Use the JS compiler to validate syntax by compiling to bytecode (no execution)
        # The -o flag saves bytecode; output is discarded via os.devnull
        # The compiler will exit with error if there are syntax errors during parsing
        result = subprocess.run(
            [js_compiler_path, '-o', os.devnull, temp_path],
            capture_output=True,
            text=True,
            timeout=5
        )

        if result.returncode != 0:
            # Extract meaningful error from stderr
            error_msg = result.stderr.strip() if result.stderr else "Unknown syntax error"
            # Clean up temp file path from error message
            error_msg = error_msg.replace(temp_path, '<script>')
            errors.append(f"  Script ({location}): {error_msg}")

    except subprocess.TimeoutExpired:
        errors.append(f"  Script ({location}): Validation timed out")
    except Exception as e:
        errors.append(f"  Script ({location}): Validation error: {e}")
    finally:
        if 'temp_path' in locals():
            os.unlink(temp_path)

    return errors


def extract_scripts_from_resource(resource: dict, endpoint_id: str = None) -> list:
    """
    Extract all scripts from a resource definition.
    Returns list of (script, stub_type, location) tuples.
    """
    scripts = []
    resource_id = resource.get('id', 'unknown')
    location_prefix = f"endpoints[{endpoint_id}].{resource_id}" if endpoint_id else f"resources.{resource_id}"

    mapper = resource.get('mapper', {})

    # Read mapper
    read_mapper = mapper.get('read')
    if read_mapper and read_mapper.get('script'):
        scripts.append((
            read_mapper['script'],
            'read',
            f"{location_prefix}.mapper.read.script"
        ))

    # Write mapper
    write_mapper = mapper.get('write')
    if write_mapper and write_mapper.get('script'):
        # Determine if it's attribute or command write
        if write_mapper.get('attribute'):
            stub_type = 'write_attribute'
        elif write_mapper.get('command') or write_mapper.get('commands'):
            stub_type = 'write_command'
        else:
            # Skip if neither attribute, command, nor commands field is present
            stub_type = None

        if stub_type:
            scripts.append((
                write_mapper['script'],
                stub_type,
                f"{location_prefix}.mapper.write.script"
            ))

    # Execute mapper
    execute_mapper = mapper.get('execute')
    if execute_mapper:
        if execute_mapper.get('script'):
            scripts.append((
                execute_mapper['script'],
                'execute',
                f"{location_prefix}.mapper.execute.script"
            ))
        if execute_mapper.get('scriptResponse'):
            scripts.append((
                execute_mapper['scriptResponse'],
                'execute_response',
                f"{location_prefix}.mapper.execute.scriptResponse"
            ))

    # Event mapper
    event_mapper = mapper.get('event')
    if event_mapper and event_mapper.get('script'):
        scripts.append((
            event_mapper['script'],
            'event',
            f"{location_prefix}.mapper.event.script"
        ))

    return scripts


def extract_all_scripts(spec_data: dict) -> list:
    """
    Extract all scripts from an SBMD spec.
    Returns list of (script, stub_type, location) tuples.
    """
    scripts = []

    # Device-level resources
    for resource in spec_data.get('resources', []):
        scripts.extend(extract_scripts_from_resource(resource))

    # Endpoint resources
    for endpoint in spec_data.get('endpoints', []):
        endpoint_id = endpoint.get('id', 'unknown')
        for resource in endpoint.get('resources', []):
            scripts.extend(extract_scripts_from_resource(resource, endpoint_id))

    return scripts


def validate_scripts(spec_data: dict, js_compiler_path: str, stubs: dict) -> list:
    """
    Validate all JavaScript scripts in the spec.
    Returns a list of error messages, empty if all valid.
    """
    errors = []
    scripts = extract_all_scripts(spec_data)

    for script, stub_type, location in scripts:
        errors.extend(validate_js_syntax(script, stub_type, location, js_compiler_path, stubs))

    return errors


def find_mqjs() -> str:
    """Find the mqjs (MicroQuickJS) executable."""
    mqjs_path = shutil.which('mqjs')
    if mqjs_path:
        return mqjs_path

    # Check common locations
    common_paths = ['/usr/bin/mqjs', '/usr/local/bin/mqjs']
    for path in common_paths:
        if os.path.isfile(path) and os.access(path, os.X_OK):
            return path

    return None


def find_qjsc() -> str:
    """Find the qjsc (QuickJS compiler) executable."""
    qjsc_path = shutil.which('qjsc')
    if qjsc_path:
        return qjsc_path

    # Check common locations
    common_paths = ['/usr/bin/qjsc', '/usr/local/bin/qjsc']
    for path in common_paths:
        if os.path.isfile(path) and os.access(path, os.X_OK):
            return path

    return None


def main():
    default_engine = os.environ.get('BCORE_MATTER_SBMD_JS_ENGINE', 'mquickjs')
    parser = argparse.ArgumentParser(
        description='Validate SBMD specification files against the JSON schema and validate scripts'
    )
    parser.add_argument('schema', help='Path to the JSON schema file')
    parser.add_argument('specs', nargs='+', help='Path(s) to .sbmd files to validate')
    parser.add_argument('-q', '--quiet', action='store_true', help='Only show errors')
    parser.add_argument('--no-scripts', action='store_true', help='Skip JavaScript validation')
    parser.add_argument('--stubs', help='Path to generated stubs JSON file (from sbmd-script.d.ts)')
    parser.add_argument(
        '--js-engine',
        choices=['mquickjs', 'quickjs'],
        default=default_engine,
        help='JavaScript engine to use for script validation (default: %(default)s, '
             'or set BCORE_MATTER_SBMD_JS_ENGINE env var)'
    )
    args = parser.parse_args()

    # Load stubs for script validation
    try:
        stubs = load_stubs(args.stubs)
    except FileNotFoundError:
        print(f"ERROR: Stubs file not found: {args.stubs}", file=sys.stderr)
        return 1
    except (json.JSONDecodeError, ValueError) as e:
        print(f"ERROR: Invalid stubs file {args.stubs}: {e}", file=sys.stderr)
        return 1

    # Find JS compiler for script validation
    js_compiler_path = None
    if not args.no_scripts:
        if args.js_engine == 'quickjs':
            js_compiler_path = find_qjsc()
            if not js_compiler_path:
                print("WARNING: qjsc not found, skipping JavaScript validation", file=sys.stderr)
                print("         Install QuickJS from: https://bellard.org/quickjs/", file=sys.stderr)
        else:
            js_compiler_path = find_mqjs()
            if not js_compiler_path:
                print("WARNING: mqjs not found, skipping JavaScript validation", file=sys.stderr)
                print("         Build and install from: https://github.com/bellard/mquickjs", file=sys.stderr)

    # Load schema
    try:
        schema = load_schema(args.schema)
    except FileNotFoundError:
        print(f"ERROR: Schema file not found: {args.schema}", file=sys.stderr)
        return 1
    except json.JSONDecodeError as e:
        print(f"ERROR: Invalid JSON in schema file: {e}", file=sys.stderr)
        return 1

    # Create validator
    validator = Draft202012Validator(schema)

    # Collect .sbmd files from arguments
    sbmd_files = collect_sbmd_files(args.specs)
    if not sbmd_files:
        print("ERROR: No .sbmd files found", file=sys.stderr)
        return 1

    if not args.quiet:
        validation_type = "schema and scripts" if js_compiler_path else "schema only"
        print(f"Validating {len(sbmd_files)} SBMD file(s) ({validation_type})...")

    # Validate each file
    total_errors = 0
    for sbmd_file in sbmd_files:
        try:
            spec_data = load_sbmd_file(sbmd_file)
        except FileNotFoundError:
            print(f"ERROR: File not found: {sbmd_file}", file=sys.stderr)
            total_errors += 1
            continue
        except yaml.YAMLError as e:
            print(f"ERROR: Invalid YAML in {sbmd_file}: {e}", file=sys.stderr)
            total_errors += 1
            continue

        if spec_data is None:
            print(f"ERROR: Empty file: {sbmd_file}", file=sys.stderr)
            total_errors += 1
            continue

        # Schema validation
        errors = validate_spec(spec_data, validator, sbmd_file)

        # Script validation (only if schema is valid and JS compiler available)
        if not errors and js_compiler_path:
            errors.extend(validate_scripts(spec_data, js_compiler_path, stubs))

        if errors:
            print(f"FAIL: {sbmd_file}")
            for error in errors:
                print(error)
            total_errors += len(errors)
        elif not args.quiet:
            print(f"OK: {sbmd_file}")

    # Summary
    if total_errors > 0:
        print(f"\nValidation FAILED: {total_errors} error(s) in {len(sbmd_files)} file(s)")
        return 1
    else:
        if not args.quiet:
            print(f"\nValidation PASSED: {len(sbmd_files)} file(s) validated successfully")
        return 0


if __name__ == '__main__':
    sys.exit(main())
