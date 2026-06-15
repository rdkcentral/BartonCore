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
 * @see docs/SBMD.md for detailed documentation
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

    /**
     * Cluster feature maps keyed by cluster ID (as string).
     * Use to check cluster capabilities before encoding.
     * Clusters listed in matterMeta.featureClusters are available here.
     */
    clusterFeatureMaps: Record<string, number>;

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
 * var val = SbmdUtils.Tlv.decode(sbmdReadArgs.tlvBase64);
 * return SbmdUtils.Response.value(val ? 'true' : 'false');
 *
 * @example
 * // Enum to boolean conversion (Door Lock state)
 * // LockState enum: 0=NotFullyLocked, 1=Locked, 2=Unlocked, 3=Unlatched
 * var lockState = SbmdUtils.Tlv.decode(sbmdReadArgs.tlvBase64);
 * return SbmdUtils.Response.value(lockState === 1 ? 'true' : 'false');
 *
 * @example
 * // Percentage conversion (Level Control 0-254 to 0-100)
 * var level = SbmdUtils.Tlv.decode(sbmdReadArgs.tlvBase64);
 * var percent = Math.round(level / 254 * 100);
 * return SbmdUtils.Response.value(percent.toString());
 */
interface SbmdReadArgs extends SbmdBaseContext {
    /** Base64-encoded TLV data from Matter attribute */
    tlvBase64: string;

    /** Matter cluster ID */
    clusterId: number;

    /** Matter attribute ID */
    attributeId: number;

    /** Attribute name from the SBMD spec */
    attributeName: string;

    /** Matter attribute type (e.g., "bool", "uint8", "enum8") */
    attributeType: string;
}

/**
 * Output object for read mapper scripts (legacy format).
 *
 * Return one of: SbmdReadResult, SbmdErrorResult, or {} (suppress).
 *
 * @example
 * return SbmdUtils.Response.value('true');
 * return SbmdUtils.Response.value(50);  // Numbers are converted to strings
 * return {};  // suppress — skip the resource update
 */
interface SbmdReadResult {
    /**
     * Value for the Barton resource.
     * Will be converted to a string for the resource value.
     */
    value: string | number | boolean;
}

// =============================================================================
// Write Mapper Interface
// =============================================================================

/**
 * Input object for write mapper scripts.
 *
 * Write mappers are script-only. The script determines the full Matter operation
 * and returns either a `write` (attribute) or `invoke` (command) result with
 * pre-encoded TLV.
 *
 * Available as global variable: `sbmdWriteArgs`
 *
 * @example
 * // Attribute write - encode value as TLV
 * const secs = parseInt(sbmdWriteArgs.input, 10);
 * const tlvBase64 = SbmdUtils.Tlv.encode(secs, 'uint16');
 * return SbmdUtils.Response.write(3, 0, tlvBase64);
 *
 * @example
 * // Command invocation - On/Off
 * const isOn = sbmdWriteArgs.input === 'true';
 * return SbmdUtils.Response.invoke(6, isOn ? 1 : 0);
 */
interface SbmdWriteArgs {
    /** Barton resource string value to write */
    input: string;

    /** Device UUID */
    deviceUuid: string;

    /** Barton endpoint ID */
    endpointId: string;

    /** Barton resource ID */
    resourceId: string;

    /**
     * Cluster feature maps keyed by cluster ID (as string).
     * Use to check cluster capabilities before encoding.
     */
    clusterFeatureMaps: Record<string, number>;
}

/**
 * Output object for write mapper scripts.
 *
 * Must return either an `invoke` or `write` operation with pre-encoded TLV.
 * Use `SbmdUtils.Response.write()` or `SbmdUtils.Response.invoke()` helpers.
 *
 * @example
 * // Attribute write
 * return { write: { clusterId: 3, attributeId: 0, tlvBase64: "..." } };
 *
 * @example
 * // Command invocation
 * return { invoke: { clusterId: 6, commandId: 1 } };
 */
interface SbmdWriteResult {
    write?: {
        clusterId: number;
        attributeId: number;
        tlvBase64: string;
        endpointId?: string;
    };
    invoke?: {
        clusterId: number;
        commandId: number;
        tlvBase64?: string;
        endpointId?: string;
        timedInvokeTimeoutMs?: number;
    };
}

// =============================================================================
// Execute Mapper Interface (Command Execute)
// =============================================================================

/**
 * Input object for command execute mapper scripts.
 *
 * Execute mappers are script-only. The script determines the full Matter command
 * to invoke and returns an `invoke` result with pre-encoded TLV.
 *
 * Available as global variable: `sbmdCommandArgs`
 *
 * @example
 * // No-argument command (Toggle)
 * return SbmdUtils.Response.invoke(6, 2);
 *
 * @example
 * // Lock with optional PIN using clusterFeatureMaps
 * const featureMap = sbmdCommandArgs.clusterFeatureMaps['257'] || 0;
 * var args = { PINCode: null };
 * if (((featureMap & 0x81) === 0x81) && sbmdCommandArgs.input.length > 0) {
 *     var pinBytes = [];
 *     for (let i = 0; i < sbmdCommandArgs.input.length; i++) {
 *         pinBytes.push(sbmdCommandArgs.input.charCodeAt(i));
 *     }
 *     args.PINCode = pinBytes;
 * }
 * const tlvBase64 = SbmdUtils.Tlv.encodeStruct(args, {PINCode: {tag: 0, type: 'octstr'}});
 * return SbmdUtils.Response.invoke(257, 0, tlvBase64, {timedInvokeTimeoutMs: 10000});
 */
interface SbmdCommandArgs {
    /** Barton argument string */
    input: string;

    /** Device UUID */
    deviceUuid: string;

    /** Barton endpoint ID */
    endpointId: string;

    /** Barton resource ID */
    resourceId: string;

    /**
     * Cluster feature maps keyed by cluster ID (as string).
     * Use to check cluster capabilities before encoding.
     */
    clusterFeatureMaps: Record<string, number>;
}

/**
 * Output object for command execute mapper scripts.
 *
 * Must return an `invoke` operation with pre-encoded TLV.
 * Use `SbmdUtils.Response.invoke()` helper.
 *
 * @example
 * return { invoke: { clusterId: 6, commandId: 2 } };
 *
 * @example
 * return { invoke: { clusterId: 257, commandId: 0, tlvBase64: "...", timedInvokeTimeoutMs: 10000 } };
 */
interface SbmdCommandResult {
    invoke: {
        clusterId: number;
        commandId: number;
        tlvBase64?: string;
        endpointId?: string;
        timedInvokeTimeoutMs?: number;
    };
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
 * // Decode TLV response and return a field
 * var resp = SbmdUtils.Tlv.decode(sbmdCommandResponseArgs.tlvBase64);
 * return SbmdUtils.Response.value(resp.userName || "");
 *
 * @example
 * // Return decoded response as JSON string
 * var resp = SbmdUtils.Tlv.decode(sbmdCommandResponseArgs.tlvBase64);
 * return SbmdUtils.Response.value(JSON.stringify(resp));
 */
interface SbmdCommandResponseArgs extends SbmdBaseContext {
    /** Base64-encoded TLV response data */
    tlvBase64: string;

    /** Matter cluster ID */
    clusterId: number;

    /** Matter command ID */
    commandId: number;

    /** Command name from the SBMD spec */
    commandName: string;
}

/**
 * Output object for command response mapper scripts (legacy format).
 *
 * Return one of: SbmdCommandResponseResult, SbmdErrorResult, or {} (suppress).
 *
 * @example
 * return SbmdUtils.Response.value("success");
 * return SbmdUtils.Response.value(JSON.stringify(result));
 */
interface SbmdCommandResponseResult {
    /**
     * Response value for Barton.
     * Will be converted to a string.
     */
    value: string | number | boolean;
}

// =============================================================================
// Event Mapper Interface
// =============================================================================

/**
 * Input object for event mapper scripts.
 *
 * Event mappers process Matter device events (e.g., LockOperation)
 * and produce a resource value.
 *
 * Available as global variable: `sbmdEventArgs`
 *
 * @example
 * // DoorLock LockOperation event
 * var event = SbmdUtils.Tlv.decode(sbmdEventArgs.tlvBase64);
 * var opType = event[0]; // LockOperationType at context tag 0
 * if (opType === 0) return SbmdUtils.Response.value('true');  // Lock
 * if (opType === 1) return SbmdUtils.Response.value('false'); // Unlock
 * return {};  // suppress other operation types
 */
interface SbmdEventArgs extends SbmdBaseContext {
    /** Base64-encoded TLV data from Matter event */
    tlvBase64: string;

    /** Matter cluster ID */
    clusterId: number;

    /** Matter event ID */
    eventId: number;

    /** Event name from the SBMD spec */
    eventName: string;
}

/**
 * Output object for event mapper scripts (legacy format).
 *
 * Return one of: SbmdEventResult, SbmdErrorResult, or {} (suppress).
 *
 * @example
 * return SbmdUtils.Response.value("true");
 * return {};  // suppress — skip the resource update
 */
interface SbmdEventResult {
    /**
     * Value for the Barton resource.
     * Will be converted to a string for the resource value.
     */
    value: string | number | boolean;
}

/**
 * Error result returned by any SBMD mapper script.
 *
 * The engine logs the error message and skips the resource update.
 * Use SbmdUtils.Response.error() to construct.
 *
 * @example
 * return SbmdUtils.Response.error('Unexpected lock state: ' + state);
 */
interface SbmdErrorResult {
    error: string;
}

// =============================================================================
// Global Variable Declarations
// =============================================================================

/**
 * Global variables available to SBMD scripts.
 * The specific variable depends on the script type.
 */
declare var sbmdReadArgs: SbmdReadArgs;
declare var sbmdWriteArgs: SbmdWriteArgs;
declare var sbmdCommandArgs: SbmdCommandArgs;
declare var sbmdCommandResponseArgs: SbmdCommandResponseArgs;
declare var sbmdEventArgs: SbmdEventArgs;
