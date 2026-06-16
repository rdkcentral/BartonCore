## Context

SBMD handlers need per-device key-value storage for debounce state, last-known values,
and operational context. The scaffolding exists (JS builder emits `setPersistentData` /
`setTransientData` ops, C++ parser extracts them) but the executor logs "not yet
implemented". The `setTransientData` JS builder is also missing the `ttlSecs` parameter.
The doc promised standalone `Sbmd.getPersistentData()` / `Sbmd.getTransientData()` getter
functions, but these are architecturally wrong — all data reads should flow through
supplements to maintain the result-builder-only side-effect model.

Storage is scoped per-device. The existing Barton `deviceService` metadata API
(`deviceServiceGetMetadata` / `deviceServiceSetMetadata`) provides persistent string
key-value storage backed by the JSON database. Transient storage is an in-memory map
with TTL-based expiry, managed by the SBMD runtime itself.

```
Handler ──→ supplements: { persistentData: ["k1"] } ──→ AddSupplements ──→ args.supplements.persistentData.k1
                                                           │
                                                           ├─ attrFetcher (existing)
                                                           ├─ resFetcher  (existing)
                                                           ├─ persistFetcher (NEW: deviceServiceGetMetadata)
                                                           └─ transientFetcher (NEW: in-memory map lookup)

Handler ──→ Sbmd.result().storage.setPersistentData("k1", "v1").success()
                                                           │
                                                     ExecuteOps
                                                           ├─ SetPersistentData → deviceServiceSetMetadata
                                                           └─ SetTransientData → in-memory map with TTL
```

## Goals / Non-Goals

**Goals:**
- Reads through supplements only — no standalone JS getter functions
- Persistent storage via device metadata API (survives reboots)
- Transient storage via in-memory map with TTL-based expiry (process lifetime)
- Fix `setTransientData` to accept `ttlSecs` across all layers
- Remove `Sbmd.getPersistentData()` / `Sbmd.getTransientData()` from doc

**Non-Goals:**
- No new storage backends or databases
- No cross-device storage
- No complex data types (values are strings)
- No TTL enforcement thread — expiry checked on read

## Decisions

### 1. Reads through supplements, not standalone getters

Supplements are the established pattern for pre-fetching data before handler
execution. Adding `persistentData` and `transientData` arrays to the supplements
schema keeps the model consistent: handlers declare what they need, the runtime
fetches it, and it arrives in `args.supplements`. This eliminates the need for
synchronous native JS functions that would require C function registration and
break the side-effect-free handler model.

Alternative considered: Standalone `Sbmd.getPersistentData()` — rejected because
it introduces synchronous native calls and breaks the pattern that all external
reads are declared upfront.

### 2. Persistent storage maps to device metadata

Device metadata (`deviceServiceGetMetadata` / `deviceServiceSetMetadata`) is an
existing per-device string key-value store backed by the JSON database. SBMD
persistent data keys are stored with a `sbmd.` prefix to namespace them from
other metadata.

URI format: `/devices/{deviceUuid}/metadata/sbmd.{key}`

Alternative considered: Separate storage file per driver — rejected because the
metadata API already exists and is well-tested.

### 3. Transient storage is a process-lifetime in-memory map

A `std::unordered_map<std::string, TransientEntry>` on the driver instance, where
`TransientEntry` holds value + expiry timestamp. Entries are checked for expiry on
read (lazy expiry). No background thread needed.

Key format: per-device scoping is implicit — each `SpecBasedMatterDeviceDriver`
instance has its own map.

### 4. TTL is mandatory for transient data

`setTransientData(key, value, ttlSecs)` requires `ttlSecs`. Without TTL, use
persistent data instead. This prevents accidental memory leaks from
never-expiring transient entries.

## Risks / Trade-offs

- **Lazy expiry accumulates stale entries** → Acceptable for the expected low
  volume of transient keys per device. A periodic sweep can be added later if
  needed.
- **Metadata URI prefix collision** → Mitigated by `sbmd.` namespace prefix.
  Drivers cannot access non-SBMD metadata.
- **Thread safety for transient map** → Transient map access occurs under the
  existing JS mutex (`MQuickJsRuntime::GetMutex()`), same as handler invocation.
  No additional locking needed.
