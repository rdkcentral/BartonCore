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
The runtime SHALL support activating a driver (re-reading its `.sbmd.js` file from disk, re-evaluating it, and GC-rooting handler JSValues) and deactivating a driver (releasing GC roots so handler objects are eligible for collection). Metadata required for claiming — device types, vendor/product IDs, device class, name, and the spec file path — SHALL remain available regardless of activation state.

A driver SHALL be loaded in the inactive state. While inactive, the driver SHALL retain only its claim metadata (the claiming metadata above) and SHALL NOT retain the spec source text or the heavy parsed registration (endpoint, resource, alias, and handler collections). Deactivation SHALL release the spec source text and the heavy parsed registration in addition to releasing handler GC roots, returning the driver to the claim-metadata state. Because an inactive driver does not retain the source text, activation SHALL obtain the spec content by re-reading the spec file from disk at `filePath`; a driver that still holds its source (i.e. has not yet been reduced to its claim metadata) MAY re-evaluate that retained copy instead. In the production flow the factory reduces every driver to its claim metadata immediately after registration, so activation always re-reads from disk.

A driver SHALL remain activated for as long as at least one device is bound to it, and SHALL be deactivated only when its last bound device is removed. Before deactivation, any outstanding deferred operations SHALL be settled (completed as failures) so their handler references are released and no late response can dispatch against an inactive driver.

#### Scenario: Inactive driver used for claiming
- **WHEN** a new device is commissioned and its device type matches an inactive driver's `matter.deviceTypes`
- **THEN** the driver is claimed using its claim metadata alone, and is activated (spec file re-read from disk, re-evaluated, handlers rooted) when the device is bound to it

#### Scenario: Driver deactivated when last device removed
- **WHEN** the last device bound to a driver is removed
- **THEN** the driver is deactivated, its handler GC roots are released, and its spec source text and heavy parsed registration are released back to the claim-metadata state

#### Scenario: Driver stays active while any device remains
- **WHEN** a device bound to a driver is removed but at least one other device remains bound to the same driver
- **THEN** the driver remains activated

#### Scenario: Outstanding deferred operations settled before deactivation
- **WHEN** the last device bound to a driver is removed while the driver has outstanding deferred operations
- **THEN** those deferred operations are completed as failures and their handler references released before the driver is deactivated

#### Scenario: Metadata available while inactive
- **WHEN** a driver is inactive
- **THEN** its device types, vendor/product IDs, device class, name, and spec file path remain accessible for claiming decisions

#### Scenario: Inactive driver retains only its claim metadata
- **WHEN** a driver has been loaded but not yet claimed by any device, or has been deactivated after its last device was removed
- **THEN** the driver does not retain the spec source text or the heavy parsed registration (endpoints, resources, aliases, handler references)

#### Scenario: Activation re-reads the spec file from disk
- **WHEN** an inactive driver is activated
- **THEN** the runtime reads the spec file from `filePath`, re-evaluates it, and rebuilds the parsed registration and dispatch tables

#### Scenario: Activation re-reads the same file regardless of working directory
- **WHEN** a spec was loaded at startup through a relative `sbmd_dirs` entry and the process's working directory has changed before a device binds
- **THEN** activation re-reads the file the factory loaded, because the spec's path was recorded as absolute at load

#### Scenario: Activation rejects a spec that no longer matches its claim metadata
- **WHEN** an inactive driver is activated and the re-read spec's claim-identity fields (name, device types, vendor/product IDs, device class) no longer match the claim metadata, or the spec file cannot be read (including when it is not a regular file) or parsed, or loading it does not finish within the SBMD script deadline
- **THEN** activation fails, the failure is logged, and the device bind is aborted rather than dispatching against a mismatched or partially built driver

#### Scenario: Activation rejects a spec whose device-class or profile version changed
- **WHEN** a device binds and the re-read spec's `barton.deviceClassVersion`, an endpoint `profileVersion`, or the set of endpoint profiles differs from what the driver was registered with at load (a profile added, removed, or a version changed)
- **THEN** the bind is rejected and the driver is not activated, because commissioning and reconfiguration publish/compare the versions cached at registration while the re-read handlers would run the changed spec

#### Scenario: Endpoints sharing a profile compare against the cached version
- **WHEN** a spec declares several endpoints with the same profile but different `profileVersion` values, so registration caches only the first such endpoint's version, and a device binds without the spec changing
- **THEN** activation compares that profile against the version of the first endpoint declaring it, exactly as cached, and the bind is accepted

#### Scenario: Loading rejects a spec with an invalid numeric claim/version field
- **WHEN** a spec is loaded (at startup or re-read on activation) whose `matter.deviceTypes` entries or `matter.vendorId`/`matter.productId` are not integers in 0–`0xFFFF`, or whose `barton.deviceClassVersion` or an endpoint `profileVersion` is not an integer in 0–255 (out of range, fractional, `NaN`/infinite, or not a number at all, such as a string, boolean, or `null`; a non-array `deviceTypes` is likewise invalid, and only an absent optional field means "not set")
- **THEN** the spec fails to load rather than having the value wrapped, narrowed, truncated, or coerced into one that could alias a claim identity or version; at startup only that driver is skipped (not registered), and on re-read the activation fails and the bind is aborted

#### Scenario: Last-device deactivation deferred while a bind is in flight
- **WHEN** a driver's last device is removed at the same time a new matching device is being commissioned, so the removal's empty-map observation races the in-progress bind that has already activated the driver but not yet inserted its device
- **THEN** deactivation is skipped while any bind is in flight or the device map is non-empty when rechecked, so the concurrent bind is never left pointing at a driver whose runtime state was shed

#### Scenario: A re-read spec is validated before it is committed
- **WHEN** a bind activates the driver and validates the re-read spec's cached versions, while another bind concurrently observes the driver as already activated
- **THEN** the re-read spec is validated before it is committed, so the driver never becomes active with an unvalidated registration and a concurrent binder only ever adopts a validated one

#### Scenario: Lifecycle transitions never overlap report dispatch
- **WHEN** a driver is activated or deactivated while the Matter event loop may be dispatching an attribute report, event, or incoming command into it
- **THEN** the step that replaces or frees the registration and dispatch tables holds the Matter stack lock (or runs on the Matter event loop thread itself), which report dispatch also holds, so dispatch never reads a registration or dispatch table that is being replaced or freed; reading and evaluating the spec happens before the lock is taken, and when the Matter event loop thread does not exist (before start, or after stop has joined it) the step runs directly

#### Scenario: A rejected bind never wires the device to the driver
- **WHEN** a bind fails after activation (for example the endpoint map cannot be resolved or a required prerequisite is unmet)
- **THEN** the device's attribute, event, and command callbacks and incoming command handlers were never installed, so nothing from that device can dispatch into the driver; and the device detaches from its data cache under the Matter stack lock when it is destroyed, so no cache callback can still be running into it

#### Scenario: Teardown after the JS runtime has shut down
- **WHEN** a driver is deactivated after the JS runtime has been shut down and its context freed (for example a bind that fails during Matter shutdown)
- **THEN** the handler references are detached without touching the freed context, and activation is refused because no context is available

#### Scenario: A failed or rolled-back bind never sheds a concurrent bind's state
- **WHEN** one bind activates the driver and then fails (e.g. a prerequisite is unmet) while another concurrent bind has already adopted that activation and bound its device
- **THEN** the failing bind's rollback deactivates only when the driver is idle (no bind in flight and no device bound), so it cannot shed the live state the other bind depends on; and if the last device was removed while a bind was racing, the teardown is retried when that bind finishes idle rather than leaving the driver active forever

#### Scenario: Deactivate then reactivate round trip
- **WHEN** a driver is activated, then deactivated after its last device is removed, then a new matching device is later commissioned
- **THEN** the driver is activated again from disk and dispatches to its handlers correctly

### Requirement: Alias resolution
Aliases declared in the `aliases` section SHALL be resolved to cluster+ID pairs at driver activation time. Resources, supplements, and handler registrations that reference aliases by name SHALL use the resolved IDs for dispatch and cache lookups.

#### Scenario: Attribute alias resolved for supplement
- **WHEN** a handler declares `supplements: { attributes: ["onOff"] }` and `onOff` is an alias with `clusterId: 0x0006, attributeId: 0x0000`
- **THEN** the runtime reads from cluster 0x0006, attribute 0x0000 in the device data cache and delivers the value as `args.supplements.attributes.onOff`

#### Scenario: Event alias prerequisite check
- **WHEN** a resource has `prerequisites: ["lockOperation"]` and `lockOperation` is an event alias with `clusterId: 0x0101`
- **THEN** the prerequisite is satisfied if cluster 0x0101 is present in the device's data cache

### Requirement: volatile resource mode disables value caching

The SBMD runtime SHALL support a resource mode `volatile`. A resource declared with the `volatile` mode SHALL be registered with `CACHING_POLICY_NEVER`, causing `updateResource` to emit a `resourceUpdated` event on every call (for a resource that emits events) regardless of whether the new value equals the currently stored value. The `volatile` mode SHALL NOT, by itself, add read, write, or execute access, and SHALL be accepted by mode-to-bitmask conversion without error. A resource is registered `CACHING_POLICY_NEVER` when it declares a read handler OR declares the `volatile` mode; otherwise it is registered `CACHING_POLICY_ALWAYS`.

#### Scenario: Volatile resource emits on unchanged value
- **WHEN** a resource declared with `modes: ['volatile']` is updated twice with the same value
- **THEN** the runtime SHALL deliver two `resourceUpdated` events (no value-change suppression)

#### Scenario: Volatile mode is accepted by the schema and runtime
- **WHEN** a driver declares a resource with `volatile` in its modes array
- **THEN** the spec SHALL validate against the SBMD schema AND the resource SHALL register successfully without adding read, write, or execute modes

#### Scenario: Non-volatile resource without a read handler remains cached
- **WHEN** an event-only resource is declared without a read handler and without the `volatile` mode
- **THEN** the runtime SHALL register it with `CACHING_POLICY_ALWAYS` and suppress `resourceUpdated` events whose value is unchanged
