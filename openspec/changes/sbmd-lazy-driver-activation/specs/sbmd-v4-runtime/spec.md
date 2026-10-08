## MODIFIED Requirements

### Requirement: Driver lifecycle — activate and deactivate
The runtime SHALL support activating a driver (re-reading its `.sbmd.js` file from disk, re-evaluating it, and GC-rooting handler JSValues) and deactivating a driver (releasing GC roots so handler objects are eligible for collection). Metadata required for claiming — device types, vendor/product IDs, device class, name, and the spec file path — SHALL remain available regardless of activation state.

A driver SHALL be loaded in the inactive state. While inactive, the driver SHALL retain only a claim stub (the claiming metadata above) and SHALL NOT retain the spec source text or the heavy parsed registration (endpoint, resource, alias, and handler collections). Deactivation SHALL release the spec source text and the heavy parsed registration in addition to releasing handler GC roots, returning the driver to the claim-stub state. Because an inactive driver does not retain the source text, activation SHALL obtain the spec content by re-reading the spec file from disk at `filePath`; a driver that still holds its source (i.e. has not yet been shrunk) MAY re-evaluate that retained copy instead. In the production flow the factory shrinks every driver immediately after registration, so activation always re-reads from disk.

A driver SHALL remain activated for as long as at least one device is bound to it, and SHALL be deactivated only when its last bound device is removed. Before deactivation, any outstanding deferred operations SHALL be settled (completed as failures) so their handler references are released and no late response can dispatch against an inactive driver.

#### Scenario: Inactive driver used for claiming
- **WHEN** a new device is commissioned and its device type matches an inactive driver's `matter.deviceTypes`
- **THEN** the driver is activated (spec file re-read from disk, re-evaluated, handlers rooted) before the claiming process proceeds

#### Scenario: Driver deactivated when last device removed
- **WHEN** the last device bound to a driver is removed
- **THEN** the driver is deactivated, its handler GC roots are released, and its spec source text and heavy parsed registration are released back to the claim-stub state

#### Scenario: Driver stays active while any device remains
- **WHEN** a device bound to a driver is removed but at least one other device remains bound to the same driver
- **THEN** the driver remains activated

#### Scenario: Outstanding deferred operations settled before deactivation
- **WHEN** the last device bound to a driver is removed while the driver has outstanding deferred operations
- **THEN** those deferred operations are completed as failures and their handler references released before the driver is deactivated

#### Scenario: Metadata available while inactive
- **WHEN** a driver is inactive
- **THEN** its device types, vendor/product IDs, device class, name, and spec file path remain accessible for claiming decisions

#### Scenario: Inactive driver retains only the claim stub
- **WHEN** a driver has been loaded but not yet claimed by any device, or has been deactivated after its last device was removed
- **THEN** the driver does not retain the spec source text or the heavy parsed registration (endpoints, resources, aliases, handler references)

#### Scenario: Activation re-reads the spec file from disk
- **WHEN** an inactive driver is activated
- **THEN** the runtime reads the spec file from `filePath`, re-evaluates it, and rebuilds the parsed registration and dispatch tables

#### Scenario: Activation rejects a spec that no longer matches its claim stub
- **WHEN** an inactive driver is activated and the re-read spec's claim-identity fields (name, device types, vendor/product IDs, device class) no longer match the claim stub, or the spec file cannot be read or parsed
- **THEN** activation fails, the failure is logged, and the device bind is aborted rather than dispatching against a mismatched or partially built driver

#### Scenario: Activation rejects a spec whose device-class or profile version changed
- **WHEN** a device binds and the re-read spec's `barton.deviceClassVersion`, an endpoint `profileVersion`, or the set of endpoint profiles differs from what the driver was registered with at load (a profile added, removed, a version changed, or a `profileVersion` outside the `uint8` range)
- **THEN** the bind is rejected and the driver is deactivated, because commissioning and reconfiguration publish/compare the versions cached at registration while the re-read handlers would run the changed spec

#### Scenario: Last-device deactivation deferred while a bind is in flight
- **WHEN** a driver's last device is removed at the same time a new matching device is being commissioned, so the removal's empty-map observation races the in-progress bind that has already activated the driver but not yet inserted its device
- **THEN** deactivation is skipped while any bind is in flight or the device map is non-empty when rechecked, so the concurrent bind is never left pointing at a driver whose runtime state was shed

#### Scenario: Activation and version validation are one serialized transition
- **WHEN** a bind activates the driver and validates the re-read spec's cached versions, while another bind concurrently observes the driver as already activated
- **THEN** validation is performed under the same lock hold as activation and the activation is rolled back before being recorded if it fails, so a concurrent binder only ever adopts a validated registration

#### Scenario: A failed or rolled-back bind never sheds a concurrent bind's state
- **WHEN** one bind activates the driver and then fails (e.g. a prerequisite is unmet) while another concurrent bind has already adopted that activation and bound its device
- **THEN** the failing bind's rollback deactivates only when the driver is idle (no bind in flight and no device bound), so it cannot shed the live state the other bind depends on; and if the last device was removed while a bind was racing, the teardown is retried when that bind finishes idle rather than leaving the driver active forever

#### Scenario: Deactivate then reactivate round trip
- **WHEN** a driver is activated, then deactivated after its last device is removed, then a new matching device is later commissioned
- **THEN** the driver is activated again from disk and dispatches to its handlers correctly
