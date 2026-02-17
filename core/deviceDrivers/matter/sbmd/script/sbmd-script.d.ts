/**
 * SBMD Script Interface Type Definitions
 *
 * This file provides TypeScript type definitions for the JSON interfaces
 * used by SBMD (Specification-Based Matter Driver) mapper scripts.
 *
 * Scripts are executed in a QuickJS JavaScript runtime. Each script type
 * receives a specific input object as a global variable and must return
 * a result object with the expected structure.
 *
 * @file sbmd-script.d.ts
 * @see docs/SBMD-Design.md for detailed documentation
 */

// =============================================================================
// Common Types
// =============================================================================

/**
 * Base context available to all SBMD scripts.
 */
interface SbmdBaseContext {
    /** Device UUID */
    deviceUuid: string;

    /** Matter cluster ID */
    clusterId: number;

    /** Cluster feature map (bitmask indicating enabled features) */
    featureMap: number;

    /** Endpoint ID (empty string for device-level resources) */
    endpointId: string;
}

// =============================================================================
// Read Mapper Interface
// =============================================================================

/**
 * Input object for read mapper scripts.
 *
 * Available as global variable: `sbmdReadArgs`
 *
 * @example
 * // Boolean passthrough
 * return { output: sbmdReadArgs.input };
 *
 * @example
 * // Enum to boolean conversion (Door Lock state)
 * // LockState enum: 0=NotFullyLocked, 1=Locked, 2=Unlocked, 3=Unlatched
 * return { output: sbmdReadArgs.input === 1 };
 *
 * @example
 * // Percentage conversion (Level Control 0-254 to 0-100)
 * var percent = Math.round(sbmdReadArgs.input / 254 * 100);
 * return { output: percent.toString() };
 */
interface SbmdReadArgs extends SbmdBaseContext {
    /**
     * Attribute value converted from Matter TLV to JSON.
     * Type depends on the Matter attribute type:
     * - boolean for bool attributes
     * - number for integer/enum attributes
     * - string for char_string attributes
     * - object for struct attributes
     * - array for list attributes
     */
    input: any;

    /** Matter attribute ID */
    attributeId: number;

    /** Attribute name from the SBMD spec */
    attributeName: string;

    /** Matter attribute type (e.g., "bool", "uint8", "enum8") */
    attributeType: string;
}

/**
 * Output object for read mapper scripts.
 *
 * @example
 * return { output: "true" };
 * return { output: 50 };  // Numbers are converted to strings
 */
interface SbmdReadResult {
    /**
     * Value for the Barton resource.
     * Will be converted to a string for the resource value.
     */
    output: string | number | boolean;
}

// =============================================================================
// Write Mapper Interface (Attribute Write)
// =============================================================================

/**
 * Input object for attribute write mapper scripts.
 *
 * Available as global variable: `sbmdWriteArgs`
 *
 * @example
 * // Boolean string to boolean
 * return { output: sbmdWriteArgs.input === 'true' };
 *
 * @example
 * // String to integer
 * const secs = parseInt(sbmdWriteArgs.input, 10);
 * return { output: secs };
 *
 * @example
 * // Percentage to level (0-100 to 0-254)
 * var percent = parseInt(sbmdWriteArgs.input, 10);
 * var level = Math.round(percent / 100 * 254);
 * return { output: level };
 */
interface SbmdWriteArgs extends SbmdBaseContext {
    /** Barton resource string value to write */
    input: string;

    /** Matter attribute ID */
    attributeId: number;

    /** Attribute name from the SBMD spec */
    attributeName: string;

    /** Matter attribute type (e.g., "bool", "uint8", "int16") */
    attributeType: string;
}

/**
 * Output object for attribute write mapper scripts.
 *
 * @example
 * return { output: true };      // Boolean attribute
 * return { output: 127 };       // Integer attribute
 * return { output: "hello" };   // String attribute
 */
interface SbmdWriteResult {
    /**
     * Value to write to the Matter attribute.
     * Must match the expected Matter type (will be TLV-encoded).
     */
    output: any;
}

// =============================================================================
// Write Command Mapper Interface
// =============================================================================

/**
 * Input object for write-command mapper scripts.
 *
 * Used when a resource write maps to one or more Matter commands
 * instead of an attribute write. When multiple commands are available,
 * the script must select which command to execute.
 *
 * Available as global variable: `sbmdWriteArgs`
 *
 * @example
 * // Single command (auto-selected, no 'command' field needed)
 * return { output: { Level: 127, TransitionTime: 0 } };
 *
 * @example
 * // Multiple commands - select based on input value
 * var isOn = sbmdWriteArgs.input === 'true';
 * return {
 *     output: null,
 *     command: isOn ? 'On' : 'Off'
 * };
 */
interface SbmdWriteCommandArgs {
    /** Barton resource string value to write */
    input: string;

    /** Device UUID */
    deviceUuid: string;

    /**
     * Array of available command names.
     * When there is only one command, it is auto-selected and
     * the script does not need to return a 'command' field.
     * When multiple commands exist, the script must return which
     * command to execute in the 'command' field.
     */
    commands: string[];
}

/**
 * Output object for write-command mapper scripts.
 *
 * @example
 * // Single command - just provide output args
 * return { output: { Level: 127, TransitionTime: 0 } };
 *
 * @example
 * // Multiple commands - must specify which command
 * return { output: null, command: 'On' };
 */
interface SbmdWriteCommandResult {
    /**
     * Command arguments to encode as TLV.
     * Can be:
     * - null: Command with no arguments
     * - single value: Command with one argument
     * - object: Command with named arguments
     * - array of bytes: For octet string arguments
     */
    output: any;

    /**
     * Name of the command to execute.
     * Required when multiple commands are available.
     * Optional (ignored) when only one command is available.
     */
    command?: string;
}

// =============================================================================
// Execute Mapper Interface (Command Execute)
// =============================================================================

/**
 * Input object for command execute mapper scripts.
 *
 * Available as global variable: `sbmdCommandArgs`
 *
 * @example
 * // Command with no arguments
 * return { output: null };
 *
 * @example
 * // Lock/Unlock with optional PIN
 * var result = null;
 * if (((sbmdCommandArgs.featureMap & 0x81) === 0x81) &&
 *     sbmdCommandArgs.input.length > 0) {
 *     // Convert PIN string to byte array
 *     result = [];
 *     for (let i = 0; i < sbmdCommandArgs.input[0].length; i++) {
 *         result.push(sbmdCommandArgs.input[0].charCodeAt(i));
 *     }
 * }
 * return { output: result };
 *
 * @example
 * // Command with timeout argument
 * const secs = parseInt(sbmdCommandArgs.input[0], 10);
 * return { output: secs };
 */
interface SbmdCommandArgs extends SbmdBaseContext {
    /** Array of Barton argument strings */
    input: string[];

    /** Matter command ID */
    commandId: number;

    /** Command name from the SBMD spec */
    commandName: string;
}

/**
 * Output object for command execute mapper scripts.
 *
 * @example
 * return { output: null };           // No arguments
 * return { output: 30 };             // Single argument
 * return { output: [0x31, 0x32] };   // Byte array
 * return { output: { Level: 127, TransitionTime: 10 } }; // Named args
 */
interface SbmdCommandResult {
    /**
     * Command arguments to encode as TLV.
     * Can be:
     * - null: Command with no arguments
     * - single value: Command with one argument
     * - object: Command with multiple named arguments
     * - array of numbers: For octet string arguments (byte array)
     */
    output: any;
}

// =============================================================================
// Execute Response Mapper Interface
// =============================================================================

/**
 * Input object for command response mapper scripts.
 *
 * Used for commands that return response data.
 *
 * Available as global variable: `sbmdCommandResponseArgs`
 *
 * @example
 * // Return response value directly
 * return { output: sbmdCommandResponseArgs.input };
 *
 * @example
 * // Convert struct response to JSON string
 * return { output: JSON.stringify(sbmdCommandResponseArgs.input) };
 */
interface SbmdCommandResponseArgs extends SbmdBaseContext {
    /**
     * Command response converted from Matter TLV to JSON.
     * Type depends on the command response definition.
     */
    input: any;

    /** Matter command ID */
    commandId: number;

    /** Command name from the SBMD spec */
    commandName: string;
}

/**
 * Output object for command response mapper scripts.
 *
 * @example
 * return { output: "success" };
 * return { output: JSON.stringify(result) };
 */
interface SbmdCommandResponseResult {
    /**
     * Response value for Barton.
     * Will be converted to a string.
     */
    output: string | number | boolean;
}

// =============================================================================
// Global Variable Declarations
// =============================================================================

/**
 * Global variables available to SBMD scripts.
 * The specific variable depends on the script type.
 */
declare var sbmdReadArgs: SbmdReadArgs;
declare var sbmdWriteArgs: SbmdWriteArgs | SbmdWriteCommandArgs;
declare var sbmdCommandArgs: SbmdCommandArgs;
declare var sbmdCommandResponseArgs: SbmdCommandResponseArgs;
