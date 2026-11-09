## MODIFIED Requirements

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
