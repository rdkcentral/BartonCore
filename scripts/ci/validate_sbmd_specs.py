#!/usr/bin/env python3
"""
SBMD Specification Validator

Validates .sbmd YAML files against the SBMD JSON Schema and validates
embedded JavaScript scripts using QuickJS compiler (qjsc) for syntax checking.

Usage:
    validate_sbmd_specs.py <schema_file> <spec_file_or_directory> [<spec_file_or_directory> ...]

Example:
    validate_sbmd_specs.py schema.json specs/
    validate_sbmd_specs.py schema.json door-lock.sbmd light.sbmd
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


def find_sbmd_files(paths: list) -> list:
    """Find all .sbmd files in the given paths (files or directories)."""
    sbmd_files = []
    for path in paths:
        p = Path(path)
        if p.is_file() and p.suffix == '.sbmd':
            sbmd_files.append(str(p))
        elif p.is_dir():
            sbmd_files.extend(str(f) for f in p.rglob('*.sbmd'))
        elif p.is_file():
            print(f"WARNING: Skipping non-.sbmd file: {path}", file=sys.stderr)
    return sorted(sbmd_files)


def validate_js_syntax(script: str, stub_type: str, location: str, qjsc_path: str, stubs: dict) -> list:
    """
    Validate JavaScript syntax using QuickJS compiler (qjsc).
    Uses compile-only mode to check syntax without executing code.
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

    # Write to temp file and compile with qjsc to validate syntax
    temp_path = None
    temp_output = None
    try:
        with tempfile.NamedTemporaryFile(mode='w', suffix='.js', delete=False) as f:
            f.write(wrapped_script)
            temp_path = f.name

        # Create a temp output file for the compiled bytecode
        with tempfile.NamedTemporaryFile(delete=False) as f:
            temp_output = f.name

        # Use qjsc to compile the script to bytecode (syntax check without execution)
        # The -o flag specifies output file; compilation will fail on syntax errors
        result = subprocess.run(
            [qjsc_path, '-o', temp_output, temp_path],
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
        if temp_path and os.path.exists(temp_path):
            os.unlink(temp_path)
        if temp_output and os.path.exists(temp_output):
            os.unlink(temp_output)

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
        else:
            stub_type = 'write_command'
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


def validate_scripts(spec_data: dict, qjsc_path: str, stubs: dict) -> list:
    """
    Validate all JavaScript scripts in the spec.
    Returns a list of error messages, empty if all valid.
    """
    errors = []
    scripts = extract_all_scripts(spec_data)

    for script, stub_type, location in scripts:
        errors.extend(validate_js_syntax(script, stub_type, location, qjsc_path, stubs))

    return errors


def find_qjsc() -> str:
    """Find the qjsc executable (QuickJS compiler)."""
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
    parser = argparse.ArgumentParser(
        description='Validate SBMD specification files against the JSON schema and validate scripts'
    )
    parser.add_argument('schema', help='Path to the JSON schema file')
    parser.add_argument('specs', nargs='+', help='Path(s) to .sbmd files or directories')
    parser.add_argument('-q', '--quiet', action='store_true', help='Only show errors')
    parser.add_argument('--no-scripts', action='store_true', help='Skip JavaScript validation')
    parser.add_argument('--stubs', required=True, help='Path to generated stubs JSON file (from sbmd-script.d.ts)')
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

    # Find qjsc for script validation
    qjsc_path = None
    if not args.no_scripts:
        qjsc_path = find_qjsc()
        if not qjsc_path:
            print("WARNING: qjsc not found, skipping JavaScript validation", file=sys.stderr)
            print("         Install with: apt install quickjs", file=sys.stderr)

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

    # Find all .sbmd files
    sbmd_files = find_sbmd_files(args.specs)
    if not sbmd_files:
        print("ERROR: No .sbmd files found", file=sys.stderr)
        return 1

    if not args.quiet:
        validation_type = "schema and scripts" if qjsc_path else "schema only"
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

        # Script validation (only if schema is valid and qjsc available)
        if not errors and qjsc_path:
            errors.extend(validate_scripts(spec_data, qjsc_path, stubs))

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
