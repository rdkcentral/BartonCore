## ADDED Requirements

### Requirement: Persistent data write via result op
The runtime SHALL support a `storage.setPersistentData(key, value)` result builder method that stores a string key-value pair in per-device non-volatile storage. The key SHALL be namespaced with an `sbmd.` prefix when written to the device metadata store. The value SHALL survive device and service reboots.

#### Scenario: Handler stores persistent data
- **WHEN** a handler returns `Sbmd.result().storage.setPersistentData("lastLockOp", "lock").success()`
- **THEN** the runtime writes key `sbmd.lastLockOp` with value `"lock"` to the device's metadata store via `deviceServiceSetMetadata`

#### Scenario: Persistent data survives restart
- **WHEN** persistent data was written with key `"myKey"` and the service restarts
- **THEN** a subsequent supplement fetch for `persistentData: ["myKey"]` returns the previously stored value

### Requirement: Transient data write via result op
The runtime SHALL support a `storage.setTransientData(key, value, ttlSecs)` result builder method that stores a string key-value pair in a per-device in-memory map with a time-to-live. The `ttlSecs` parameter (number) is required and specifies how many seconds until the entry expires. Expired entries SHALL return `null` when read via supplements.

#### Scenario: Handler stores transient data with TTL
- **WHEN** a handler returns `Sbmd.result().storage.setTransientData("debounce", "1", 30).success()`
- **THEN** the runtime stores key `"debounce"` with value `"1"` and an expiry 30 seconds from now

#### Scenario: Transient data expires after TTL
- **WHEN** transient data was written with `ttlSecs: 5` and 6 seconds have elapsed
- **THEN** a supplement fetch for `transientData: ["debounce"]` returns `null`

#### Scenario: Transient data available before TTL
- **WHEN** transient data was written with `ttlSecs: 30` and 10 seconds have elapsed
- **THEN** a supplement fetch for the key returns the stored value

#### Scenario: Transient data does not survive restart
- **WHEN** transient data was stored and the service restarts
- **THEN** a supplement fetch for the key returns `null`

### Requirement: Persistent data read via supplements
The runtime SHALL support a `persistentData` array in the supplements declaration. Each entry is a string key name. Before calling the handler, the runtime SHALL fetch the value from the device metadata store (using the `sbmd.` prefix) and deliver it in `args.supplements.persistentData[key]`. If the key does not exist, the value SHALL be `null`.

#### Scenario: Supplement fetches existing persistent data
- **WHEN** a handler declares `supplements: { persistentData: ["lastLockOp"] }` and the key has been previously set
- **THEN** `args.supplements.persistentData.lastLockOp` contains the stored string value

#### Scenario: Supplement fetches non-existent persistent data
- **WHEN** a handler declares `supplements: { persistentData: ["missingKey"] }` and the key has never been set
- **THEN** `args.supplements.persistentData.missingKey` is `null`

### Requirement: Transient data read via supplements
The runtime SHALL support a `transientData` array in the supplements declaration. Each entry is a string key name. Before calling the handler, the runtime SHALL look up the key in the per-device in-memory transient store and deliver its value in `args.supplements.transientData[key]` if the entry exists and has not expired. Expired or missing entries SHALL be `null`.

#### Scenario: Supplement fetches existing transient data
- **WHEN** a handler declares `supplements: { transientData: ["debounce"] }` and the key was stored with remaining TTL
- **THEN** `args.supplements.transientData.debounce` contains the stored string value

#### Scenario: Supplement fetches expired transient data
- **WHEN** a handler declares `supplements: { transientData: ["debounce"] }` and the key's TTL has elapsed
- **THEN** `args.supplements.transientData.debounce` is `null`

### Requirement: No standalone getter functions
The runtime SHALL NOT provide `Sbmd.getPersistentData()` or `Sbmd.getTransientData()` standalone JavaScript functions. All storage reads SHALL go through the supplements mechanism.

#### Scenario: No getPersistentData on Sbmd namespace
- **WHEN** a handler attempts to call `Sbmd.getPersistentData("key")`
- **THEN** a JavaScript TypeError occurs because the function does not exist
