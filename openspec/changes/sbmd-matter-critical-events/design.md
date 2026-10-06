## Context

The SBMD v5 runtime (`SpecBasedMatterDeviceDriver.cpp`) already has a fully wired event dispatch pipeline: Matter events flow from `OnEventData` → `HandleEvent` → dispatch table lookup → `BuildEventArgs` (producing `args.event.tlvBase64`) → JS handler invocation. The infrastructure is complete; no C++ changes are required.

> **Implementation note (ported to current `main`):** This change was implemented against `main` after the SBMD v5 migration. Drivers use `schemaVersion: '5.0'`, the door-lock handlers follow the current named-function style, and `updateResource` takes the endpoint id as its first argument (`updateResource(args.endpointId, RES, value)`). Handler JSValue lifetime is managed structurally by the runtime's `SafeJSValue` RAII wrapper, so no driver-side GC handling is needed.

Three production drivers are missing `eventHandlers` blocks:
- `door-lock.sbmd.js` — handles the `doorLock` device class
- `contact-sensor.sbmd.js` — handles the `sensor` device class (contact switch subtype)
- `water-leak-detector.sbmd.js` — handles the `sensor` device class (water subtype)

The Zigbee door lock driver (`zigbeeDoorLockDeviceDriver.c` + `doorLockCluster.c`) provides the precedent for alarm-to-resource mapping and state-clearing semantics, which we mirror exactly.

## Goals / Non-Goals

**Goals:**
- Add `eventHandlers` to all three drivers consuming the relevant Critical/Informational events
- Introduce `jammed`, `tampered`, `invalidCodeEntryLimit` resources on the door lock driver
- Mirror the Zigbee driver's alarm-to-resource mapping and clearing behavior
- Fix the `args.event` API documentation in `docs/SBMD.md`

**Non-Goals:**
- `DoorStateChange` event (requires Door Position Sensor feature; no field devices have it)
- `LockOperationError` / `LockUserChange` audit-trail events (out of scope per ticket)
- Time-based clearing of `invalidCodeEntryLimit` (requires a timer primitive not yet in SBMD v5)
- C++ `clusterFeatureMaps` TODO (unrelated to event handling)

## Decisions

### Decision 1: Dual-path live updates (attribute handler + event handler)

All three drivers keep their existing `attributeHandlers` live-update handler **and** add an `eventHandlers` handler for the same state, both active. A `seed` handler additionally reads the relevant attribute once at commission time to establish initial resource state.

**Rationale**: The event alone is not a safe replacement for the attribute in released Matter 1.5.1:
- `BooleanState.StateChange` is **optional** conformance (mandatory only under an in-progress spec ifdef) and **INFO** priority — a spec-conformant contact/leak sensor may never emit it. Removing `handleStateValue` would leave `faulted` frozen on such devices.
- `DoorLock.LockOperation` is CRITICAL only for Unlock/ForcedUser; a Lock MAY be INFO. `DoorLockAlarm` is CRITICAL/mandatory, but `LockState` remains the mandatory attribute baseline.

Keeping both is safe because it does **not** produce duplicate resource-changed events: `jammed`/`tampered`/`invalidCodeEntryLimit`/`faulted`/`locked` are `CACHING_POLICY_ALWAYS` (read-only, non-volatile), and `deviceService.c` only emits `RESOURCE_UPDATED` when the value actually changes (`didChange`). A redundant same-value update from the second path is suppressed and emits nothing.

**Alternative considered**: Event-only live updates (remove the attribute handler). Rejected — it breaks live reporting on devices that don't implement the optional event, and the duplicate-event cost that once motivated it does not exist given same-value suppression.

---

### Decision 2: TLV decoding pattern

Event payloads are decoded via `Sbmd.Tlv.decode(args.event.tlvBase64)`. A struct payload decodes to an **object keyed by each field's numeric TLV context tag** (e.g. `{0: <alarmCode>}`); a single scalar payload decodes to the value directly. Fields are accessed by tag number (e.g. `fields[0]` reads the field with context tag 0).

```
args.event.tlvBase64 (base64 struct TLV)
  │
  └─► Sbmd.Tlv.decode()
        └─► { 0: <alarmCode>, 1: <source>, ... }   // object keyed by context tag
                  ↑
              fields[0]  // key "0", not an array index
```

**Rationale**: Matches the implementation in `BuildEventArgs` (which sets `tlvBase64`, not `data`) and `sbmd-tlv.js`, which converts struct fields into an object keyed by their context tags (verified by `SbmdTlvTest.DecodeStruct`, which decodes to `{"1":5,"2":true}`). Numeric access such as `fields[0]` works because tag `0` is an object key `"0"`, not because the payload is an array. Tag-number access is safe for these well-known events where the Matter spec fixes each field's context tag.

**Alternative considered**: Iterating keys to find a tag. More defensive but verbose for these fixed single-or-small-field events. Not needed given the Matter-defined tag assignments.

---

### Decision 3: Door Lock alarm-to-resource mapping (mirrors Zigbee)

| Matter AlarmCode | Value | Resource updated | Zigbee precedent |
|---|---|---|---|
| LockJammed | 0x00 | `jammed = true` | `BOLT_JAMMED` → `jammedStateChanged(true)` |
| LockFactoryReset | 0x01 | log only | `LOCK_RESET_TO_FACTORY_DEFAULTS` → log |
| LockRadioPowerCycled | 0x03 | log only | `RF_MODULE_POWER_CYCLED` → log |
| WrongCodeEntryLimit | 0x04 | `invalidCodeEntryLimit = true` | `TAMPER_ALARM_WRONG_CODE_ENTRY_LIMIT` → `invalidCodeEntryLimitChanged(true)` |
| FrontEscutcheonRemoved | 0x05 | `tampered = true` | `TAMPER_ALARM_FRONT_ESCUTCHEON_REMOVED` → `tamperedStateChanged(true)` |
| DoorForcedOpen | 0x06 | `tampered = true` | `DOOR_FORCED_OPEN_WHILE_LOCKED` → `tamperedStateChanged(true)` |
| DoorAjar | 0x07 | log only | (Matter-only; no Zigbee precedent) |
| ForcedUser | 0x08 | log only | (Matter-only; no Zigbee precedent) |

---

### Decision 4: LockOperation clearing semantics (mirrors Zigbee)

On `LockOperation` (successful lock/unlock/unlatch):
- **`tampered`** → always clear to `false` (any source)
- **`invalidCodeEntryLimit`** → always clear to `false` (any source, best approximation — see Risk 1)
- **`jammed`** → clear to `false` **only** when `OperationSource == Manual (1)`

**Rationale**: Mirrors `zigbeeDoorLockDeviceDriver.c` `lockedStateChanged()` exactly. `tampered` is cleared on any operation because a functioning lock implies tamper has resolved. `jammed` is only cleared on manual source because a motor-driven remote unlock could succeed even with a partially jammed bolt, whereas a person physically turning the lock cannot.

```
LockOperation event
  │
  ├─ opType == Lock (0)    ──► locked=true,  jammed=false (if manual), tampered=false, invalidCodeEntryLimit=false
  ├─ opType == Unlock (1)  ──► locked=false, jammed=false (if manual), tampered=false, invalidCodeEntryLimit=false
  ├─ opType == Unlatch (4) ──► locked=false, jammed=false (if manual), tampered=false, invalidCodeEntryLimit=false
  └─ other                 ──► no-op
```

The `OperationSource` field is at TLV tag 1 in the `LockOperation` struct.

---

### Decision 5: New door lock resources are additive and seeded to `"false"`

`jammed`, `tampered`, and `invalidCodeEntryLimit` are declared in the driver's `endpoints['1'].resources` block with `type: 'boolean', modes: ['read'], prerequisites: [CL_DOOR_LOCK]`, each with a `seed` handler that establishes `"false"` at commission time but preserves an already-set value on later re-seeds.

**Rationale (parity with Zigbee)**: the Zigbee driver seeds all three to `"false"` at pairing via `initialResourceValuesPutEndpointValue(...)`. Seeding them the same way keeps the network-neutral resource interface consistent — a freshly commissioned Matter lock reports a definite `"false"` (not faulted) rather than a `null`/unknown value, so consumers do not need Matter-specific branching to distinguish "not tampered" from "unknown". The first qualifying event then flips the resource as needed.

**Preserve-on-synchronize**: `SeedInitialResourceValues` re-runs every `seed` handler on each synchronize/reconnect, not just at commission. A naive handler that always returns `'false'` would clobber a live fault (e.g. a `jammed` raised while Barton was in comm-fail) the moment the device reconnected. Each fault seed handler therefore reads its own current resource value via `supplements: { resources: ['1/<name>'] }` and returns the existing value when set, falling back to `'false'` only when the resource has no value yet (commission). This yields `"false"` on first commission while leaving an existing fault intact across reconnects.

**`locked` source/userId metadata**: the `LockOperation` handler attaches `{ source, userId }` metadata to the `locked` update (mapping Matter `OperationSourceEnum` → the canonical `DOORLOCK_PROFILE_LOCKED_SOURCE_*` strings and `UserIndex` at TLV tag 2), matching the Zigbee driver. The attribute-path `locked` update carries no source (an attribute report has none); when both paths fire, same-value suppression means whichever changes the value first wins.

---

### Decision 6: Trigger reconfiguration via the endpoint `profileVersion`

Reconfiguration of already-commissioned devices is decided by `deviceServiceDeviceNeedsReconfiguring`, which compares the Barton **device-class version** and the **endpoint profile version** against the current driver — it does **not** look at the SBMD top-level `driverVersion` (that field is only recorded and logged by `SbmdLoader`).

Because the door lock adds new endpoint resources, its endpoint `profileVersion` is bumped from `3` to `4` so `deviceServiceDeviceNeedsReconfiguring` detects the mismatch, reruns configuration on reconnect, and registers `jammed`, `tampered`, and `invalidCodeEntryLimit`. `driverVersion` is also bumped to `2` as a content marker, but it does not itself drive reconfiguration.

The contact sensor and water leak detector add no new resources (event-only live updates plus a one-time seed on the existing `faulted` resource). Barton subscribes to every device with a full wildcard (all attributes **and** all events — see `DeviceDataCache::OnDeviceConnected`), so already-commissioned sensors already receive `BooleanState.StateChange`; after a normal restart the new event handler picks it up with no per-device reconfiguration. They therefore need no version bump; their `driverVersion` bump to `2` is a content marker only.

**Sensor `faulted` seed polarity and preserve-on-resync**: the `faulted` seed reads the `StateValue` attribute supplement. When `StateValue` is available it sets `faulted` from it (contact: closed → `"false"`, open → `"true"`; water: water → `"true"`, dry → `"false"`). Because the seed also re-runs on every synchronize/reconnect (`SeedInitialResourceValues`), and `MakeAttrFetcher` returns `null` on a cache miss (e.g. the subscription has not yet repopulated `StateValue` after reconnect), the seed must not derive a value from a missing `StateValue`. It therefore reads its own current `faulted` resource (`supplements.resources: ['1/faulted']`) and, on an unavailable `StateValue`, preserves that existing value — falling back to a per-sensor fail-safe default only at commission when no value exists yet (contact → `"true"` so an unknown open contact is not missed; water → `"false"` to avoid a false leak alarm). This mirrors the door-lock fault seeds (Decision 5) so a reconnect with a cold attribute cache cannot spuriously fault a closed contact sensor or clear a real water leak.

### Decision 7: Events that do not map to a resource are logged or deferred

Resolves the open question of what to do with events that have no 1:1 resource mapping:

- **Unresourced `DoorLockAlarm` codes** (`0x01` LockFactoryReset, `0x03` LockRadioPowerCycled, `0x07` DoorAjar, `0x08` ForcedUser) are **logged and ignored** — the handler exists but takes no resource action, so a diagnostic record is emitted without inventing a resource.
- **Audit-trail / feature-gated events** (`DoorStateChange`, `LockOperationError`, `LockUserChange`) are **deferred / out of scope** — no handler is registered, so the wildcard-delivered event is silently dropped by the dispatch table.

No new resources are invented to hold event data that has no established Barton mapping.

## Risks / Trade-offs

**[Risk 1] `invalidCodeEntryLimit` clears on any LockOperation, not just after lockout expiry**
→ *Mitigation*: This is the best approximation without a timer. A remote unlock during an active keypad lockout would incorrectly clear the resource. This is an observable interface difference from the Zigbee driver, which auto-clears on a lockout-duration timer (`restoreLockoutCallback`); the Matter equivalent (`UserCodeTemporaryDisableTime`, attribute 0x0031) exists, so once an SBMD scheduler/`scheduleCallback` result-builder op lands this becomes directly implementable. Acceptable for v1; tracked as a future enhancement.

**[Risk 2] `DoorLockAlarm` has no "cleared" event in Matter**
→ *Mitigation*: Clearing is inferred from `LockOperation`. This is the same design as Zigbee. No further mitigation possible without a richer event model from the device.

**[Risk 3] `lastUserInteractionDate` is not updated (Zigbee parity gap)**
→ *Mitigation*: The Zigbee driver updates the device-level `lastUserInteractionDate` on each lock change. The SBMD driver does not, and this cannot be done in the driver alone today: while the SBMD result executor can *update* a device-level resource (omit the endpoint → device root) and `Date.now()` is available, the SBMD loader does **not** extract the schema's top-level device-level `resources` block into `SbmdRegistration`, so a `.sbmd.js` cannot *register* the resource. Closing this gap requires a C++ runtime change (loader extraction + registration + `SpecBasedMatterDeviceDriver` registration). Deferred to a follow-up; tracked as a user story.

**[Risk 4] `doorLock` profile version numbering has diverged across stacks**
→ *Mitigation*: Zigbee registers `doorLock` profile version `2`; this change takes the Matter driver to `4`. Same profile, two independent counters. Pre-existing; a follow-up ticket should decide whether the profile version describes the shared profile contract or the per-stack driver.

**[Risk 5] SBMD.md `args.event` doc fix may conflict with an upstream fix**
→ *Mitigation*: The fix is scoped to the event API table and the door-lock example. If another author fixes the same lines, a merge conflict will surface it cleanly.
