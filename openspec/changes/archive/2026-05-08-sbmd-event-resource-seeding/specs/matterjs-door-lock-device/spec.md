## ADDED Requirements

### Requirement: Side-band operations emit LockOperation events
The door lock virtual device SHALL emit a `LockOperation` event (DoorLock cluster `0x0101`, event `0x0002`) whenever the lock state changes via a side-band operation, in addition to updating the `LockState` attribute. The `LockOperationType` field in the event SHALL be `0` (Lock) for side-band lock operations and `1` (Unlock) for side-band unlock operations. This is required for SBMD drivers that use event-driven resource updates rather than attribute subscription, such as the `door-lock.sbmd` spec with `mapper.event` on the `locked` resource.

#### Scenario: Side-band lock emits LockOperation event
- **WHEN** a side-band `lock` operation is sent to the virtual door lock
- **THEN** the device SHALL emit a `LockOperation` event with `LockOperationType` = 0 (Lock)
- **AND** the `LockState` attribute SHALL be updated to `Locked`

#### Scenario: Side-band unlock emits LockOperation event
- **WHEN** a side-band `unlock` operation is sent to the virtual door lock
- **THEN** the device SHALL emit a `LockOperation` event with `LockOperationType` = 1 (Unlock)
- **AND** the `LockState` attribute SHALL be updated to `Unlocked`
