# Design: SBMD Data Access Layer — core (BARTON-421) and epic architecture (BARTON-420)

This document holds the epic-wide architecture for BARTON-420 and the detailed design for the DAL core delivered by BARTON-421. Detailed designs for the other changes live in their own change directories: `sbmd-dal-source-plugins` (BARTON-430), `sbmd-transaction-orchestrator` (BARTON-423), `sbmd-dal-driver-api` (BARTON-422), `sbmd-transaction-failure-handler` (BARTON-424), `sbmd-dal-driver-migration` (BARTON-425).

## Context

SBMD drivers (`core/deviceDrivers/matter/sbmd/`) run JavaScript handlers that read supplements (Matter attributes, Barton resources, persistent data stored as `sbmd.`-prefixed metadata, and transient data) and return ops (`updateResource`, `setMetadata`, `setPersistentData`, `setTransientData`, `log`) plus a terminal (`success`, `error`, `sendCommand`, `requestCommand`, `readAttribute`, `writeAttribute`).

Today's threading: a device-service API call reaches `MatterDeviceDriver::ConnectAndExecute` on the calling thread, which blocks on promises (`core/deviceDrivers/matter/MatterDeviceDriver.cpp` ~940–1050). The work lambda, the JS handler, supplement fetch, result execution, and deferred `onResponse`/`onError` all run on the Matter thread. `SpecBasedMatterDeviceDriver::transientStore` is Matter-thread-confined. The device-service main loop is the default GLib main context run on the `glibEvt` thread (`startMainLoop`, `core/src/deviceService.c:1811`).

Two defects motivate the epic. BARTON-411 is a lost update: a deferred SBMD callback writes back a transient-data snapshot captured at park time, erasing concurrent changes. BARTON-412: failures before a command leaves the stack (~18 `FailOperation(promises)` sites in `SpecBasedMatterDeviceDriver.cpp`, and `CompletePendingOperation(pendingId, false)`) never invoke the driver's `onError`. BARTON-412 is fixed separately on `sbmd-deferred-failure-callbacks`; the epic depends on it.

State when this change lands (D13): BARTON-423 (`sbmd-transaction-orchestrator`) has already delivered `SbmdTransaction`, `SbmdTransactionOrchestrator`, `SbmdWorker`, and `ThreadHop` (the shared thread-hop primitive the DAL reuses), and an internal data-access seam `SbmdDataAccess` / `SbmdDataSession` (`Fetch`, `Stage`, `StageConditional`, `RecordAttributeWrite`, `Commit`, `Abort`) implemented by `LegacySbmdDataAccess`, which wraps today's fetchers/setters and `transientStore` with correct thread affinity and holds staged writes in the `SbmdTransaction`. This change adds the DAL core and `DalSbmdDataAccess`, a DAL-backed implementation of that seam; BARTON-430 supplies the plugins, switches the orchestrator to `DalSbmdDataAccess`, and deletes `transientStore` and `LegacySbmdDataAccess` entirely.

No `docs/dev/architecture.md` exists in the project. SBMD architecture is documented in `docs/SBMD.md`.

## References

- [GLib `GMainContext`](https://docs.gtk.org/glib/struct.MainContext.html) — `g_main_context_invoke_full` and `g_main_context_is_owner` semantics used for marshaling onto, and detecting, the device-service main loop.
- [Matter SDK `PlatformManager::ScheduleWork`](https://github.com/project-chip/connectedhomeip/blob/master/src/include/platform/PlatformManager.h) — Mechanism behind `ThreadHop::OnMatter`, used for `MatterThread`-affinity plugins.
- `openspec/changes/sbmd-transaction-orchestrator/design.md` — `ThreadHop`, `SbmdDataAccess`/`SbmdDataSession`, `LegacySbmdDataAccess`, and the shared `SbmdDataAccess` contract test suite.
- [RFC 7396 JSON Merge Patch](https://www.rfc-editor.org/rfc/rfc7396) — Semantics for `storage.mergeTransientData` (BARTON-422).
- `docs/SBMD.md` — Current SBMD runtime, supplements, and terminal semantics.
- `core/deviceDrivers/matter/sbmd/SpecBasedMatterDeviceDriver.{h,cpp}` — Existing fetchers, `transientStore`, pending operations.

## Glossary

| Term | Meaning |
| --- | --- |
| **SBMD Transaction** | The driver-level execution of one request, from the initial handler invocation through every parked device operation and deferred callback, to the terminal result and, on failure, the failure handler. Code: `SbmdTransaction` (BARTON-423). Holds the operation log. |
| **DAL Transaction** | A caller-agnostic data-access unit: per-key reads routed to plugins, staged writes, commit or abort. Code: `DalTransaction` (this change). |
| **Orchestrator** | The thread that owns an SBMD Transaction and its deadline and schedules work onto other executors. Code: `SbmdTransactionOrchestrator` (BARTON-423). |
| **Plugin** | A `DataSourcePlugin` that serves one namespace (optionally one device). |
| **Binding** | A `PluginBinding`: one plugin's per-DAL-Transaction state. |
| **Affinity** | The executor a plugin's binding calls must run on: `MainLoop` or `MatterThread`. |
| **Atomic capability** | A plugin capability (`PluginCaps::atomic`) declaring support for sequence allocation and conditional writes (D11). |
| **Sequence allocation** | `DalTransaction::AllocateSequence(key)`: atomically increment a counter key and return the new value; immediate, never rolled back (D11). |
| **Conditional write** | `DalTransaction::StageConditional(key, expected, value)`: a staged write applied only if the key's current value equals `expected` at commit (D11). |
| **`SbmdDataAccess`** | Internal seam (BARTON-423) through which the orchestrator performs all SBMD data access. Implementations: `LegacySbmdDataAccess` (423, wraps today's paths) and `DalSbmdDataAccess` (this change, backed by the DAL). |
| **Recovery** | A step's `onError` returning a non-error terminal or continuation; the SBMD Transaction continues and `onTransactionFailure` does not run (D12). |

Normally one SBMD Transaction owns exactly one DAL Transaction. The DAL never references SBMD types; it can be used by any caller.

## Goals / Non-Goals

**Goals:**

- Define the epic-wide architecture (D1–D13) so every BARTON-420 change implements one consistent model.
- Deliver `DataKey`, `DataSourcePlugin`, `PluginBinding`, `DalPluginRegistry`, `DalTransaction`, `DalExecutor`, and the `Dal` façade.
- Atomic primitives (D11): `DalTransaction::AllocateSequence`, `DalTransaction::StageConditional`, the plugin `Atomic` capability, and commit-time condition checks.
- `DalSbmdDataAccess`: the DAL-backed implementation of the `SbmdDataAccess` seam introduced by BARTON-423 (D13).
- Main-loop confinement of all DAL state, generic executor hops for plugin affinity, inline execution when the caller owns the main loop.
- Batched routed reads (one hop per step), staged writes, read-your-own-staged-writes, best-effort commit with prior-value rollback, abort, abort-on-unregister.
- Deadlines on every wait.

**Non-Goals:**

- A data store, caches, versions, reconciliation, optimistic validation, conflicts (removed: D3, D4).
- Concrete plugins (BARTON-430), orchestrator (BARTON-423), driver API (BARTON-422), failure handler (BARTON-424), migration (BARTON-425).
- Two-phase commit.
- Public API or GObject changes.

## Architecture

### Orchestrator model

```text
 device-service API caller / SbmdWorker            (orchestrator: owns SbmdTransaction + deadline)
        |
        |  (1) batched supplement read + sequence allocation (D11) -+
        |                                                          v
        |                                   +-----------------------------------------+
        |                                   | device-service main loop (glibEvt)      |
        |                                   |  Dal / DalPluginRegistry / DalTransaction|
        |                                   |   - MainLoop plugins: called inline     |
        |                                   |   - MatterThread plugins: hop + wait ---+--+
        |                                   +-----------------------------------------+  |
        |                                                                                |
        |  (2) run JS handler with supplements ------------+                             |
        |  (4) send device op; response handed back -------+                             |
        |                                                  v                             v
        |                                   +-----------------------------------------+
        |                                   | Matter thread (CHIP event loop)         |
        |                                   |  QuickJS handler, device commands,      |
        |                                   |  attribute plugin reads (DeviceDataCache)|
        |                                   +-----------------------------------------+
        |
        |  (3) stage handler writes (incl. conditional), (6) commit / abort -> main loop
        v
   result to caller

 Allowed waits:  orchestrator -> main loop;  orchestrator -> Matter thread;  main loop -> Matter thread
 Forbidden:      Matter thread -> main loop  (never)
```

### Layering

```text
+--------------------------------------------------------------------+
| core/src  deviceService (main loop, resources, metadata, props)    |
+--------------------------------------------------------------------+
        ^ plugin calls (BARTON-430)
+--------------------------------------------------------------------+
| core/deviceDrivers/matter/sbmd/dal   (BARTON-421, this change)     |
|   Dal  ->  DalTransaction  ->  PluginBinding  <-  DataSourcePlugin |
|            DalPluginRegistry   DalExecutor -> ThreadHop (423)     |
|            AllocateSequence / StageConditional  (Atomic plugins)   |
+--------------------------------------------------------------------+
        ^ batched read / allocate / stage / commit / abort
+--------------------------------------------------------------------+
| SbmdDataAccess seam (423):  LegacySbmdDataAccess (423, interim)    |
|                             DalSbmdDataAccess    (421, this change)|
+--------------------------------------------------------------------+
        ^
+--------------------------------------------------------------------+
| SbmdTransactionOrchestrator / SbmdTransaction / SbmdWorker (423)   |
+--------------------------------------------------------------------+
        ^ request
+--------------------------------------------------------------------+
| SpecBasedMatterDeviceDriver / MatterDeviceDriver / QuickJS runtime |
+--------------------------------------------------------------------+
```

## Decisions

### D1: Terminology — SBMD Transaction vs DAL Transaction

**Decision:** Two distinct transaction concepts, named explicitly everywhere (see Glossary). An SBMD Transaction spans the whole driver request including parks and the failure handler; a DAL Transaction is the data-access unit it owns. Code types are `SbmdTransaction` and `DalTransaction`.

**Rationale:** Earlier drafts overloaded "transaction", which hid that device operations (commands, attribute writes) are not DAL writes and cannot be rolled back by the DAL.

**Alternative considered:** A single combined transaction type. Rejected — couples the DAL to SBMD and Matter, preventing reuse and making the commit semantics ambiguous.

---

### D2: Per-step cleanup hooks dropped

**Decision:** `Sbmd.result().onFailure(...)`, `CleanupRegistry`, and LIFO cleanup hooks are removed from the design. Replaced by the driver-level failure handler (D5).

**Rationale:** Per-step hooks need closure state captured at park time (the same staleness problem as BARTON-411) and multiply code paths. One handler with the full operation log is simpler and sees everything.

**Alternative considered:** LIFO hook stack. Rejected for the reasons above.

---

### D3: No automatic pre-park conflict detection or retry

**Decision:** No pre-park validation, no `kMaxPreParkRetries`, no `PrepareSideEffect`, no automatic re-run. Interleaving safety comes from the D11 atomic primitives plus D9 fresh reads.

**Rationale:** The original premise — that handler execution before the first device call is a single synchronous run — no longer holds strictly under the orchestrator model (D6): a step's supplement fetch, JS run, and commit are separate hops, so two concurrent SBMD Transactions on the same device can interleave before any park. Rather than detect and retry, drivers use sequences, merge writes, or conditional writes (D11) for data that can race, and fresh reads (D9) across parks.

**Alternative considered:** Optimistic validation with retry, or per-device serialization. Rejected — validation requires versions (D4); serialization reduces concurrency and can deadlock with parked operations. The team chose atomic DAL operations (D11).

---

### D4: No DAL data store, no versions

**Decision:** The DAL holds only the plugin registry and each open DAL Transaction's bindings, staged writes, and attribute overlay (D10), all cleared when the transaction ends. Steady state retains no data. Reads always go to source plugins, which own their caches. Dropped: global `DataStore`, `VersionedValue`, per-key versions, `IngressQueue`, `sbmdDataObserver` reconciliation, post-park `TryMerge`, `"conflict"` error type. Transient data is owned by a DAL plugin (it is the data, not a cache) and replaces `transientStore` (BARTON-430).

**Rationale:** A second copy of every value would need reconciliation with every writer in the system (attribute reports, public API writes, other subsystems). Reading from the source eliminates that entire class of bugs.

**Alternative considered:** Versioned authoritative store with ingress queue. Rejected — large surface, duplicated state, no benefit once conflicts are out of scope.

**Trade-offs:** Each step pays a read hop. Mitigated by batching (one hop per step, D6).

---

### D5: Driver-level failure handler

**Decision:** Optional `onTransactionFailure(args)` declared once per driver. `args.operations` is the ordered log `{ step, kind: "command"|"readAttribute"|"writeAttribute", endpoint, cluster, command|attribute, request, status: "succeeded"|"failed"|"notSent", response?, error? }`; `args.failure` is `{ step, type, message, matterCode? }` with types `timeout`, `commandFailed`, `readFailed`, `transport`, `internal`, `aborted`, `commitFailed`; `args.context` is the driver context; supplements are freshly fetched. It runs exactly once per SBMD Transaction that terminates in failure (see D12: a step whose `onError` recovers does not trigger it), after the failing step's `onError` (if any) and after the DAL Transaction's staged writes are discarded. It may return a result chain that runs as a new SBMD Transaction with failure handling disabled; that chain's failure is logged and reported. Covers device error, timeout, depth limit, pre-send failure (412), aborted, commitFailed. Detailed in BARTON-424.

**Rationale:** Door-lock example: SetUser ok, SetCredential ok, SetWeekDaySchedule fails — the handler reads user index and credential from the log and clears them. Only the driver knows the compensating commands.

**Alternative considered:** Automatic undo. Rejected — device operations have side effects and many are not invertible.

---

### D6: Threading — orchestrator model

**Decision:** The calling thread (the one that today blocks in `ConnectAndExecute`) is the orchestrator and owns the SBMD Transaction and its deadline (`SbmdTransactionOrchestrator`). It schedules JS execution and device operations on the Matter thread and DAL calls on the main loop. JS never calls the DAL; scheduled Matter work never calls the DAL. Per step: (1) fetch all supplements from the DAL in one batched call, including attributes and any declared sequence allocations (D11); (2) run the JS handler on the Matter thread; (3) stage returned writes in the DAL Transaction; (4) send any device operation on the Matter thread, with its response handed back to the orchestrator; (5) repeat with fresh supplements; (6) commit on success, or abort and run the failure handler on failure. Wait direction: orchestrator → main loop → Matter thread is allowed; the Matter thread never waits on the main loop. If the orchestrator is the main loop (`g_main_context_is_owner()`), DAL calls run inline. Work with no waiting caller (attribute-report mappers, event handlers, unsolicited data) runs on a dedicated `SbmdWorker` thread acting as orchestrator. All waits have deadlines. Detailed in BARTON-423; the DAL side (main-loop hop, inline detection, affinity hops, deadlines) is in this change.

**Rationale:** A one-directional wait graph cannot deadlock. Deadlines convert a violated rule into a logged transaction failure instead of a hang.

**Alternative considered:** Run the DAL on the Matter thread. Rejected — resource, metadata, and property sources are main-loop/deviceService-owned, and blocking the Matter thread on them stalls the CHIP stack.

---

### D7: Plugin system

**Decision:** `DalPluginRegistry` on the main loop; plugins implement `DataSourcePlugin` with `Scope()`, `Caps()`, `Affinity()`, and `Bind(DalTransactionId)` → `PluginBinding` (`Read(keys)` batched, `Commit(writes)`, `Release()`). `DataKey` = namespace + device ID + path; namespaces `Attribute`, `Resource`, `Metadata`, `Property`, `Transient`. `Register`/`Unregister` callable from any thread, marshaled to the main loop. The registerer unregisters; the DAL never guesses plugin lifetimes. A DAL Transaction binds per plugin on first touch; bindings are released at transaction end. Unregistering a plugin aborts every transaction bound to it with failure type `aborted`.

Plugins (BARTON-430): device-service properties, device resources + metadata, and transient data (service lifetime); Matter attributes (one per device, registered by the Matter driver on `MatterDevice` add and unregistered on removal, read-only, `MatterThread` affinity).

```cpp
// core/deviceDrivers/matter/sbmd/dal/DataKey.h
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <tuple>

namespace barton::sbmd::dal
{
    enum class Namespace : uint8_t
    {
        Attribute,
        Resource,
        Metadata,
        Property,
        Transient,
    };

    const char *ToString(Namespace ns);

    /**
     * Identifies one datum. deviceId is empty for device-independent namespaces (Property).
     * Path is namespace-specific (e.g. "1/0x0006/0x0000" for Attribute, "1/isOn" for Resource,
     * "sbmd.sessions" for Metadata, "sessions" for Transient).
     */
    struct DataKey
    {
        Namespace ns;
        std::string deviceId;
        std::string path;

        bool operator==(const DataKey &o) const { return ns == o.ns && deviceId == o.deviceId && path == o.path; }
        bool operator<(const DataKey &o) const
        {
            return std::tie(ns, deviceId, path) < std::tie(o.ns, o.deviceId, o.path);
        }
    };

    struct DataKeyHash
    {
        size_t operator()(const DataKey &k) const noexcept
        {
            size_t h = std::hash<std::string> {}(k.deviceId);
            h ^= std::hash<std::string> {}(k.path) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
            return h ^ static_cast<size_t>(k.ns);
        }
    };
} // namespace barton::sbmd::dal
```

```cpp
// core/deviceDrivers/matter/sbmd/dal/DalTypes.h
#pragma once

#include "DataKey.h"

#include <chrono>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace barton::sbmd::dal
{
    using DalTransactionId = uint64_t;

    /** A value as the SBMD runtime sees it: string-serialized (JSON for structured data). */
    struct DataValue
    {
        std::string value;
        /** Optional TTL for writes into the Transient namespace. Ignored elsewhere. */
        std::optional<std::chrono::seconds> ttl;
    };

    /** Absent keys map to std::nullopt. */
    using ReadResult = std::map<DataKey, std::optional<DataValue>>;

    /** A staged write. std::nullopt value means delete. */
    struct Write
    {
        DataKey key;
        std::optional<DataValue> value;
        /**
         * D11 conditional write: when set, the write applies only if the key's current value equals
         * *expected at commit (inner nullopt = "key must be absent"). Requires an Atomic plugin.
         */
        std::optional<std::optional<std::string>> expected;
    };

    enum class DalFailureType
    {
        Aborted,      // plugin unregistered, transaction aborted, shutdown
        CommitFailed, // a plugin commit failed or a condition did not hold (rollback attempted)
        NoPlugin,     // no registered plugin serves the key
        NotWritable,  // write to a read-only plugin
        NotAtomic,    // AllocateSequence / StageConditional on a plugin without the Atomic capability
        Timeout,      // executor hop exceeded its deadline
        Internal,
    };

    /** Detail for CommitFailed. */
    enum class CommitFailureReason
    {
        None,
        PluginError,     // a plugin's Commit returned an error
        ConditionFailed, // a D11 conditional write's expected value did not match
        RollbackFailed,  // a D8 rollback write also failed
    };

    struct DalError
    {
        DalFailureType type;
        std::string message;
        CommitFailureReason reason = CommitFailureReason::None;
    };
} // namespace barton::sbmd::dal
```

```cpp
// core/deviceDrivers/matter/sbmd/dal/DataSourcePlugin.h
#pragma once

#include "DalTypes.h"

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace barton::sbmd::dal
{
    enum class Executor
    {
        MainLoop,
        MatterThread,
    };

    struct PluginScope
    {
        Namespace ns;
        /** Set for per-device plugins (Attribute); empty serves every device in ns. */
        std::optional<std::string> deviceId;

        bool Matches(const DataKey &k) const { return k.ns == ns && (!deviceId || *deviceId == k.deviceId); }
    };

    struct PluginCaps
    {
        bool readable = true;
        bool writable = false;
        /** D11: supports AllocateSequence and atomic check-and-apply of conditional writes. */
        bool atomic = false;
    };

    /**
     * Per-DAL-Transaction plugin state. All calls are made on the plugin's Affinity() executor,
     * serially, by the DAL. Release() is called exactly once, after which no other call is made.
     */
    class PluginBinding
    {
    public:
        virtual ~PluginBinding() = default;

        /** Batched read. Every requested key must appear in the result. */
        virtual std::variant<ReadResult, DalError> Read(const std::vector<DataKey> &keys) = 0;

        /**
         * Apply writes atomically from the plugin's point of view where possible. Writable plugins only.
         * Atomic plugins MUST check every Write::expected against the current value before applying any
         * write in the batch and, on mismatch, apply nothing and return CommitFailed/ConditionFailed.
         */
        virtual std::optional<DalError> Commit(const std::vector<Write> &writes)
        {
            return DalError {DalFailureType::NotWritable, "plugin is read-only"};
        }

        /**
         * D11: atomically increment the integer counter at key (absent = 0) and return the new value.
         * Applied immediately; never rolled back. Atomic plugins only.
         */
        virtual std::variant<int64_t, DalError> AllocateSequence(const DataKey &key)
        {
            return DalError {DalFailureType::NotAtomic, "plugin does not support sequences"};
        }

        virtual void Release() {}
    };

    class DataSourcePlugin
    {
    public:
        virtual ~DataSourcePlugin() = default;

        virtual std::string Name() const = 0;
        virtual PluginScope Scope() const = 0;
        virtual PluginCaps Caps() const = 0;
        virtual Executor Affinity() const = 0;

        /** Called on Affinity() executor on a transaction's first touch of this plugin. */
        virtual std::unique_ptr<PluginBinding> Bind(DalTransactionId txn) = 0;
    };
} // namespace barton::sbmd::dal
```

**Alternative considered:** Static compile-time source list. Rejected — per-device Matter plugins come and go at runtime, and lifetime ownership must be explicit.

---

### D8: Commit across plugins — best effort with light rollback

**Decision:** At commit, the DAL groups staged writes by plugin, reads the current value of every key it will write through the same plugin read path, applies writes in deterministic plugin order (namespace order, then device ID, then plugin name), and if a later plugin's commit fails, writes the prior values back to already-committed plugins in reverse order. Exact because all commits run on the main loop. If rollback fails: log, report `CommitFailed`; the SBMD layer runs the failure handler. No two-phase commit.

**Alternative considered:** Two-phase commit. Rejected for now — no plugin needs it, and it requires prepare support in every source. Listed as an open question.

**Trade-offs:** Another thread writing the same key between commit and rollback could be overwritten by the rollback. Window is tiny and all DAL commits are serialized on the main loop; non-DAL writers to the same key are rare.

---

### D9: Fresh reads + per-key writes (driver API)

**Decision:** Every step, including deferred `onResponse`/`onError` and the failure handler, receives supplements fetched at that step's time. Default inherits the original handler's declared supplements; a `supplements` option on the deferred request may override. `storage.mergeTransientData(name, patch, ttl)` updates per member with RFC 7396 merge patch (verify `cJSONUtils_MergePatch` in bundled cJSON). Writes staged before an `error` terminal are discarded (DAL Transaction abort); `log` ops still run. Detailed in BARTON-422.

**Rationale:** Fresh reads remove the stale snapshot; per-member merge removes the read-modify-write.

---

### D10: Attribute writes are device operations

**Decision:** Attribute plugins are read-only. A `writeAttribute` is a device operation recorded in the SBMD Transaction's operation log, takes effect on device acceptance, and is undone only by the driver's failure handler. Read-your-own-writes: when a `writeAttribute` succeeds, the orchestrator records the written value on the transaction's binding to that device's attribute plugin; later reads of that attribute in the same DAL Transaction return it. The overlay is discarded at transaction end; subscription reports remain the source of truth. The DAL core exposes a generic `DalTransaction::SetOverlay(key, value)` hook; the attribute plugin binding behavior is in BARTON-430.

**Alternative considered:** Automatic attribute undo. Rejected — side effects, write-only and fabric-scoped attributes, device-originated changes.

---

### D11: Atomic DAL operations

**Decision:** Under D6 a step's supplement fetch, JS run, and commit are separate hops, so two concurrent SBMD Transactions on the same device can interleave before any park (for example, two concurrent camera `createSession` calls both read `TD_NEXT_SESSION_ID` and the second overwrites the first session). The DAL provides two atomic primitives, executed by the orchestrator on the main loop because JS never calls the DAL synchronously:

- **Sequence allocation** — `DalTransaction::AllocateSequence(key)`. A handler declares e.g. `{ sequence: "TD_NEXT_SESSION_ID" }` in its supplements (JS exposure in BARTON-422); during the batched supplement fetch the DAL routes the key to an `Atomic` plugin, which atomically increments the counter and returns the new value. It is applied immediately, bypasses staging, and is NOT rolled back on abort or failed commit (gaps are acceptable). A staged write to the same key in the same transaction is allowed and follows normal commit rules.
- **Conditional write (compare-and-set)** — `DalTransaction::StageConditional(key, expected, value)`. Stages a write carrying an expected current value (`nullopt` = must be absent). At commit the owning `Atomic` plugin checks all conditions in its batch before applying any write. A mismatch fails the commit with `CommitFailed` reason `ConditionFailed`, triggers the normal D8 rollback of already-committed plugins, and the SBMD layer reports `args.failure.type = "commitFailed"` (reason `conditionFailed`) and runs the failure handler. JS exposure in BARTON-422 (e.g. `storage.setTransientDataIf(name, expected, value, ttl)`).

Both primitives require the owning plugin to declare `PluginCaps::atomic`; otherwise they fail with `NotAtomic`. Because all commits and sequence allocations for main-loop plugins run on the main loop, atomicity is by confinement. Per-key merge writes (`storage.mergeTransientData`, D9) remain the default for multi-member objects.

**Limitation:** A driver that performs an unconditional read-compute-write of the same key can still lose an update. Driver authoring guidance (BARTON-422/425 docs) directs such drivers to sequences, merges, or conditional writes.

**Alternative considered:** Per-device serialization of SBMD Transactions. Rejected — reduces concurrency and can deadlock or stall behind parked operations. Versions with conflict detection — rejected by D4.

**Trade-offs:** Sequence gaps on abort; conditional-write failures surface as transaction failures the driver must handle.

---

### D12: Recovery rule

**Decision:** If a step's `onError` returns `success()` (or any non-error terminal or continuation), the driver has recovered: the SBMD Transaction continues or succeeds and `onTransactionFailure` does NOT run. `onTransactionFailure` runs only when the SBMD Transaction terminates in failure. The camera migration (BARTON-425) changes its `onError` handlers to return `error()` where a real failure must be reported. Detailed in BARTON-424.

**Alternative considered:** Always run the failure handler after any step error. Rejected — would compensate for failures the driver already handled.

---

### D13: Delivery order and interim data seam

**Decision:** Delivery order is **412 fix → 423 → 421 → 430 → 422 → 424 → 425**. The orchestrator (BARTON-423) lands first on the existing data paths through an internal seam, `SbmdDataAccess` (batched supplement fetch, stage, commit, abort, sequence allocation stub, attribute-write overlay hook), implemented by `LegacySbmdDataAccess`, which wraps today's fetchers/setters and `transientStore` and is invoked from the orchestrator with correct thread affinity (`transientStore` and attribute-cache reads scheduled onto the Matter thread; device-service calls on the orchestrator/main loop). Staged writes are held in the `SbmdTransaction` and applied at commit by the legacy implementation. BARTON-421 (this change) adds `DalSbmdDataAccess`, a DAL-backed implementation of that seam. BARTON-430 adds the plugins, switches the orchestrator to `DalSbmdDataAccess`, and deletes `transientStore` and `LegacySbmdDataAccess` entirely (no fallback). No interim locking (e.g. a mutex on the transient store) is ever needed. The `sbmd-v4-runtime` delta archive order stays 423 → 422 → 424.

**Alternative considered:** DAL first (421 → 430 → 423). Rejected — the plugins would have to be usable from today's Matter-thread call sites, forcing an interim mutex or a forbidden Matter → main-loop wait.

**Trade-offs:** The seam is an extra abstraction; it also isolates the orchestrator from the DAL for testing.

## Data Storage

None persisted. In-memory, main-loop-confined state only:

- `DalPluginRegistry`: `std::vector<std::shared_ptr<DataSourcePlugin>>` plus, per plugin, the set of open transaction IDs bound to it.
- `DalTransaction` (per open transaction): bindings by plugin, staged writes (`std::map<DataKey, Write>`, last write wins per key, each optionally carrying a D11 expected value), overlay (`std::map<DataKey, DataValue>`), state (`Open`, `Committed`, `Aborted`), abort reason.

All of it is cleared when the transaction ends. Sequence counters (D11) are owned by the `Atomic` plugin that serves the key, not by the DAL.

## Data Structures

Input: `DataKey`, `std::vector<DataKey>` (batched read and batched sequence allocation), `Write` (stage; `expected` set for conditional writes), `DataValue` (overlay). Output: `ReadResult`, `std::map<DataKey, int64_t>` (allocated sequence values), `DalError` (with `CommitFailureReason`), `std::optional<DalError>` for commit. See D7 code.

## Interfaces

No user-facing interfaces (no public API, CLI, or GObject changes). Internal C++ interface:

```cpp
// core/deviceDrivers/matter/sbmd/dal/Dal.h
#pragma once

#include "DalPluginRegistry.h"
#include "DalTransaction.h"

#include <chrono>
#include <memory>
#include <variant>

namespace barton::sbmd::dal
{
    /**
     * Thread-safe façade. Every method may be called from any thread except the Matter thread
     * (wait-direction rule). Calls are marshaled to the device-service main loop, or run inline
     * when the caller owns it. Each call waits at most `deadline`.
     */
    class Dal
    {
    public:
        explicit Dal(std::shared_ptr<barton::ThreadHop> hop); // same hop instance as the orchestrator

        DalPluginRegistry &Registry();

        std::variant<DalTransactionId, DalError> Begin(std::chrono::milliseconds deadline);
        std::variant<ReadResult, DalError>
        Read(DalTransactionId txn, const std::vector<DataKey> &keys, std::chrono::milliseconds deadline);
        /** D11: batched; each key allocated immediately and never rolled back. */
        std::variant<std::map<DataKey, int64_t>, DalError>
        AllocateSequences(DalTransactionId txn, const std::vector<DataKey> &keys, std::chrono::milliseconds deadline);
        /** Writes with Write::expected set are conditional (D11). */
        std::optional<DalError> Stage(DalTransactionId txn, std::vector<Write> writes, std::chrono::milliseconds deadline);
        std::optional<DalError> SetOverlay(DalTransactionId txn, DataKey key, DataValue value, std::chrono::milliseconds deadline);
        std::optional<DalError> Commit(DalTransactionId txn, std::chrono::milliseconds deadline);
        void Abort(DalTransactionId txn, std::chrono::milliseconds deadline);

    private:
        std::shared_ptr<DalExecutor> executor;
        DalPluginRegistry registry;
        std::map<DalTransactionId, std::unique_ptr<DalTransaction>> open; // main loop only
        DalTransactionId nextId = 1;                                       // main loop only
    };
} // namespace barton::sbmd::dal
```

## Implementation Detail

### `DalExecutor` (adapter over BARTON-423 `ThreadHop`)

The DAL does not define its own threading primitive. BARTON-423 lands first and provides `barton::ThreadHop` / `DefaultThreadHop` (`core/deviceDrivers/matter/sbmd/txn/ThreadHop.h`): `IsMatterThread()`, `IsMainLoopOwner()`, `OnMainLoop(deadline, fn)` (inline when owner, otherwise `g_main_context_invoke_full` + `wait_until`), `OnMatter(deadline, fn)` (`PlatformMgr().ScheduleWork` + `wait_until`), and abandoned-state handling for late closures. `DalExecutor` is a thin, non-virtual adapter that maps a plugin's `Executor` affinity onto those calls and converts `ThreadHop`'s assertion on Matter-thread waits into a logged fail-fast error, so a DAL contract violation never aborts the process in release builds.

```cpp
// core/deviceDrivers/matter/sbmd/dal/DalExecutor.h
#pragma once

#include "DataSourcePlugin.h"
#include "../txn/ThreadHop.h"

#include <chrono>
#include <functional>
#include <memory>
#include <optional>

namespace barton::sbmd::dal
{
    /** Maps plugin Executor affinity onto barton::ThreadHop (BARTON-423). Stateless apart from the hop. */
    class DalExecutor
    {
    public:
        using Deadline = barton::ThreadHop::Deadline;

        explicit DalExecutor(std::shared_ptr<barton::ThreadHop> hop) : hop(std::move(hop)) {}

        bool IsCurrent(Executor e) const
        {
            return e == Executor::MainLoop ? hop->IsMainLoopOwner() : hop->IsMatterThread();
        }

        /**
         * Run fn on executor e and wait until deadline. Inline when already on e. Returns nullopt on
         * deadline miss or on a forbidden wait (main-loop work requested from the Matter thread), which
         * is logged and never blocks.
         */
        template<typename F>
        auto Run(Executor e, Deadline deadline, F &&fn) -> std::optional<decltype(fn())>
        {
            if (e == Executor::MainLoop)
            {
                if (!hop->IsMainLoopOwner() && hop->IsMatterThread())
                {
                    icError("DAL: Matter thread must never wait on the main loop");
                    return std::nullopt;
                }
                return hop->OnMainLoop(deadline, std::forward<F>(fn));
            }

            if (hop->IsMatterThread())
            {
                return fn();
            }
            return hop->OnMatter(deadline, std::forward<F>(fn));
        }

        /** Non-blocking post to the main loop (used by Unregister and purge from the Matter thread). */
        void PostMainLoop(std::function<void()> fn) { hop->PostMainLoopAsync(std::move(fn)); }

        barton::ThreadHop &Hop() { return *hop; }

    private:
        std::shared_ptr<barton::ThreadHop> hop;
    };
} // namespace barton::sbmd::dal
```

Requirements on 423's `ThreadHop`: `PostMainLoop` is currently `protected`; the DAL needs a public non-blocking main-loop post. This change adds `void PostMainLoopAsync(std::function<void()> fn)` to `ThreadHop` (a public forwarder to the protected `PostMainLoop`), with no behavior change for 423 callers. The DAL façade takes the same `std::shared_ptr<barton::ThreadHop>` instance the orchestrator uses, so tests can inject one fake hop for both. Matter-thread posting is already provided by `DefaultThreadHop`; no separate Matter hook installation is needed by 430.

Deadlines: DAL entry points accept `std::chrono::milliseconds` budgets and convert them once to `ThreadHop::Deadline` (`steady_clock::now() + budget`); nested hops (main loop → Matter thread for an attribute plugin) share the caller's absolute deadline.

**Alternative considered:** A parallel `GlibDalExecutor` with its own Matter hooks. Rejected — duplicates 423's hop logic, abandoned-state handling, and test fakes.

### `DalPluginRegistry`

```cpp
// core/deviceDrivers/matter/sbmd/dal/DalPluginRegistry.h
#pragma once

#include "DataSourcePlugin.h"

#include <functional>
#include <map>
#include <memory>
#include <set>

namespace barton::sbmd::dal
{
    class DalPluginRegistry
    {
    public:
        using AbortFn = std::function<void(DalTransactionId, DalError)>;

        explicit DalPluginRegistry(std::shared_ptr<DalExecutor> executor, AbortFn onAbort);

        /** Any thread. Marshaled to the main loop. Rejects a second plugin with an overlapping scope. */
        void Register(std::shared_ptr<DataSourcePlugin> plugin);

        /**
         * Any thread except the Matter thread when blocking is requested. Marshaled to the main loop.
         * Aborts every transaction bound to the plugin with DalFailureType::Aborted, releases those
         * bindings, then removes the plugin. Idempotent.
         */
        void Unregister(const std::shared_ptr<DataSourcePlugin> &plugin);

        /** Main loop only. Most specific match wins (device-scoped over namespace-wide). */
        std::shared_ptr<DataSourcePlugin> Resolve(const DataKey &key) const;

        /** Main loop only. */
        void NoteBound(const DataSourcePlugin *plugin, DalTransactionId txn);
        void NoteReleased(const DataSourcePlugin *plugin, DalTransactionId txn);

    private:
        std::shared_ptr<DalExecutor> executor;
        AbortFn onAbort;
        std::vector<std::shared_ptr<DataSourcePlugin>> plugins;
        std::map<const DataSourcePlugin *, std::set<DalTransactionId>> boundTxns;
    };
} // namespace barton::sbmd::dal
```

`Unregister` posts (non-blocking) when called from the Matter thread, so the Matter driver may unregister from its device-removal path without violating the wait rule. Any read routed to an unregistered plugin after posting but before processing still succeeds against the plugin, which the registerer keeps alive via `shared_ptr` until the registry drops it.

### `DalTransaction`

```cpp
// core/deviceDrivers/matter/sbmd/dal/DalTransaction.h
#pragma once

#include "DalExecutor.h"
#include "DalPluginRegistry.h"

#include <map>
#include <memory>

namespace barton::sbmd::dal
{
    /** Main loop only. */
    class DalTransaction
    {
    public:
        enum class State { Open, Committed, Aborted };

        DalTransaction(DalTransactionId id, DalPluginRegistry &registry, DalExecutor &executor);
        ~DalTransaction(); // releases bindings if still held

        DalTransactionId Id() const { return id; }
        State GetState() const { return state; }

        /**
         * Resolution order per key: staged write, overlay, plugin. Keys are grouped by plugin;
         * each plugin gets one Read call on its affinity executor.
         */
        std::variant<ReadResult, DalError> Read(const std::vector<DataKey> &keys, std::chrono::milliseconds deadline);

        /** Validates a writable plugin serves each key (binding it), then stages. Last write wins. */
        std::optional<DalError> Stage(const std::vector<Write> &writes);

        /**
         * D11: stage a compare-and-set write. expected == nullopt means "key must be absent".
         * Fails with NotAtomic unless the owning plugin declares PluginCaps::atomic.
         * Read-your-own-staged-writes returns `value` for later reads in this transaction.
         */
        std::optional<DalError> StageConditional(const DataKey &key,
                                                 std::optional<std::string> expected,
                                                 std::optional<DataValue> value);

        /**
         * D11: atomically increment the counter at key in its Atomic plugin and return the new value.
         * Applied immediately, bypassing staging; not rolled back by Abort or a failed Commit.
         */
        std::variant<int64_t, DalError> AllocateSequence(const DataKey &key, std::chrono::milliseconds deadline);

        void SetOverlay(const DataKey &key, DataValue value);

        /** D8: prior-value capture, ordered apply, reverse rollback. Ends the transaction. */
        std::optional<DalError> Commit(std::chrono::milliseconds deadline);

        /** Discards staged writes and overlay, releases bindings. Idempotent. */
        void Abort(DalError reason);

    private:
        PluginBinding *BindingFor(const std::shared_ptr<DataSourcePlugin> &plugin, std::chrono::milliseconds deadline);
        void ReleaseAll();

        DalTransactionId id;
        DalPluginRegistry &registry;
        DalExecutor &executor;
        State state = State::Open;
        std::map<std::shared_ptr<DataSourcePlugin>, std::unique_ptr<PluginBinding>> bindings;
        std::map<DataKey, Write> staged;
        std::map<DataKey, DataValue> overlay;
    };
} // namespace barton::sbmd::dal
```

Commit algorithm (main loop):

```text
groups = staged grouped by plugin, sorted by (ns, deviceId, plugin->Name())
priors = {}
for g in groups: priors[g] = binding(g).Read(keys(g))          # affinity hop if needed
   on error: Abort(CommitFailed/PluginError); return it
done = []
for g in groups:
   err = binding(g).Commit(writes(g))     # Atomic plugin checks every expected value first;
                                          # mismatch -> applies nothing, returns ConditionFailed
   if err:
       for d in reverse(done): binding(d).Commit(priorsAsWrites(d))  # unconditional; nullopt prior -> delete
           on error: log; reason = RollbackFailed
       Abort(CommitFailed/err.reason); return CommitFailed with reason
   done.push(g)
state = Committed; ReleaseAll(); return nullopt
```

The condition is checked by the plugin inside its own `Commit`, not by the DAL comparing against `priors`, so the check and the apply are one atomic step on the plugin's executor. Sequence allocations made earlier in the transaction are not touched by commit failure or abort.

Any operation on a non-`Open` transaction returns `Aborted` (or the recorded reason).

### `DalSbmdDataAccess`

DAL-backed implementation of the `SbmdDataAccess` / `SbmdDataSession` seam defined by BARTON-423 (`core/deviceDrivers/matter/sbmd/txn/SbmdDataAccess.h`; this design does not redefine the seam, it implements whatever signature 423 lands — one `SbmdDataSession` wraps one DAL Transaction). Mapping:

| 423 seam (`barton::SbmdDataAccess` / `SbmdDataSession`) | `DalSbmdDataAccess` / `DalSbmdDataSession` (this change) |
| --- | --- |
| `SbmdDataAccess::Begin(deviceUuid)` | `Dal::Begin`; returns a `DalSbmdDataSession` owning exactly one DAL Transaction |
| `SbmdDataSession::Fetch(request, deadline)` | build `DataKey`s for every read entry (attribute → `Attribute` path `"<ep>/0x<cluster>/0x<attr>"` already resolved by the orchestrator; resource `"<ep>/<res>"`; persistent → `Metadata` `"sbmd.<key>"`; transient → `Transient`) and every sequence-allocation entry; one `Dal::Read` plus one `Dal::AllocateSequences` on the same main-loop hop; map results back to `SbmdFetchResult` |
| `SbmdDataSession::Stage(writes)` | `updateResource` → `Resource`; `setMetadata` → `Metadata`; `setPersistentData` → `Metadata` `"sbmd.<key>"`; `setTransientData` → `Transient` with TTL; via `Dal::Stage` |
| `SbmdDataSession::StageConditional(write)` | `Dal::Stage` with `Write::expected` set (D11) |
| `SbmdDataSession::RecordAttributeWrite(ep, cluster, attr, tlvBase64)` | `Dal::SetOverlay` on the `Attribute` key |
| `SbmdDataSession::Commit(deadline)` | `Dal::Commit`; `CommitFailed` → `SbmdDataError{CommitFailed, reason}` with reason `"conditionFailed"`, `"pluginCommitFailed"`, or `"rollbackFailed"`; `Aborted` → aborted |
| `SbmdDataSession::Abort(reason)` | `Dal::Abort`; sequence allocations are not returned |
| `SbmdDataSession::SetAbortListener(listener)` | invoked when the DAL aborts the transaction (e.g. plugin unregistered) |

`log` ops are not DAL writes and stay with the orchestrator. In this change `DalSbmdDataAccess` is built and unit-tested against fake plugins and a fake orchestrator caller; the orchestrator keeps using `LegacySbmdDataAccess`. BARTON-430 registers the real plugins, switches the orchestrator to `DalSbmdDataAccess`, and deletes `LegacySbmdDataAccess` entirely.

## Migrations

No persistent migration. Creation order inside this change: `DataKey`/`DalTypes` → `DataSourcePlugin` → `DalExecutor` → `DalPluginRegistry` → `DalTransaction` (including D11 primitives) → `Dal` → `DalSbmdDataAccess`. This change depends on BARTON-423 (the `SbmdDataAccess` seam). Production code does not route through the DAL until BARTON-430 registers the plugins and switches the orchestrator from `LegacySbmdDataAccess` to `DalSbmdDataAccess`.

### Delivery order across the epic

```text
BARTON-412 fix (sbmd-deferred-failure-callbacks)
  -> BARTON-423 sbmd-transaction-orchestrator    (SbmdDataAccess seam + LegacySbmdDataAccess)
  -> BARTON-421 sbmd-dal-core                    (this change: DAL core, D11, DalSbmdDataAccess)
  -> BARTON-430 sbmd-dal-source-plugins          (plugins; switch to DalSbmdDataAccess; delete LegacySbmdDataAccess + transientStore)
  -> BARTON-422 sbmd-dal-driver-api
  -> BARTON-424 sbmd-transaction-failure-handler
  -> BARTON-425 sbmd-dal-driver-migration
```

`sbmd-v4-runtime` delta archive order stays 423 → 422 → 424.

### Backward compatibility

No public API, GObject signal/property, or GIR change. No SBMD schema change. No driver-visible behavior change in this change.

## Thread Safety

- All DAL state (`DalPluginRegistry`, open `DalTransaction`s) is confined to the device-service GLib main loop (`glibEvt` thread). No mutexes guard DAL state; confinement does.
- `Dal` façade methods marshal to the main loop through 423's `ThreadHop::OnMainLoop` (`g_main_context_invoke_full` + deadline wait), or run inline when `ThreadHop::IsMainLoopOwner()` is true. All DAL hops go through `DalExecutor`, an adapter over the shared `ThreadHop`.
- Plugin binding calls run on the plugin's affinity executor. `MainLoop` plugins are called inline. `MatterThread` plugins are reached by a main-loop → Matter-thread hop with deadline. This is the only cross-executor wait the DAL performs, and it is in the allowed direction.
- The Matter thread never waits on the main loop. `Dal` methods called on the Matter thread fail fast with `Internal` (logged). `Unregister` from the Matter thread posts without waiting.
- Timeouts: a hop that misses its deadline returns `Timeout`; late work captures only `shared_ptr`-owned state so it is safe to complete after the waiter has gone.
- Existing main-loop code that blocks on the Matter thread (`LockChipStack`, `RunOnMatterStack`) remains allowed by the rule; it affects only latency (open question).

## Testing Philosophy

### Unit — registry

Fake plugins with configurable scope, caps, and affinity. Verify registration, overlap rejection, most-specific resolution, unregister idempotence, and that unregister aborts exactly the bound transactions with `Aborted` and calls `Release()` once per binding.

### Unit — transaction reads and staging

Verify one `Read` call per plugin per batch, resolution order (staged → overlay → plugin), absent keys as `nullopt`, `NoPlugin` and `NotWritable` errors, last-write-wins staging, and that bindings are created lazily on first touch.

### Unit — commit and rollback

Two and three writable fakes; inject a commit failure in the second and verify prior values (including deletes for previously absent keys) are written back to the first in reverse order, the result is `CommitFailed`, and rollback failure is logged. Verify deterministic ordering.

### Unit — atomic primitives

An `Atomic` fake plugin. Verify `AllocateSequence` increments from absent (1, 2, …), applies immediately, survives `Abort` and a failed commit, and fails with `NotAtomic` on a non-atomic plugin. Verify `StageConditional` succeeds when the expected value (or absence) matches, fails the whole commit with `CommitFailed`/`ConditionFailed` on mismatch with nothing applied in that plugin, and triggers D8 rollback of earlier plugins. Simulate two interleaved transactions doing the camera `createSession` pattern and verify distinct sequence values and that the second conditional write fails.

### Unit — `DalSbmdDataAccess`

Against fake plugins: supplement fetch maps every category to the right keys and back to the supplement shape in one read hop; each op type stages the right write; sequence and overlay hooks route correctly; commit failure reasons map to the `commitFailed`/`aborted` failure types expected by the orchestrator. Run the shared `SbmdDataAccess` contract test suite owned by BARTON-423 against `DalSbmdDataAccess` (fake plugins here; real plugins in 430). The same suite runs against `LegacySbmdDataAccess` in 423.

### Unit — executor

Use 423's `ThreadHop` test fake (or `DefaultThreadHop` with a real `GMainLoop` thread and a simulated Matter thread). Verify `DalExecutor` affinity mapping, inline execution when already on the target executor, deadline propagation across nested hops, fail-fast (logged, no assert, no block) when main-loop work is requested from the Matter thread, and non-blocking `PostMainLoop`.

### Integration

Deferred to BARTON-430, where real plugins are registered and the orchestrator (already delivered by BARTON-423) is switched to `DalSbmdDataAccess`. No Docker or hardware beyond the standard build container is required.

## Documentation Plan

### `docs/SBMD.md`

**Audience:** Driver and core developers

Add an "SBMD Data Access Layer" architecture subsection with the glossary, the orchestrator diagram, the wait-direction rule, the plugin model, commit semantics, the D11 atomic primitives (sequences, conditional writes) with the read-compute-write limitation, and the D12 recovery rule. Driver-facing API changes are documented by BARTON-422/424.

## Open Questions

- Thread-safety contract of `DeviceDataCache::GetAttributeData` — assumed Matter-thread-only; the attribute plugin uses `MatterThread` affinity accordingly.
- Audit main-loop code that blocks on the Matter thread (`LockChipStack`, `RunOnMatterStack`): allowed by the wait rule, but adds latency to DAL calls; document findings.
- `SbmdWorker`: single thread vs GLib thread pool; per-device ordering guarantees.
- Availability of `cJSONUtils_MergePatch` in the bundled cJSON (needed by BARTON-422).
- matter.js camera mock feasibility for BARTON-425 integration tests (C++ tests are the fallback).
- Whether a future two-phase commit is ever needed.
- Whether any non-transient namespace needs `Atomic` support.

Resolved: sequence counters are decimal `int64_t` strings in the `Transient` namespace with no expiry and are lost on restart (accepted; session IDs are transient too). `SbmdDataAccess` contract tests are owned by BARTON-423 and run against both implementations. `LegacySbmdDataAccess` is deleted entirely by BARTON-430 (no fallback). The DAL reuses 423's `ThreadHop` rather than a parallel executor.

## Risks / Trade-offs

### Main-loop latency

**Risk:** Every SBMD step hops to the main loop; a busy main loop delays device control.

**Mitigation:** One batched hop per step, inline execution when already on the main loop, and deadlines that turn stalls into logged failures. Measure in BARTON-423.

### Best-effort rollback

**Risk:** A rollback can fail or overwrite a concurrent non-DAL write.

**Mitigation:** All DAL commits are serialized on the main loop; rollback failure is reported as `commitFailed` and handed to the driver failure handler.

### Wait-rule violation

**Risk:** Future code calls the DAL from the Matter thread and deadlocks.

**Mitigation:** Fail-fast detection in `Dal` and `DalExecutor`, plus deadlines on every wait.

### Plugin lifetime bugs

**Risk:** A registerer forgets to unregister, leaving stale per-device plugins.

**Mitigation:** Explicit ownership rule (registerer unregisters) and registry diagnostics logging registered plugins at shutdown.

### Unconditional read-compute-write still loses updates

**Risk:** D11 primitives are opt-in; a driver that reads a key and writes a computed value unconditionally can still lose a concurrent update.

**Mitigation:** Driver authoring guidance in `docs/SBMD.md` and the BARTON-425 driver audit direct such code to sequences, merges, or conditional writes.

### Seam drift between 423 and 421

**Risk:** `DalSbmdDataAccess` must match the `SbmdDataAccess` seam exactly as 423 lands it; behavior differences from `LegacySbmdDataAccess` could surface only after the 430 switch.

**Mitigation:** Shared contract tests run against both implementations; 430 runs the full SBMD suite after switching.
