/**
 * SBMD v4 Type Definitions
 *
 * TypeScript type definitions for SBMD (Specification-Based Matter Driver)
 * v4 `.sbmd.js` driver files.
 *
 * Each driver file calls `SbmdDriver({...})` with a registration object.
 * Handler functions receive an `args` object and return a result built
 * with the `Sbmd.result()` builder.
 *
 * @file sbmd-script.d.ts
 * @see docs/SBMD.md for detailed documentation
 */

// =============================================================================
// SbmdDriver Registration Object
// =============================================================================

/**
 * Top-level registration object passed to `SbmdDriver()`.
 */
interface SbmdRegistration {
    /** Schema version. Must be "4.0". */
    schemaVersion: "4.0";

    /** Driver-specific version string or number. */
    driverVersion: string | number;

    /** Human-readable driver name. */
    name: string;

    /** Named constants injected as read-only globals. Values must be primitives. */
    constants: Record<string, number | string | boolean>;

    /** Named references to Matter cluster attributes, events, or commands. */
    aliases?: Record<string, SbmdAlias>;

    /** Barton device class mapping. */
    barton: SbmdBarton;

    /** Matter device type matching. */
    matter: SbmdMatter;

    /** Attribute reporting interval. */
    reporting?: SbmdReporting;

    /** Device-level resource declarations keyed by resource name. */
    resources?: Record<string, SbmdResource>;

    /** Endpoint definitions keyed by endpoint ID string. */
    endpoints?: Record<string, SbmdEndpoint>;

    /** Attribute report handlers keyed by handler name. */
    attributeHandlers?: Record<string, SbmdAttributeHandler>;

    /** Event handlers keyed by handler name. */
    eventHandlers?: Record<string, SbmdEventHandler>;

    /** Unsolicited command handlers keyed by handler name. */
    commandHandlers?: Record<string, SbmdCommandHandler>;
}

// =============================================================================
// Registration Sub-Types
// =============================================================================

/**
 * Alias: a named reference to a Matter cluster element.
 * Must have `clusterId` and at most one of `attributeId`, `eventId`, `commandId`.
 * A cluster-only alias (no ID field) matches all elements on that cluster.
 */
interface SbmdAlias {
    clusterId: number;
    attributeId?: number;
    eventId?: number;
    commandId?: number;
    /** Matter data type (documentation only, ignored by runtime). */
    type?: string;
}

interface SbmdBarton {
    deviceClass: string;
    deviceClassVersion: number;
}

interface SbmdMatter {
    /** Matter device type IDs this driver handles. */
    deviceTypes: number[];
    /** Minimum Matter device type revision required. */
    revision?: number;
    /** Matter vendor ID for vendor-specific matching. */
    vendorId?: number;
    /** Matter product ID for vendor-specific matching. Requires vendorId. */
    productId?: number;
    /** Cluster IDs whose feature maps should be cached. */
    featureClusters?: number[];
    /** Default timeout in ms for deferred operations. */
    defaultTimeoutMs?: number;
}

interface SbmdReporting {
    /** Minimum attribute reporting interval in seconds. */
    minSecs: number;
    /** Maximum attribute reporting interval in seconds. */
    maxSecs: number;
}

interface SbmdEndpoint {
    /** Barton resource profile name. */
    profile: string;
    /** Profile version. */
    profileVersion: number;
    /** Resource declarations keyed by resource name. */
    resources: Record<string, SbmdResource>;
}

// =============================================================================
// Supplements
// =============================================================================

/**
 * Pre-fetched data delivered to the handler in `args.supplements`.
 */
interface SbmdSupplements {
    /** Alias names for Matter attributes to read from device data cache. */
    attributes?: string[];
    /** Barton resource paths: "endpointId/resourceName" or "resourceName". */
    resources?: string[];
    /** Persistent storage keys to fetch. */
    persistentData?: string[];
    /** Transient storage keys to fetch (TTL-based). */
    transientData?: string[];
}

// =============================================================================
// Resources
// =============================================================================

/** A seed or read handler with optional supplements. */
interface SbmdReadOrSeedHandler {
    supplements?: SbmdSupplements;
    handler: SbmdHandlerFunction;
}

/**
 * Resource declaration.
 */
interface SbmdResource {
    /** Resource value type: "boolean", "string", "function", or a custom type. */
    type: string;

    /**
     * Access modes: "read", "write", "dynamic" (default on), "static" (opts out of dynamic),
     * "emitEvents" (default on), "noEvents", "lazySaveNext", "sensitive".
     */
    modes?: Array<"read" | "write" | "dynamic" | "static" | "emitEvents" | "noEvents" | "lazySaveNext" | "sensitive">;

    /** Alias names or cluster IDs that must be present before creating this resource. */
    prerequisites?: Array<string | number>;

    /** If true, silently skip when prerequisites are not met. Default false. */
    optional?: boolean;

    /** Initialization handler (runs on discovery and each startup). */
    seed?: SbmdReadOrSeedHandler | SbmdHandlerFunction;

    /** Read handler (runs on every read request). */
    read?: SbmdReadOrSeedHandler | SbmdHandlerFunction;

    /** Write handler function. */
    write?: SbmdHandlerFunction;

    /** Execute handler function (for type: "function" resources). */
    execute?: SbmdHandlerFunction;
}

// =============================================================================
// Attribute / Event / Command Handlers
// =============================================================================

interface SbmdAttributeHandler {
    /** Alias names to match. Mutually exclusive with clusterId. */
    aliases?: string[];
    /** Cluster to match. Mutually exclusive with aliases. */
    clusterId?: number;
    /** Single attribute ID or "*" wildcard. */
    attributeId?: number | "*";
    /** Multiple attribute IDs. */
    attributeIds?: number[];
    supplements?: SbmdSupplements;
    handler: SbmdHandlerFunction;
}

interface SbmdEventHandler {
    aliases?: string[];
    clusterId?: number;
    eventId?: number | "*";
    eventIds?: number[];
    supplements?: SbmdSupplements;
    handler: SbmdHandlerFunction;
}

interface SbmdCommandHandler {
    aliases?: string[];
    clusterId?: number;
    commandId?: number | "*";
    commandIds?: number[];
    supplements?: SbmdSupplements;
    handler: SbmdHandlerFunction;
}

// =============================================================================
// Handler Arguments
// =============================================================================

/** Handler function signature. All handlers receive `args` and return a result. */
type SbmdHandlerFunction = (args: SbmdHandlerArgs) => SbmdResultTerminal;

/** Common fields present on all handler args. */
interface SbmdHandlerArgsBase {
    /** The Barton device UUID. */
    deviceUuid: string;

    /** Barton endpoint ID, or null for device-level resources. */
    endpointId: string | null;

    /** Feature maps for clusters declared in matter.featureClusters. */
    clusterFeatureMaps: Record<string, number>;

    /** Pre-fetched supplement data, present when supplements are declared. */
    supplements?: {
        attributes?: Record<string, any>;
        resources?: Record<string, string | null>;
        persistentData?: Record<string, string | null>;
        transientData?: Record<string, string | null>;
    };

    /** Arbitrary context from a requestCommand/readAttribute call. */
    handlerContext?: any;

    /** Error details, present only on onError handlers. */
    error?: {
        message: string;
        type: "timeout" | "transport" | "internal";
        matterCode: number | null;
    };
}

/** Attribute trigger (present on attribute handlers and readAttribute response handlers). */
interface SbmdAttributeTrigger {
    clusterId: number;
    attributeId: number;
    /** Decoded attribute value. */
    value: any;
    /** Base64-encoded TLV data. */
    tlvBase64: string;
    /** Alias name if registered via aliases, otherwise null. */
    alias: string | null;
}

/** Event trigger (present on event handlers). */
interface SbmdEventTrigger {
    clusterId: number;
    eventId: number;
    /** Decoded event payload (array of TLV field values). */
    data: any[];
    /** Base64-encoded TLV data. */
    tlvBase64: string;
    alias: string | null;
}

/** Command trigger (present on unsolicited command handlers). */
interface SbmdCommandTrigger {
    clusterId: number;
    commandId: number;
    /** Decoded command payload. */
    data: any;
    /** Base64-encoded TLV data. */
    tlvBase64: string;
    alias: string | null;
}

/** Response trigger (present on requestCommand response handlers). */
interface SbmdResponseTrigger {
    clusterId: number;
    commandId: number;
    /** Base64-encoded TLV response data, or null. */
    data: string | null;
}

/** Resource trigger (present on read/write/execute/seed handlers). */
interface SbmdResourceTrigger {
    resourceId: string;
    /** Write value or execute argument (string), null for reads/seeds. */
    input: string | null;
}

/** Union handler args — exactly one trigger field is present depending on context. */
interface SbmdHandlerArgs extends SbmdHandlerArgsBase {
    attribute?: SbmdAttributeTrigger;
    event?: SbmdEventTrigger;
    command?: SbmdCommandTrigger;
    response?: SbmdResponseTrigger;
    resource?: SbmdResourceTrigger;
}

// =============================================================================
// Result Builder — Sbmd.result()
// =============================================================================

/** Terminal result returned by `.success()`, `.error()`, `.device.sendCommand()`, etc. */
interface SbmdResultTerminal {
    ops: any[];
    terminal: any;
}

/**
 * Result builder. Returned by `Sbmd.result()`.
 * Non-terminal methods return the builder; terminal methods return `SbmdResultTerminal`.
 */
interface SbmdResultBuilder {
    /** Emit a diagnostic log message. */
    log(message: string): SbmdResultBuilder;

    /** Mark operation as successful. Optional value for execute response. */
    success(value?: string): SbmdResultTerminal;

    /** Mark operation as failed. */
    error(message: string): SbmdResultTerminal;

    /** Barton device data model operations. */
    dataModel: {
        /** Update a device-level resource. */
        updateResource(resource: string, value: string): SbmdResultBuilder;
        /** Update an endpoint-level resource. */
        updateResource(endpoint: string, resource: string, value: string, metadata?: any): SbmdResultBuilder;
        /** Set device metadata. */
        setMetadata(name: string, value: string): SbmdResultBuilder;
    };

    /** Persistent and transient storage operations. */
    storage: {
        /** Store a key-value pair in non-volatile storage. */
        setPersistentData(key: string, value: string): SbmdResultBuilder;
        /** Store a key-value pair in memory with TTL-based expiry. */
        setTransientData(key: string, value: string, ttlSecs: number): SbmdResultBuilder;
    };

    /** Matter device interaction operations. */
    device: {
        /** Terminal: send a Matter command. */
        sendCommand(
            clusterId: number,
            commandId: number,
            tlvBase64?: string | null,
            options?: { timedInvokeTimeoutMs?: number; successValue?: string },
        ): SbmdResultTerminal;

        /** Terminal: write a Matter attribute. */
        writeAttribute(
            clusterId: number,
            attributeId: number,
            tlvBase64: string,
            options?: { endpointId?: number },
        ): SbmdResultTerminal;

        /** Deferred: send command and wait for response. */
        requestCommand(
            clusterId: number,
            commandId: number,
            payload: string | null,
            options: {
                responseCommandId: number;
                onResponse: SbmdHandlerFunction;
                onError: SbmdHandlerFunction;
                context?: any;
                timeoutMs?: number;
                timedInvokeTimeoutMs?: number;
            },
        ): SbmdResultTerminal;

        /** Deferred: read attribute and wait for response. */
        readAttribute(
            clusterId: number,
            attributeId: number,
            options: {
                onResponse: SbmdHandlerFunction;
                onError: SbmdHandlerFunction;
                context?: any;
                timeoutMs?: number;
            },
        ): SbmdResultTerminal;
    };
}

// =============================================================================
// TLV Utilities — Sbmd.Tlv
// =============================================================================

interface SbmdTlv {
    /** Encode a JS object into base64-encoded Matter TLV struct. */
    encodeStruct(
        fields: Record<string, any>,
        schema: Record<string, { tag: number; type: string }>,
    ): string;

    /** Encode a single primitive value into base64-encoded TLV. */
    encode(value: any, type: string, base?: number): string | null;

    /** Decode base64-encoded TLV to a JS value. */
    decode(tlvBase64: string): any;

    /** Create a base64-encoded empty TLV struct. */
    emptyStruct(): string;
}

// =============================================================================
// Base64 Utilities — Sbmd.Base64
// =============================================================================

interface SbmdBase64 {
    /** Encode byte array to base64 string. */
    encode(bytes: number[] | Uint8Array): string;
    /** Decode base64 string to byte array. */
    decode(base64: string): number[];
}

// =============================================================================
// Sbmd Namespace
// =============================================================================

interface SbmdNamespace {
    /** Create a new result builder. */
    result(): SbmdResultBuilder;
    /** TLV encoding/decoding utilities. */
    Tlv: SbmdTlv;
    /** Base64 encoding/decoding utilities. */
    Base64: SbmdBase64;
}

// =============================================================================
// Global Declarations
// =============================================================================

/** Register an SBMD driver with the runtime. */
declare function SbmdDriver(registration: SbmdRegistration): void;

/** SBMD runtime namespace. */
declare var Sbmd: SbmdNamespace;

