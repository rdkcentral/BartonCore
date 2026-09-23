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

### Decision 1: Event-only live updates with one-time attribute seed

All three drivers switch to event-only live updates. The `attributeHandlers` block for the live-update handler is removed. A `seed` handler reads the relevant attribute once at commission time to establish initial resource state. After that, only Critical events drive updates.

**Rationale**: Critical events have higher delivery reliability than attribute reports on congested Matter networks — that is the entire point of the ticket. Keeping the attribute handler active alongside the event handler undermines this by producing two updates per state change and dilutes the signal, causing subscribers to see duplicate events for a single physical occurrence. The seed handler covers initial state; the subscription priming report that would have driven the live attribute handler is no longer needed.

**Alternative considered**: Dual update path (attribute subscription AND event handler, both active). Rejected — subscribers receive two resource-changed events per physical state change (once from the attribute report, once from the Critical event). Violates expected semantics and degrades the consumer experience.

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

### Decision 5: New door lock resources are additive, with no value until first event

`jammed`, `tampered`, and `invalidCodeEntryLimit` are declared in the driver's `endpoints['1'].resources` block with `type: 'boolean', modes: ['read'], prerequisites: [CL_DOOR_LOCK]`. They have **no seed handler**, so they carry no cached value until the first qualifying event sets them; consumers must handle an absent value.

**Note on the Zigbee precedent**: the Zigbee driver explicitly seeds these to `"false"` via `initialResourceValuesPutEndpointValue(...)`. The SBMD driver intentionally leaves them unseeded (no value until first event), which matches the requirement in the spec ("Resources absent until first event").

---

### Decision 6: Trigger reconfiguration via the endpoint `profileVersion`

Reconfiguration of already-commissioned devices is decided by `deviceServiceDeviceNeedsReconfiguring`, which compares the Barton **device-class version** and the **endpoint profile version** against the current driver — it does **not** look at the SBMD top-level `driverVersion` (that field is only recorded and logged by `SbmdLoader`).

Because the door lock adds new endpoint resources, its endpoint `profileVersion` is bumped from `3` to `4` so `deviceServiceDeviceNeedsReconfiguring` detects the mismatch, reruns configuration on reconnect, and registers `jammed`, `tampered`, and `invalidCodeEntryLimit`. `driverVersion` is also bumped to `2` as a content marker, but it does not itself drive reconfiguration.

The contact sensor and water leak detector add no new resources (event-only live updates plus a one-time seed on the existing `faulted` resource). Barton subscribes to every device with a full wildcard (all attributes **and** all events — see `DeviceDataCache::OnDeviceConnected`), so already-commissioned sensors already receive `BooleanState.StateChange`; after a normal restart the new event handler picks it up with no per-device reconfiguration. They therefore need no version bump; their `driverVersion` bump to `2` is a content marker only.

### Decision 7: Events that do not map to a resource are logged or deferred

Resolves the open question of what to do with events that have no 1:1 resource mapping:

- **Unresourced `DoorLockAlarm` codes** (`0x01` LockFactoryReset, `0x03` LockRadioPowerCycled, `0x07` DoorAjar, `0x08` ForcedUser) are **logged and ignored** — the handler exists but takes no resource action, so a diagnostic record is emitted without inventing a resource.
- **Audit-trail / feature-gated events** (`DoorStateChange`, `LockOperationError`, `LockUserChange`) are **deferred / out of scope** — no handler is registered, so the wildcard-delivered event is silently dropped by the dispatch table.

No new resources are invented to hold event data that has no established Barton mapping.

## Risks / Trade-offs

**[Risk 1] `invalidCodeEntryLimit` clears on any LockOperation, not just after lockout expiry**
→ *Mitigation*: This is the best approximation without a timer. A remote unlock during an active keypad lockout would incorrectly clear the resource. Acceptable for v1; time-based clearing tracked as a future enhancement (SBMD v5 needs a `scheduleCallback` result builder operation).

**[Risk 2] `DoorLockAlarm` has no "cleared" event in Matter**
→ *Mitigation*: Clearing is inferred from `LockOperation`. This is the same design as Zigbee. No further mitigation possible without a richer event model from the device.

**[Risk 3] `jammed`/`tampered`/`invalidCodeEntryLimit` start with no value until first event**
→ *Mitigation*: Resources are registered at commission time but have no initial value. Consumers must handle `null`/absent value. This **intentionally differs** from the Zigbee driver, which seeds these to `"false"` at commission; the SBMD driver leaves them unseeded (first qualifying event sets them). Marked in the spec.

**[Risk 4] SBMD.md `args.event` doc fix may conflict with an upstream fix**
→ *Mitigation*: The fix is scoped to the event API table and the door-lock example. If another author fixes the same lines, a merge conflict will surface it cleanly.
