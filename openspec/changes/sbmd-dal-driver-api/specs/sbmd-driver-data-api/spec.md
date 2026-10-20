## ADDED Requirements

### Requirement: Deferred callbacks receive fresh supplements
When a `requestCommand` or `readAttribute` `onResponse` or `onError` callback is invoked (including on timeout), the runtime SHALL populate `args.supplements` by reading from the Data Access Layer at callback time. The supplement declaration SHALL be the `supplements` option of the deferred terminal if present, otherwise the declaration of the handler that returned the terminal. If neither declares supplements, `args.supplements` SHALL be absent. `args.handlerContext` SHALL continue to carry the `context` value unchanged.

#### Scenario: Concurrent change visible after park
- **WHEN** handler A declares `supplements: { transientData: ["sessions"] }`, returns `requestCommand`, and while parked operation B changes `sessions`
- **THEN** A's `onResponse` SHALL receive B's value in `args.supplements.transientData.sessions`

#### Scenario: Error callback receives fresh supplements
- **WHEN** a parked `requestCommand` fails or times out and its handler declared supplements
- **THEN** `onError` SHALL receive `args.supplements` read at callback time

#### Scenario: Option overrides inherited declaration
- **WHEN** a `requestCommand` specifies `supplements: { resources: ["1/stream"] }`
- **THEN** the callbacks SHALL receive only the `resources` supplements declared in the option

---

### Requirement: Per-member transient data merge
The result builder SHALL provide `storage.mergeTransientData(name, patch, ttlSecs)`. At execution time the runtime SHALL apply `patch` to the current transient value of `name` using RFC 7396 JSON Merge Patch semantics, treating an absent or non-object current value as `{}`, and SHALL store the result with the given TTL. A `null` member in the patch SHALL delete that member. `ttlSecs` of 0 SHALL delete the key. An unparsable patch SHALL be logged and SHALL NOT modify the stored value.

#### Scenario: Merge preserves other members
- **WHEN** `sessions` is `{"1":{"state":"streaming"}}` and a handler merges `{"2":{"state":"created"}}`
- **THEN** `sessions` SHALL be `{"1":{"state":"streaming"},"2":{"state":"created"}}`

#### Scenario: Null deletes a member
- **WHEN** a handler merges `{"1":null}` into `sessions`
- **THEN** member `1` SHALL be removed and other members SHALL be preserved

#### Scenario: Merge applies against value current at execution
- **WHEN** a deferred callback merges a patch after another operation changed the key during the park
- **THEN** the patch SHALL be applied to the changed value, not to the park-time value
