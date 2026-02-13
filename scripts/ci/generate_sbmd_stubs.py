#!/usr/bin/env python3
"""
SBMD Stub Generator

Parses sbmd-script.d.ts TypeScript definitions and generates JavaScript
stub declarations for use by the SBMD validator.

This ensures the validator stubs stay in sync with the TypeScript definitions
without manual duplication.

Usage:
    generate_sbmd_stubs.py <d.ts_file> <output_json>

Example:
    generate_sbmd_stubs.py sbmd-script.d.ts sbmd-stubs.json
"""

import sys
import re
import json
import argparse
from pathlib import Path


def parse_interface(content: str, name: str) -> dict:
    """
    Parse a TypeScript interface and extract property names with placeholder values.
    Returns a dict suitable for JSON serialization.
    """
    # Find the interface definition
    # Match: interface Name { ... } or interface Name extends Base { ... }
    pattern = rf'interface\s+{re.escape(name)}\s+(?:extends\s+\w+\s+)?\{{([^}}]+)\}}'
    match = re.search(pattern, content, re.DOTALL)

    if not match:
        return {}

    body = match.group(1)
    props = {}

    # Parse properties: name: type; or name?: type;
    # Handles multiline JSDoc comments between properties
    prop_pattern = r'^\s*(\w+)\??:\s*([^;]+);'

    for line in body.split('\n'):
        line = line.strip()
        # Skip comments and empty lines
        if not line or line.startswith('*') or line.startswith('//'):
            continue

        prop_match = re.match(prop_pattern, line)
        if prop_match:
            prop_name = prop_match.group(1)
            prop_type = prop_match.group(2).strip()

            # Generate placeholder value based on type
            if prop_type == 'string':
                props[prop_name] = ""
            elif prop_type == 'number':
                props[prop_name] = 0
            elif prop_type == 'boolean':
                props[prop_name] = False
            elif prop_type == 'any':
                props[prop_name] = None
            elif prop_type.startswith('string[]'):
                props[prop_name] = []
            else:
                # Default to null for complex types
                props[prop_name] = None

    return props


def parse_base_context(content: str) -> dict:
    """Parse SbmdBaseContext interface for inherited properties."""
    return parse_interface(content, 'SbmdBaseContext')


def generate_stubs(dts_content: str) -> dict:
    """
    Parse the .d.ts file and generate stub definitions for each script context.

    Returns a dict mapping script type to JavaScript variable declarations.
    """
    # Get base context properties (inherited by most interfaces)
    base_props = parse_base_context(dts_content)

    # Define the mapping from script types to their TypeScript interface names
    # and the global variable name used in scripts
    contexts = {
        'read': {
            'interface': 'SbmdReadArgs',
            'variable': 'sbmdReadArgs',
            'extends_base': True
        },
        'write_attribute': {
            'interface': 'SbmdWriteArgs',
            'variable': 'sbmdWriteArgs',
            'extends_base': True
        },
        'write_command': {
            'interface': 'SbmdWriteCommandArgs',
            'variable': 'sbmdWriteArgs',
            'extends_base': False  # Has its own deviceUuid
        },
        'execute': {
            'interface': 'SbmdCommandArgs',
            'variable': 'sbmdCommandArgs',
            'extends_base': True
        },
        'execute_response': {
            'interface': 'SbmdCommandResponseArgs',
            'variable': 'sbmdCommandResponseArgs',
            'extends_base': True
        }
    }

    stubs = {}

    for script_type, ctx in contexts.items():
        # Parse the interface
        props = parse_interface(dts_content, ctx['interface'])

        # Merge with base context if extended
        if ctx['extends_base']:
            merged = {**base_props, **props}
        else:
            merged = props

        # Generate JavaScript stub declaration
        var_name = ctx['variable']
        props_json = json.dumps(merged, indent=4)

        stub = f"// Generated from {ctx['interface']} in sbmd-script.d.ts\nvar {var_name} = {props_json};\n"
        stubs[script_type] = stub

    return stubs


def main():
    parser = argparse.ArgumentParser(
        description='Generate JavaScript stubs from sbmd-script.d.ts'
    )
    parser.add_argument('dts_file', help='Path to sbmd-script.d.ts')
    parser.add_argument('output_file', help='Output JSON file path')
    args = parser.parse_args()

    # Read the .d.ts file
    try:
        dts_path = Path(args.dts_file)
        dts_content = dts_path.read_text()
    except FileNotFoundError:
        print(f"ERROR: File not found: {args.dts_file}", file=sys.stderr)
        return 1
    except Exception as e:
        print(f"ERROR: Failed to read {args.dts_file}: {e}", file=sys.stderr)
        return 1

    # Generate stubs
    stubs = generate_stubs(dts_content)

    if not stubs:
        print("ERROR: No stubs generated - check .d.ts file format", file=sys.stderr)
        return 1

    # Write output JSON
    try:
        output = {
            'source': args.dts_file,
            'stubs': stubs
        }
        output_path = Path(args.output_file)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(json.dumps(output, indent=2))
    except Exception as e:
        print(f"ERROR: Failed to write {args.output_file}: {e}", file=sys.stderr)
        return 1

    print(f"Generated {len(stubs)} stubs from {args.dts_file} -> {args.output_file}")
    return 0


if __name__ == '__main__':
    sys.exit(main())
