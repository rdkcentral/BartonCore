## ADDED Requirements

### Requirement: Door lock SBMD driver handles DoorLockAlarm events
The door lock SBMD driver SHALL subscribe to and handle `DoorLockAlarm` events (cluster 0x0101, event 0x0000). On receipt, the driver SHALL update the `jammed`, `tampered`, or `invalidCodeEntryLimit` resource according to the alarm code, or log and ignore alarm codes with no corresponding resource.

Alarm code mapping:
- 0x00 (LockJammed) → `jammed = "true"`
- 0x01 (LockFactoryReset) → log only
- 0x03 (LockRadioPowerCycled) → log only
- 0x04 (WrongCodeEntryLimit) → `invalidCodeEntryLimit = "true"`
- 0x05 (FrontEscutcheonRemoved) → `tampered = "true"`
- 0x06 (DoorForcedOpen) → `tampered = "true"`
- 0x07 (DoorAjar) → log only
- 0x08 (ForcedUser) → log only

#### Scenario: Lock bolt jammed alarm sets jammed resource
- **WHEN** a `DoorLockAlarm` event is received with AlarmCode = 0x00 (LockJammed)
- **THEN** the `jammed` resource SHALL be updated to `"true"`

#### Scenario: Wrong code entry limit alarm sets invalidCodeEntryLimit resource
- **WHEN** a `DoorLockAlarm` event is received with AlarmCode = 0x04 (WrongCodeEntryLimit)
- **THEN** the `invalidCodeEntryLimit` resource SHALL be updated to `"true"`

#### Scenario: Front escutcheon removed alarm sets tampered resource
- **WHEN** a `DoorLockAlarm` event is received with AlarmCode = 0x05 (FrontEscutcheonRemoved)
- **THEN** the `tampered` resource SHALL be updated to `"true"`

#### Scenario: Door forced open alarm sets tampered resource
- **WHEN** a `DoorLockAlarm` event is received with AlarmCode = 0x06 (DoorForcedOpen)
- **THEN** the `tampered` resource SHALL be updated to `"true"`

#### Scenario: Unresourced alarm codes are logged and ignored
- **WHEN** a `DoorLockAlarm` event is received with AlarmCode = 0x01, 0x03, 0x07, or 0x08
- **THEN** the event SHALL be logged and no resource update SHALL occur

---

### Requirement: Door lock SBMD driver handles LockOperation events
The door lock SBMD driver SHALL subscribe to and handle `LockOperation` events (cluster 0x0101, event 0x0002). On receipt, the driver SHALL update `locked` based on the operation type and clear `tampered`, `invalidCodeEntryLimit`, and conditionally `jammed`.

- LockOperationType 0x00 (Lock) → `locked = "true"`, clear `tampered`, `invalidCodeEntryLimit`, and if OperationSource == 0x01 (Manual) also clear `jammed`
- LockOperationType 0x01 (Unlock) → `locked = "false"`, clear `tampered`, `invalidCodeEntryLimit`, and if OperationSource == 0x01 (Manual) also clear `jammed`
- LockOperationType 0x04 (Unlatch) → `locked = "false"`, clear `tampered`, `invalidCodeEntryLimit`, and if OperationSource == 0x01 (Manual) also clear `jammed`
- Other operation types → no-op

The door lock driver SHALL remove the `attributeHandlers.handleLockState` live-update handler. Live updates to `locked` SHALL be driven exclusively by `LockOperation` events. The existing `seed` handler continues to establish initial `locked` state at commission time.

#### Scenario: Lock operation updates locked resource
- **WHEN** a `LockOperation` event is received with LockOperationType = 0x00 (Lock)
- **THEN** the `locked` resource SHALL be updated to `"true"`

#### Scenario: Unlock operation updates locked resource
- **WHEN** a `LockOperation` event is received with LockOperationType = 0x01 (Unlock)
- **THEN** the `locked` resource SHALL be updated to `"false"`

#### Scenario: Lock operation clears tampered regardless of source
- **WHEN** a `LockOperation` event is received with any LockOperationType in {0x00, 0x01, 0x04}
- **THEN** the `tampered` resource SHALL be updated to `"false"`

#### Scenario: Lock operation clears invalidCodeEntryLimit regardless of source
- **WHEN** a `LockOperation` event is received with any LockOperationType in {0x00, 0x01, 0x04}
- **THEN** the `invalidCodeEntryLimit` resource SHALL be updated to `"false"`

#### Scenario: Manual lock operation clears jammed
- **WHEN** a `LockOperation` event is received and OperationSource == 0x01 (Manual)
- **THEN** the `jammed` resource SHALL be updated to `"false"`

#### Scenario: Non-manual lock operation does not clear jammed
- **WHEN** a `LockOperation` event is received and OperationSource != 0x01 (Manual)
- **THEN** the `jammed` resource SHALL NOT be updated

---

### Requirement: Door lock SBMD driver exposes jammed, tampered, and invalidCodeEntryLimit resources
The door lock SBMD driver (endpoint `1`, profile `doorLock`) SHALL declare the following read-only boolean resources with prerequisite cluster 0x0101:
- `jammed` (resource name per `DOORLOCK_PROFILE_RESOURCE_JAMMED`)
- `tampered` (resource name per `DOORLOCK_PROFILE_RESOURCE_TAMPERED`)
- `invalidCodeEntryLimit` (resource name per `DOORLOCK_PROFILE_RESOURCE_INVALID_CODE_ENTRY_LIMIT`)

These resources have no initial value until the first qualifying event fires.

#### Scenario: New resources registered on commission
- **WHEN** a Matter Door Lock device is commissioned
- **THEN** `jammed`, `tampered`, and `invalidCodeEntryLimit` resources SHALL be registered on endpoint `1`

#### Scenario: Resources absent until first event
- **WHEN** a Matter Door Lock device has been commissioned but no `DoorLockAlarm` or `LockOperation` event has been received
- **THEN** `jammed`, `tampered`, and `invalidCodeEntryLimit` resources SHALL have no cached value

---

### Requirement: Door lock endpoint profile version triggers reconfiguration for new resources
The door lock SBMD driver SHALL bump the endpoint `1` `profileVersion` (from `3` to `4`) so that `deviceServiceDeviceNeedsReconfiguring` detects the change and reconfigures already-commissioned devices, registering the new `jammed`, `tampered`, and `invalidCodeEntryLimit` resources. The top-level `driverVersion` is bumped to `2` as a content marker but does not itself trigger reconfiguration (it is only recorded and logged).

#### Scenario: Reconfiguration registers new resources on upgrade
- **WHEN** a device commissioned with the prior door lock driver (endpoint profile version `3`) reconnects after the driver is upgraded to endpoint profile version `4`
- **THEN** the driver SHALL reconfigure and register the `jammed`, `tampered`, and `invalidCodeEntryLimit` resources

---

### Requirement: DoorStateChange events are not handled (punted)
The door lock SBMD driver SHALL NOT include a handler for `DoorStateChange` events (cluster 0x0101, event 0x0001) in this change. This event requires the Door Position Sensor feature (DPS, bit 2 of the DoorLock feature map), which is not present on any currently field-deployed device (confirmed: Aqara Smart Lock U400 does not have DPS). DoorStateChange handling is deferred to a future change.

#### Scenario: DoorStateChange events have no handler
- **WHEN** a Matter Door Lock device emits a `DoorStateChange` event
- **THEN** the event SHALL be silently ignored (no matching handler in the dispatch table)
