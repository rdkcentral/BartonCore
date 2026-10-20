## MODIFIED Requirements

### Requirement: Supplements pre-loading
When a handler declares `supplements`, the runtime SHALL resolve alias names to cluster+attribute IDs and obtain attribute values, resource values, persistent data, and transient data through the SBMD Data Access Layer (which serves the device data cache, the Barton resource store, device metadata, and the in-memory transient store), and deliver them in `args.supplements` before calling the handler.

#### Scenario: Attribute supplement loaded from cache
- **WHEN** a `seed` handler declares `supplements: { attributes: ["onOff"] }` and the device data cache has a value for the `onOff` alias
- **THEN** `args.supplements.attributes.onOff` contains the decoded attribute value

#### Scenario: Resource supplement loaded
- **WHEN** a handler declares `supplements: { resources: ["1/isOn"] }`
- **THEN** `args.supplements.resources["1/isOn"]` contains the current Barton resource value

#### Scenario: Supplements served by the DAL
- **WHEN** any supplement category is declared
- **THEN** each value SHALL be read via `DataAccessLayer::Read` and SHALL reflect all changes reconciled into the DAL up to the time of the read
