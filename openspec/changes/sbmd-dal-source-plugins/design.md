# Design: SBMD DAL source plugins (BARTON-426)

Concrete `DataSourcePlugin` implementations for the store-less SBMD Data Access Layer. Epic-wide architecture, glossary (SBMD Transaction vs DAL Transaction), and decisions D1–D13 are in `openspec/changes/sbmd-dal-core/design.md`; this document references them by number.

## Context

Delivery order is 412 fix → 423 → 421 → 426 → 422 → 424 → 425 (epic D13). When this change lands, BARTON-423 has delivered the orchestrator, which performs all SBMD data access through the `SbmdDataAccess` seam implemented by `LegacySbmdDataAccess` (wrapping the backends below and scheduling `transientStore` and attribute-cache reads onto the Matter thread). BARTON-421 has delivered the DAL core — routing batched reads, commits, sequence allocation, and conditional writes to plugins registered in `DalPluginRegistry` on the device-service main loop — and `DalSbmdDataAccess`, but no plugins. This change supplies the plugins, switches the orchestrator to `DalSbmdDataAccess`, and deletes `transientStore` and `LegacySbmdDataAccess` entirely.

The backends below describe today's code; after 423 they are reached via `LegacySbmdDataAccess`.

Current backends, as used by `core/deviceDrivers/matter/sbmd/SpecBasedMatterDeviceDriver.cpp` and `mquickjs/SbmdHandlerInvoker.cpp`:

| Supplement / op | Today | Notes |
| --- | --- | --- |
| Attribute supplement | `MakeAttrFetcher`: alias → cluster/attribute, `ResolveEndpoint`, `MatterDevice::GetCachedAttributeData` → `DeviceDataCache::GetAttributeData`, `EncodeTlvElementToBase64(reader, 256)` | Matter thread |
| Resource supplement | `MakeResFetcher`: path `"<ep>/<res>"` or `"<res>"` → `deviceServiceGetResourceById` | Result is never `resourceDestroy`ed (leak) |
| Persistent data | `MakePersistFetcher`: `deviceServiceGetMetadata("/devices/<uuid>/metadata/sbmd.<key>")`; write via `deviceServiceSetMetadata` in `ExecuteOps` | |
| Metadata op | `ResultOp::SetMetadata` in `ExecuteOps` | |
| Resource op | `updateResource(uuid, ep|NULL, res, value, meta)` in `ExecuteOps` | |
| Transient data | `transientStore[deviceUuid][key] = TransientEntry{value, steady_clock::now()+ttl}`; `GetTransientData` erases and returns `nullopt` when `now >= expiry` | Matter-thread-confined; no sweep; never purged on device removal |

Matter device lifetime: `MatterDeviceDriver::AddDevice` (overridden by `SpecBasedMatterDeviceDriver::AddDevice`, which calls the base) emplaces into `devices` under `devicesMutex`. `MatterDeviceDriver::DeviceRemoved` erases from `devices` inside `RunOnMatterSync`. `MatterDeviceDriver::Shutdown` (run via `RunOnMatterSync`) clears `devices`.

No `docs/dev/architecture.md` exists in the project.

## References

- `openspec/changes/sbmd-dal-core/design.md` — DAL core interfaces, D11 atomic primitives, `DalSbmdDataAccess`, and epic decisions D1–D13.
- `openspec/changes/sbmd-transaction-orchestrator/design.md` — `ThreadHop`, `SbmdDataAccess`/`SbmdDataSession` seam, `LegacySbmdDataAccess`, and the shared `SbmdDataAccess` contract test suite.
- `api/c/public/private/deviceService.h` — `deviceServiceGetSystemProperty`/`SetSystemProperty`, `deviceServiceGetMetadata`/`SetMetadata` contracts (caller frees).
- `api/c/public/private/deviceServicePrivate.h` — `deviceServiceGetResourceById` (caller `resourceDestroy`s) and `updateResource`.
- `core/src/subsystems/matter/DeviceDataCache.h` — `GetAttributeData` cached read path.
- [Matter SDK `PlatformManager::ScheduleWork`](https://github.com/project-chip/connectedhomeip/blob/master/src/include/platform/PlatformManager.h) — Posting work to the Matter thread.

## Goals / Non-Goals

**Goals:**

- Four plugins: properties, resources + metadata, transient, per-device Matter attribute.
- Transient plugin replaces `transientStore` with identical TTL semantics plus purge on device removal; main-loop-confined, no mutex; implements the `Atomic` capability (sequence allocation, conditional writes).
- Switch the orchestrator from `LegacySbmdDataAccess` to `DalSbmdDataAccess`; delete `transientStore` and `LegacySbmdDataAccess` entirely (no fallback).
- Attribute plugin returns exactly what attribute supplements return today; read-only; `MatterThread` affinity; read-your-own-writes overlay per D10.
- Explicit registerer-owned lifetimes; Matter-thread reads via 423's shared `ThreadHop` (through the DAL's `DalExecutor`).

**Non-Goals:**

- The orchestrator and `SbmdDataAccess` seam (BARTON-423); `DalSbmdDataAccess` itself (BARTON-421).
- JS exposure of sequences and conditional writes (BARTON-422).
- `Atomic` support in namespaces other than `Transient`.
- `storage.mergeTransientData` (BARTON-422).
- Attribute writes via the DAL (D10: device operations).

## Decisions

### D1: Resource and metadata share one plugin, separate scopes

**Decision:** `ResourceMetadataPlugin` is one class instantiated twice — once scoped to `Resource`, once to `Metadata` — so each namespace has its own registry entry while sharing the device-service glue. Both are `MainLoop`, readable, writable.

Paths: `Resource` path is `"<endpointId>/<resourceId>"` or `"<resourceId>"` (device-level), matching today's resource supplements. `Metadata` path is the metadata name; SBMD persistent key `k` maps to path `"sbmd.k"`.

**Alternative considered:** One plugin spanning two namespaces. Rejected — `PluginScope` is single-namespace by design.

---

### D2: Transient plugin owns the data, main-loop-confined, with the Atomic capability

**Decision:** `TransientPlugin` owns a `TransientDataStore` (per-device map of key → `{value, expiry}`), confined to the device-service main loop with no mutex: after the switch to `DalSbmdDataAccess`, the plugin binding (called by the DAL on the main loop) is the only accessor, and `LegacySbmdDataAccess` (including its Matter-thread transient paths) is deleted. Writes set `expiry = steady_clock::now() + ttl` at commit time; reads lazily evict expired entries and report them absent; deletes remove the key. No background sweep (as today). A write without a TTL is rejected (`Internal`), since the SBMD op always carries one.

The plugin declares `PluginCaps::atomic` (D11):

- **Sequence allocation** — `AllocateSequence(key)` parses the current value as a decimal `int64_t` (absent or expired = 0), stores `n + 1` as a decimal string, and returns it, immediately and without staging. The counter entry keeps no expiry (`TimePoint::max()`) unless a later write sets a TTL; a non-numeric existing value fails with `Internal`. Never rolled back.
- **Conditional writes** — `Commit(writes)` first evaluates every `Write::expected` in the batch against the current (lazily evicted) values; on any mismatch it applies nothing and returns `CommitFailed`/`ConditionFailed`. Otherwise it applies all writes. Because the binding runs on the main loop and all DAL commits are serialized there, check and apply are atomic.

```cpp
// core/deviceDrivers/matter/sbmd/dal/plugins/TransientPlugin.h
#pragma once

#include "../DataSourcePlugin.h"

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>

namespace barton::sbmd::dal
{
    /** Main loop only. Not thread-safe by design (D13: no interim locking). */
    class TransientDataStore
    {
    public:
        using Clock = std::chrono::steady_clock;
        using NowFn = std::function<Clock::time_point()>;

        explicit TransientDataStore(NowFn now = Clock::now);

        void Set(const std::string &deviceId, const std::string &key, std::string value, std::chrono::seconds ttl);
        /** Lazy eviction. remainingTtl is set when present. */
        std::optional<std::string> Get(const std::string &deviceId,
                                       const std::string &key,
                                       std::optional<std::chrono::seconds> *remainingTtl = nullptr);
        void Remove(const std::string &deviceId, const std::string &key);
        std::variant<int64_t, DalError> Increment(const std::string &deviceId, const std::string &key);
        void PurgeDevice(const std::string &deviceId);

    private:
        struct Entry
        {
            std::string value;
            Clock::time_point expiry;
        };

        NowFn now;
        std::unordered_map<std::string, std::unordered_map<std::string, Entry>> data;
    };

    class TransientPlugin : public DataSourcePlugin
    {
    public:
        explicit TransientPlugin(std::shared_ptr<TransientDataStore> store);

        std::string Name() const override { return "transient"; }
        PluginScope Scope() const override { return {Namespace::Transient, std::nullopt}; }
        PluginCaps Caps() const override { return {true, true, true}; }
        Executor Affinity() const override { return Executor::MainLoop; }
        std::unique_ptr<PluginBinding> Bind(DalTransactionId txn) override;

        /** Called by the SBMD driver's device-removal path (marshaled to the main loop). */
        void PurgeDevice(const std::string &deviceId);

    private:
        std::shared_ptr<TransientDataStore> store;
    };
} // namespace barton::sbmd::dal
```

D8 rollback of a transient key writes the prior value back with its remaining TTL: the binding's `Read` populates `DataValue::ttl` with `expiry - now` (minimum 1s), and the DAL's rollback writes it back unconditionally, so rollback does not extend lifetimes. Sequence counters are never part of rollback.

Device-removal purge runs on the main loop: the Matter driver's removal path (Matter thread) posts `PurgeDevice` to the main loop without waiting.

**Alternative considered:** Keep `transientStore` in the driver and wrap it. Rejected — D4 makes transient data owned by its plugin. A mutex-guarded store shared with Matter-thread callers — rejected by D13: 423 lands first, so no Matter-thread caller remains.

**Trade-offs:** Sequence counters live in the transient namespace without expiry and are lost on restart, like all transient data. This is accepted: the IDs they produce (e.g. camera session IDs) are transient too.

---

### D3: Per-device Matter attribute plugin

**Decision:** One `MatterAttributePlugin` per `MatterDevice`, scope `{Attribute, deviceId}`, `MatterThread` affinity, read-only. Path is `"<endpointId>/<clusterId>/<attributeId>"` in hex (`"1/0x0006/0x0000"`); alias and endpoint resolution stay in the SBMD layer (the orchestrator resolves aliases to paths before calling the DAL). `Read` calls `MatterDevice::GetCachedAttributeData` and returns base64 TLV via the existing `EncodeTlvElementToBase64(reader, 256)`; cache miss is absent. The plugin holds no `MatterDevice` pointer: it captures the device ID and looks up the `MatterDevice` through the driver at read time (on the Matter thread, under `devicesMutex`), returning absent if gone, so a read racing removal is safe.

Overlay (D10): `PluginBinding` for this plugin is plain; the overlay is the DAL core's generic per-transaction `SetOverlay`, which the orchestrator calls with the written value (base64 TLV) after a successful `writeAttribute`. Resolution order (staged → overlay → plugin) makes later reads in the same transaction return the written value. `Commit` returns `NotWritable`.

```cpp
// core/deviceDrivers/matter/sbmd/dal/plugins/MatterAttributePlugin.h
#pragma once

#include "../DataSourcePlugin.h"

#include <functional>
#include <optional>
#include <string>

namespace barton::sbmd::dal
{
    class MatterAttributePlugin : public DataSourcePlugin
    {
    public:
        /** Called on the Matter thread; returns base64 TLV or nullopt on miss / device gone. */
        using CachedReader =
            std::function<std::optional<std::string>(const std::string &deviceId, uint16_t ep, uint32_t cluster, uint32_t attr)>;

        MatterAttributePlugin(std::string deviceId, CachedReader reader);

        std::string Name() const override { return "matterAttribute:" + deviceId; }
        PluginScope Scope() const override { return {Namespace::Attribute, deviceId}; }
        PluginCaps Caps() const override { return {true, false}; }
        Executor Affinity() const override { return Executor::MatterThread; }
        std::unique_ptr<PluginBinding> Bind(DalTransactionId txn) override;

        static std::optional<std::tuple<uint16_t, uint32_t, uint32_t>> ParsePath(const std::string &path);

    private:
        std::string deviceId;
        CachedReader reader;
    };
} // namespace barton::sbmd::dal
```

Lifetime ownership (registerer unregisters):

- `MatterDeviceDriver::AddDevice`: after a successful `devices.emplace`, register `MatterAttributePlugin(deviceId, reader)` and remember it in a `std::map<std::string, std::shared_ptr<MatterAttributePlugin>> attributePlugins`.
- `MatterDeviceDriver::DeviceRemoved`: inside the existing `RunOnMatterSync` block, before `devices.erase`, unregister the plugin (non-blocking post from the Matter thread) and drop it from `attributePlugins`. Bound transactions are aborted with `aborted`. The SBMD driver override also posts `TransientPlugin::PurgeDevice` to the main loop (non-blocking).
- `MatterDeviceDriver::Shutdown`: unregister all remaining attribute plugins before `devices.clear()`.

**Alternative considered:** One namespace-wide attribute plugin looking devices up. Rejected — per-device registration gives a precise abort signal on removal (D7).

**Trade-offs:** Requires `DeviceDataCache::GetAttributeData` to be Matter-thread safe (open question); affinity guarantees that.

---

### D4: Properties plugin

**Decision:** `PropertyPlugin` scope `{Property, none}`, `MainLoop`, readable + writable, over `deviceServiceGetSystemProperty` / `deviceServiceSetSystemProperty`. Path is the property name. Delete is unsupported (`Internal`). No current SBMD supplement uses it; it exists so the orchestrator and future driver APIs have a uniform path.

**Alternative considered:** `BCorePropertyProvider` (`b_core_property_provider_get_property_as_string`). Open: confirm with the team which "device-service properties" are intended; system properties are chosen because they have a set API.

---

### D5: Registration helper; Matter-thread execution via `ThreadHop`

**Decision:** `SbmdDalPlugins` creates and registers the property, resource, metadata, and transient plugins when the first `SpecBasedMatterDeviceDriver` is created and unregisters them on SBMD shutdown (it is the registerer). No Matter executor hooks are installed: the DAL is constructed with the same `ThreadHop` instance the orchestrator uses (BARTON-423 `DefaultThreadHop`), whose `OnMatter` already posts via `PlatformMgr().ScheduleWork`. Non-blocking unregister and `TransientPlugin::PurgeDevice` from the Matter thread use `DalExecutor::PostMainLoop` (`ThreadHop::PostMainLoopAsync`, added by BARTON-421).

## Data Storage

Only `TransientDataStore` (in-memory, main-loop-confined, D2), which also holds sequence counters as decimal strings. No persistence.

## Data Structures

`DataKey` paths per namespace:

| Namespace | deviceId | Path | Value |
| --- | --- | --- | --- |
| `Attribute` | device UUID | `"<ep>/0x<cluster>/0x<attr>"` | base64 TLV |
| `Resource` | device UUID | `"<ep>/<resource>"` or `"<resource>"` | resource string |
| `Metadata` | device UUID | metadata name (`sbmd.<key>` for persistent data) | string |
| `Property` | empty | system property name | string |
| `Transient` | device UUID | key | string (+ TTL on write); sequence counters as decimal `int64_t` strings |

## Interfaces

No public API, CLI, GObject signal/property, or GIR change. Resource writes keep emitting the existing resource-updated events through `updateResource`.

## Implementation Detail

### Switch to `DalSbmdDataAccess`

`SbmdDalPlugins` registration happens before the orchestrator is constructed; the SBMD driver then configures the orchestrator with `DalSbmdDataAccess` (BARTON-421) instead of `LegacySbmdDataAccess` (BARTON-423). In the same change, delete `SpecBasedMatterDeviceDriver::transientStore`, `TransientEntry`, `SetTransientData`, `GetTransientData`, `MakeTransientFetcher`, `MakeTransientSetter`, and `LegacySbmdDataAccess` in its entirety (no fallback), together with its unit tests and any legacy fetchers it wrapped that are left without callers (`MakeAttrFetcher`, `MakeResFetcher`, `MakePersistFetcher`). The orchestrator's data-access configuration point keeps the `SbmdDataAccess` interface so tests can inject fakes.

### Device-service plugins

Resource read must free: `icDeviceResource *r = deviceServiceGetResourceById(...); ...; resourceDestroy(r);`. Metadata/property reads free the returned `char *`. Resource delete is unsupported (`Internal`). A resource or metadata commit that returns false fails the binding commit with `Internal`, triggering D8 rollback.

## Migrations

Requires BARTON-423 and BARTON-421 to have landed. Order: `TransientDataStore` + plugin (with `Atomic`) → resources/metadata/properties plugins → Matter attribute plugin + `AddDevice`/`DeviceRemoved`/`Shutdown` hooks → `SbmdDalPlugins` → switch orchestrator to `DalSbmdDataAccess` → delete `transientStore` and `LegacySbmdDataAccess` entirely. Backward compatible; no driver-visible change. Delivery order: 412 fix → 423 → 421 → 426 → 422 → 424 → 425.

## Thread Safety

- Property, resource/metadata, transient plugins: `MainLoop` affinity; called inline by the DAL on the main loop. Underlying device-service APIs are callable from the main loop.
- `TransientDataStore`: main-loop-confined, no mutex. After the switch, its only callers are the transient plugin binding (run by the DAL on the main loop) and `PurgeDevice` (posted to the main loop). No Matter-thread code accesses transient data. Atomicity of sequence allocation and conditional writes follows from main-loop confinement.
- Matter attribute plugin: `MatterThread` affinity; the DAL hops main loop → Matter thread (allowed). It never calls the DAL.
- Unregister from `DeviceRemoved`/`Shutdown` runs on the Matter thread and is posted without waiting (DAL core requirement).

## Testing Philosophy

### Transient plugin

Unit-test TTL with an injectable clock: value present before expiry, absent at and after expiry with lazy eviction, last write wins, delete, per-device purge, rollback with remaining TTL. Atomic capability: increment from absent and expired values, non-numeric counter rejected, increments survive abort; conditional writes match, mismatch (nothing applied in the batch), expected-absent. Port the existing transient tests to the store. A debug assertion verifies main-loop confinement.

### Switch-over

Run the SBMD unit and integration suites with the orchestrator on `DalSbmdDataAccess`; run the shared `SbmdDataAccess` contract test suite owned by BARTON-423 against it with real plugins (removing the suite's `LegacySbmdDataAccess` target); verify by build that no reference to `transientStore` or `LegacySbmdDataAccess` remains.

### Resource/metadata and property plugins

Unit tests against mocked `deviceService` functions (existing core test mocks): path parsing (device-level vs endpoint), freeing results, write failure mapped to `Internal`, `sbmd.` prefix mapping.

### Matter attribute plugin

Unit test path parsing and read via a fake `CachedReader`; verify read-only commit rejection and overlay precedence through a real `DalTransaction`. Integration test in the Matter test harness: register on `AddDevice`, read attribute through the DAL, remove the device and verify bound transactions abort with `aborted` and the plugin is gone.

### Regression

Existing SBMD unit and integration tests pass unchanged (requires Docker build container).

## Documentation Plan

### `docs/SBMD.md`

**Audience:** Driver and core developers

Document the plugin table (namespace, affinity, writability, atomic, lifetime owner), TTL semantics of transient data, sequence counters and conditional writes in the transient namespace, and that attribute writes are device operations with a read-your-own-writes overlay.

## Risks / Trade-offs

### `DeviceDataCache` thread safety

**Risk:** If `GetAttributeData` is not safe even on the Matter thread during cache updates, reads may race.

**Mitigation:** `MatterThread` affinity serializes with cache callbacks; confirm in the open question.

### Behavior drift at switch-over

**Risk:** `DalSbmdDataAccess` may differ subtly from `LegacySbmdDataAccess` (e.g. encoding, absent vs empty values), surfacing only when the orchestrator switches; with the legacy implementation deleted there is no fallback.

**Mitigation:** Shared `SbmdDataAccess` contract tests run against both implementations, plus the full SBMD regression suite in this change.

### Accidental off-loop access to transient data

**Risk:** Future code touches `TransientDataStore` from another thread now that it has no mutex.

**Mitigation:** The store is reachable only through the plugin; a debug assertion checks `g_main_context_is_owner(NULL)`.

### Missed unregister

**Risk:** A removal path not covered leaves a stale per-device plugin.

**Mitigation:** Hooks in both `DeviceRemoved` and `Shutdown` (the only paths that erase from `devices`); registry duplicate-scope rejection surfaces stale registrations on re-add.
