## ADDED Requirements

### Requirement: seedFrom mapper type
A resource mapper SHALL support a `seedFrom` section (in addition to the existing `read`, `write`, `execute`, and `event` sections) for populating initial resource values from the attribute cache when the resource's ongoing updates are driven by a `mapper.event`. The `seedFrom` mapper SHALL NOT be used as a substitute for `mapper.read` — if ongoing attribute subscription updates are desired, `mapper.read` remains the correct choice. The `seedFrom` mapper SHALL use the `sbmdReadArgs` script interface (identical to `mapper.read`).

#### Scenario: Event-driven resource has initial value at commission
- **WHEN** a resource declares `mapper.event` for ongoing updates and `mapper.seedFrom` pointing to a corresponding attribute alias
- **THEN** the resource SHALL have a non-null value immediately after device commissioning without waiting for the first event to fire

#### Scenario: Event-driven resource has initial value after hub restart
- **WHEN** the hub restarts and a device with a `seedFrom` resource is synchronized
- **THEN** the resource SHALL be re-seeded from the attribute cache before any new event arrives

## MODIFIED Requirements

### Requirement: Event mapper (MODIFIED — added suppress scenario)
> **Note**: The event mapper requirement already existed. This change clarifies the contract for scripts that intentionally return no output, which was previously undefined behavior (the infrastructure incorrectly logged it as an error). The suppress contract is required by `door-lock.sbmd`'s `LockOperation` event script, which returns `{}` for non-state-change event types (2–4).

A resource's mapper MAY contain an `event` section with an `alias` and a `script`. If the script omits the `output` key (e.g., returns `{}`), the event SHALL be silently suppressed — the resource is not updated and no error is logged.

#### Scenario: Event script suppresses update by omitting output
- **WHEN** an event script returns an object without an `output` key (e.g., `{}`)
- **THEN** the resource SHALL NOT be updated and no error SHALL be logged — this is the standard mechanism for ignoring non-state-change events
