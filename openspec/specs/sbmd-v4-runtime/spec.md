## ADDED Requirements

### Requirement: Two-pass file evaluation with constants injection
The runtime SHALL evaluate `.sbmd.js` files using a two-pass process. Pass 1 SHALL extract the `constants:` block from the source text by brace-matching, evaluate it as a JavaScript object literal, and produce a set of name→primitive-value pairs. Pass 2 SHALL prepend `var` declarations for each constant, wrap the entire file in an IIFE, and evaluate the result using `JS_EVAL_REPL`.

#### Scenario: Constants are available in SbmdDriver registration
- **WHEN** a `.sbmd.js` file declares `constants: { CL_ON_OFF: 0x0006 }` and references `CL_ON_OFF` in its `aliases` section
- **THEN** the runtime resolves `CL_ON_OFF` to `6` during evaluation and the alias `clusterId` is correctly set

#### Scenario: IIFE wrapping prevents cross-driver namespace pollution
- **WHEN** two `.sbmd.js` files both define a function named `readIsOn`
- **THEN** each file's function is scoped to its own IIFE and no name collision occurs

#### Scenario: Constants block contains only primitives
- **WHEN** a `constants:` block contains a non-primitive value (object, array, function)
- **THEN** the runtime SHALL reject the file with an error

### Requirement: SbmdDriver capture function
The runtime SHALL inject a global `SbmdDriver` JavaScript function that captures the registration object into `__sbmd_registration`. After file evaluation, the runtime SHALL read `__sbmd_registration` via `JS_GetPropertyStr`, extract the registration data, and reset the variable to null.

#### Scenario: Single SbmdDriver call per file
- **WHEN** a `.sbmd.js` file calls `SbmdDriver({...})` exactly once
- **THEN** the runtime extracts the registration object successfully

#### Scenario: Multiple SbmdDriver calls rejected
- **WHEN** a `.sbmd.js` file calls `SbmdDriver()` more than once
- **THEN** the JS engine throws an error and the file is rejected

### Requirement: Registration object extraction
The runtime SHALL extract the following from the `SbmdDriver({...})` registration object by walking JSValue properties directly (no JSON serialization): `schemaVersion`, `driverVersion`, `name`, `constants`, `aliases`, `barton`, `matter`, `reporting`, `resources`, `endpoints`, `attributeHandlers`, `eventHandlers`, `commandHandlers`. Handler function JSValues SHALL be stored for later invocation.

#### Scenario: Metadata extracted to C++ structs
- **WHEN** a registration object contains `barton: { deviceClass: "light", deviceClassVersion: 0 }`
- **THEN** the runtime extracts `deviceClass = "light"` and `deviceClassVersion = 0` into C++ data structures

#### Scenario: Handler function references preserved
- **WHEN** a resource declares `write: writeIsOn` and `writeIsOn` is a function defined in the file
- **THEN** the runtime stores the JSValue reference to `writeIsOn` for later invocation

### Requirement: Result builder
`Sbmd.result()` SHALL return a mutable builder object that accumulates an ordered list of operations and a terminal. Non-terminal methods SHALL return the builder. Terminal methods (`success`, `error`, `sendCommand`, `writeAttribute`, `requestCommand`, `readAttribute`) SHALL set the terminal and return the raw `{ops, terminal}` result object.

#### Scenario: Linear chain produces correct structure
- **WHEN** a handler returns `Sbmd.result().dataModel.updateResource("1", "isOn", "true").log("updated").success()`
- **THEN** the result contains `ops: [{op: "updateResource", endpoint: "1", resource: "isOn", value: "true"}, {op: "log", message: "updated"}]` and `terminal: {op: "success"}`

#### Scenario: Terminal cuts off further chaining
- **WHEN** a handler calls `.success()` and then attempts to call `.log("after")`
- **THEN** a JavaScript TypeError occurs because the returned raw object has no `log` method

#### Scenario: Operations after terminal via stored builder reference
- **WHEN** a handler stores the builder, calls a terminal, then attempts to add operations via the stored builder reference
- **THEN** the builder throws an error ("Cannot add operations after a terminal")

### Requirement: Handler dispatch for device-initiated messages
The runtime SHALL build dispatch tables at driver activation time from `attributeHandlers`, `eventHandlers`, and `commandHandlers` registrations. Incoming device messages SHALL be matched against these tables. Specific handlers (single ID) SHALL fire before multi-ID handlers, which SHALL fire before wildcard handlers.

#### Scenario: Attribute report dispatched to registered handler
- **WHEN** an attribute report for cluster 0x0006, attribute 0x0000 arrives and an `attributeHandler` is registered with `aliases: ["onOff"]` where `onOff` resolves to that cluster+attribute
- **THEN** the handler function is called with `args.attribute` containing the decoded value

#### Scenario: Wildcard handler fires after specific handlers
- **WHEN** both a specific handler for attribute 0x0000 and a wildcard handler for `attributeId: "*"` on the same cluster are registered, and a report for attribute 0x0000 arrives
- **THEN** the specific handler fires first, then the wildcard handler fires

#### Scenario: No matching handler
- **WHEN** an attribute report arrives for a cluster+attribute with no registered handler
- **THEN** no handler is called and no error is raised

### Requirement: Supplements pre-loading
When a handler declares `supplements`, the runtime SHALL resolve alias names to cluster+attribute IDs, read attribute values from the device data cache, read resource values from the Barton resource store, and deliver them in `args.supplements` before calling the handler.

#### Scenario: Attribute supplement loaded from cache
- **WHEN** a `seed` handler declares `supplements: { attributes: ["onOff"] }` and the device data cache has a value for the `onOff` alias
- **THEN** `args.supplements.attributes.onOff` contains the decoded attribute value

#### Scenario: Resource supplement loaded
- **WHEN** a handler declares `supplements: { resources: ["1/isOn"] }`
- **THEN** `args.supplements.resources["1/isOn"]` contains the current Barton resource value

### Requirement: Resource handler invocation
The runtime SHALL invoke `seed`, `read`, `write`, and `execute` handler functions when Barton resource operations occur. The `args` object SHALL contain `deviceUuid`, `endpointId`, `clusterFeatureMaps`, `resource: { resourceId, input }`, and `supplements` (if declared).

#### Scenario: Seed handler called at device discovery
- **WHEN** a device is first commissioned and a resource has a `seed` handler
- **THEN** the seed handler is called with `args.resource.input` set to `null`

#### Scenario: Seed handler called at startup for paired devices
- **WHEN** the service starts and a previously paired device has resources with `seed` handlers
- **THEN** the seed handlers are called to resynchronize resource values

#### Scenario: Write handler receives input
- **WHEN** a Barton write operation is performed on a resource with value `"true"`
- **THEN** the write handler is called with `args.resource.input` set to `"true"`

### Requirement: Result chain execution
After a handler returns, the runtime SHALL execute all operations in the `ops` array in order, then execute the terminal. The runtime SHALL support the following operation types: `updateResource`, `setMetadata`, `setPersistentData`, `setTransientData`, `log`. Unknown operation types SHALL be logged as warnings and skipped.

#### Scenario: Operations execute in order
- **WHEN** a result contains `[updateResource, log, setPersistentData]` followed by `success`
- **THEN** the resource is updated, the message is logged, the data is persisted, and the operation completes successfully — in that order

#### Scenario: Operations execute even on error terminal
- **WHEN** a result contains `[log("diagnostic")]` followed by `error("failed")`
- **THEN** the log message is emitted, then the operation is marked as failed

### Requirement: Deferred operations
`requestCommand` and `readAttribute` terminals SHALL park the resource operation and register pending response state. When a matching response arrives, the stored handler function SHALL be called with the response data and the original trigger context. The handler's result chain SHALL be executed to complete the parked operation.

#### Scenario: requestCommand parks and completes on response
- **WHEN** a handler returns `requestCommand` with `responseCommandId: 26` and later a command with ID 26 arrives on the matching cluster
- **THEN** the response handler is called, its result executes, and the parked resource operation completes

#### Scenario: Timeout fires onError
- **WHEN** a `requestCommand` specifies `timeoutMs: 5000` and no matching response arrives within 5 seconds
- **THEN** the `onError` handler is called with `args.error.type` set to `"timeout"`

#### Scenario: Deferred handler returns another deferral
- **WHEN** a deferred response handler returns a new `requestCommand`
- **THEN** the pending state is re-armed with the new match criteria, handlers, and timer without creating nested structures

#### Scenario: Overall operation timeout
- **WHEN** a chain of deferrals exceeds the overall operation deadline (`matter.defaultTimeoutMs`)
- **THEN** the `onError` handler of the current hop is called with `type: "timeout"` regardless of per-hop timeouts

#### Scenario: Max deferral depth exceeded
- **WHEN** a chain of deferrals exceeds the maximum deferral depth
- **THEN** the current hop's `onError` handler is called with an appropriate error

### Requirement: Driver lifecycle — activate and deactivate
The runtime SHALL support activating a driver (re-evaluating its `.sbmd.js` file and GC-rooting handler JSValues) and deactivating a driver (releasing GC roots so handler objects are eligible for collection). Metadata extracted to C++ SHALL remain available regardless of activation state.

#### Scenario: Inactive driver used for claiming
- **WHEN** a new device is commissioned and its device type matches an inactive driver's `matter.deviceTypes`
- **THEN** the driver is activated (file re-evaluated, handlers rooted) before the claiming process proceeds

#### Scenario: Driver deactivated when last device removed
- **WHEN** the last device using a driver is removed
- **THEN** the driver is deactivated and its handler GC roots are released

#### Scenario: Metadata available while inactive
- **WHEN** a driver is inactive
- **THEN** its device types, vendor/product IDs, device class, and other C++ metadata remain accessible for claiming decisions

### Requirement: Alias resolution
Aliases declared in the `aliases` section SHALL be resolved to cluster+ID pairs at driver activation time. Resources, supplements, and handler registrations that reference aliases by name SHALL use the resolved IDs for dispatch and cache lookups.

#### Scenario: Attribute alias resolved for supplement
- **WHEN** a handler declares `supplements: { attributes: ["onOff"] }` and `onOff` is an alias with `clusterId: 0x0006, attributeId: 0x0000`
- **THEN** the runtime reads from cluster 0x0006, attribute 0x0000 in the device data cache and delivers the value as `args.supplements.attributes.onOff`

#### Scenario: Event alias prerequisite check
- **WHEN** a resource has `prerequisites: ["lockOperation"]` and `lockOperation` is an event alias with `clusterId: 0x0101`
- **THEN** the prerequisite is satisfied if cluster 0x0101 is present in the device's data cache
