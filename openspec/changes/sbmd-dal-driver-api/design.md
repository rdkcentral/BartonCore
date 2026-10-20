# Design: SBMD DAL driver API — fresh deferred supplements and merge writes (BARTON-422, epic BARTON-420)

## Context

This change depends on BARTON-421. The DAL architecture, key namespaces, versioning, thread model, and open questions are defined in `openspec/changes/sbmd-dal-core-store/design.md` and are not repeated.

Today, a deferred callback is invoked from `SpecBasedMatterDeviceDriver::HandleDeferredCommandResponse` / `HandleDeferredCommandError` / `ExecuteReadAttribute` with args built by `SbmdHandlerInvoker::BuildCommandResponseArgs` / `BuildDeferredErrorArgs`. These never call `SbmdHandlerInvoker::FetchSupplements`, so `args.supplements` is absent and the only data a callback has is `args.handlerContext` (the `context` captured at park time). Transient writes are whole-value only (`ResultOp::SetTransientData { key, value, ttlSecs }`).

No `docs/dev/architecture.md` exists in the project.

## References

- `openspec/changes/sbmd-dal-core-store/design.md` — foundation architecture (D1–D9, thread safety, open questions).
- [RFC 7396 — JSON Merge Patch](https://www.rfc-editor.org/rfc/rfc7396) — semantics adopted for `mergeTransientData`.
- `docs/SBMD.md` §4.12, §5.1, §6.2, §7.2, §7.3 — current driver contract.
- [cJSON](https://github.com/DaveGamble/cJSON) — JSON library already used throughout BartonCore; its `cJSON_Utils` companion provides `cJSONUtils_MergePatch`.

## Goals / Non-Goals

**Goals:**

- Deferred callbacks see current data, read from the DAL at callback time.
- Drivers can update one member of a transient JSON object without rewriting the whole object.
- Intent-only API: no versions, no transaction scope in JS.

**Non-Goals:**

- Read-set validation or conflict handling (BARTON-423).
- Cleanup hooks (BARTON-424).
- Driver migration (BARTON-425).

## Architecture

```text
 driver JS                         SBMD runtime (Matter thread)                     DAL
 ---------                         ----------------------------                     ---
 handler(args{supplements@T0}) <-- FetchSupplements(decl) ------------------------> Read @T0
   returns requestCommand{opts.supplements?}
                                   PendingOperation.supplementDecl = opts ?? handler decl
                                   ... park; other ops/reports reconcile ---------> versions bump
 onResponse(args{supplements@T1}) <- FetchSupplements(pending.supplementDecl) -----> Read @T1 (fresh)
   returns mergeTransientData(k, patch)
                                   ExecuteOps -> DataAccessLayer::Merge ----------> apply patch to
                                                                                    current value @T1
```

## Decisions

### D1: Deferred callbacks receive supplements fetched at callback time

**Decision:** `PendingOperation` gains `SbmdSupplements supplementDecl`. In `ExecuteRequestCommand` / `ExecuteReadAttribute`, it is set from the terminal's `supplements` option if present, else from the originating handler's declaration (carried in `HandlerContext`/`OperationContext`). Before invoking `onResponse`/`onError`, the runtime calls `FetchSupplements` with fresh DAL-backed fetchers and passes the result to the args builders.

```cpp
// core/deviceDrivers/matter/sbmd/mquickjs/SbmdHandlerInvoker.h (changed signatures)
static SafeJSValue BuildCommandResponseArgs(JSContext *ctx,
                                            const HandlerContext &hctx,
                                            uint32_t clusterId,
                                            uint32_t commandId,
                                            const std::string &tlvBase64,
                                            JSValue context,
                                            const FetchedSupplements *supplements = nullptr);

static SafeJSValue BuildDeferredErrorArgs(JSContext *ctx,
                                          const HandlerContext &hctx,
                                          const char *type,
                                          const char *message,
                                          int matterCode,
                                          JSValue context,
                                          const FetchedSupplements *supplements = nullptr);
```

```cpp
// core/deviceDrivers/matter/sbmd/SpecBasedMatterDeviceDriver.cpp (HandleDeferredCommandResponse, excerpt)
FetchedSupplements fresh;
if (!pending.supplementDecl.IsEmpty())
{
    auto device = pending.device.lock();
    fresh = SbmdHandlerInvoker::FetchSupplements(pending.supplementDecl,
                                                 device ? MakeAttrFetcher(*device) : AttributeSupplementFetcher {},
                                                 MakeResFetcher(pending.handlerContext.deviceUuid),
                                                 MakePersistFetcher(pending.handlerContext.deviceUuid),
                                                 MakeTransientFetcher(pending.handlerContext.deviceUuid));
}
{
    auto lock = MQuickJsRuntime::Instance().AcquireMutex();
    auto *ctx = MQuickJsRuntime::Instance().GetSharedContext();
    SafeJSValue args = SbmdHandlerInvoker::BuildCommandResponseArgs(
        ctx, pending.handlerContext, path.mClusterId, path.mCommandId, tlvBase64, pending.context.Get(),
        pending.supplementDecl.IsEmpty() ? nullptr : &fresh);
    result = SbmdHandlerInvoker::InvokeHandler(ctx, pending.onResponse.Get(), args, &pending.operationCtx);
}
```

Fetching happens outside the JS mutex, matching the existing `FetchSupplements`/`AddSupplements` split.

**Alternative considered:** Require drivers to re-declare supplements on every deferred terminal. Rejected as the only option — verbose and easy to forget; kept as an override.

**Alternative considered:** Snapshot supplements at park time into `context`. That is today's bug.

**Trade-offs:** Extra DAL reads per callback; O(1) each.

---

### D2: `storage.mergeTransientData(name, patch, ttlSecs)` with RFC 7396 semantics

**Decision:** New non-terminal op. `patch` is a JS object serialized to JSON by the builder. The DAL reads the current transient value at op-execution time, parses it (non-object or absent → `{}`), applies the patch with `cJSONUtils_MergePatch` (cJSON_Utils, already part of the cJSON distribution), serializes, and writes a new immutable value with the given TTL. A `null` member deletes it. `ttlSecs == 0` deletes the whole key, matching `setTransientData`.

```js
// core/deviceDrivers/matter/sbmd/scriptCommon/sbmd-result.js (storage namespace addition)
mergeTransientData: function (name, patch, ttlSecs) {
    if (typeof name !== 'string' || patch === null || typeof patch !== 'object') {
        throw new TypeError('mergeTransientData(name: string, patch: object, ttlSecs: number)');
    }
    self._ops.push({op: 'mergeTransientData', key: name, patch: JSON.stringify(patch), ttlSecs: ttlSecs | 0});
    return self;
},
```

```cpp
// core/deviceDrivers/matter/sbmd/mquickjs/SbmdResultExecutor.h
struct MergeTransientData
{
    std::string key;
    std::string patchJson;
    uint32_t ttlSecs;
};
using Data = std::variant<UpdateResource, SetMetadata, SetPersistentData, SetTransientData, MergeTransientData, Log>;
```

```cpp
// core/deviceDrivers/matter/sbmd/dal/DataAccessLayer.cpp (new)
Version DataAccessLayer::MergeJson(const std::string &deviceUuid,
                                   const DataKey &key,
                                   const std::string &patchJson,
                                   std::optional<std::chrono::seconds> ttl)
{
    VersionedValue cur = Read(deviceUuid, key);
    cJSON *target = cur.IsPresent() ? cJSON_Parse(cur.value->text.c_str()) : nullptr;
    if (target == nullptr || !cJSON_IsObject(target))
    {
        cJSON_Delete(target);
        target = cJSON_CreateObject();
    }
    cJSON *patch = cJSON_Parse(patchJson.c_str());
    if (patch == nullptr)
    {
        cJSON_Delete(target);
        return cur.version; // invalid patch: logged by caller, no write
    }
    target = cJSONUtils_MergePatch(target, patch);
    char *out = cJSON_PrintUnformatted(target);
    Version v = Write(deviceUuid, key, MakeString(out ? out : "{}"), ChangeOrigin::Driver, ttl);
    cJSON_free(out);
    cJSON_Delete(patch);
    cJSON_Delete(target);
    return v;
}
```

If `cJSON_Utils` is not built in the BartonCommon cJSON, enable it there (one source file) rather than hand-writing merge logic.

**Alternative considered:** Path-based ops (`setTransientMember(name, "3.videoStreamID", v)`, `deleteTransientMember`). Rejected — more API surface; merge patch covers set and delete for nested objects. Arrays are replaced wholesale (RFC 7396), which drivers must know; documented.

**Trade-offs:** Parse/serialize per merge. Values are small.

---

### D3: All write ops resolve against the current view at execution time

**Decision:** `SbmdHandlerInvoker::ExecuteOps` calls the DAL for every write op at the moment it executes, so a merge in a deferred callback applies against whatever the current value is then. Without BARTON-423 these writes are immediate; BARTON-423 changes them to staged writes without changing this API.

**Alternative considered:** None meaningful; this is the DAL contract from BARTON-421.

## Thread Safety

Unchanged from the foundation design: fetches and DAL writes occur on the Matter thread with no DAL locks; the JS mutex is held only while building args and invoking the handler. `FetchedSupplements` are plain C++ values produced before the JS mutex is taken.

## Interfaces

### Driver JS

| API | Change |
| --- | --- |
| `args.supplements` in `onResponse`/`onError` | New. Fresh values read at callback time. |
| `requestCommand(..., { supplements })`, `readAttribute(..., { supplements })` | New optional override; same shape as handler `supplements`. |
| `storage.mergeTransientData(name, patch, ttlSecs)` | New non-terminal op (RFC 7396). |

### Public API / GObject

None. GIR unchanged; backward compatible.

## Migrations

None. Additive contract change recorded in `schema/CHANGELOG.md`; contract versioning remains an open question in the foundation design.

## Testing Philosophy

### Unit

`SbmdResultExecutorTest.cpp`: parse `mergeTransientData`, reject malformed patch. `SbmdDalTest.cpp`: merge into absent/non-object/object values, `null` deletes, nested merge, TTL, version bump. `SbmdHandlerInvokerTest.cpp`: args builders include supplements when provided and omit otherwise.

### Runtime

`SbmdDriverTest.cpp`: a driver parks on `requestCommand`, the test writes the transient key via the DAL, then delivers the response; assert `onResponse` sees the new value, and that the options override replaces the inherited declaration. Same for `onError` and timeout paths.

### Integration

No new pytest tests here; BARTON-425 adds the BARTON-411 reproduction (Docker).

## Documentation Plan

### `docs/SBMD.md`

**Audience:** Driver authors

Update §4.12 and §5.1 (deferred callbacks receive fresh supplements; inheritance and override), §7.2 (`supplements` option), §7.3 (`mergeTransientData`, array-replacement caveat, guidance to prefer merges over read-modify-write across a park).

### `core/deviceDrivers/matter/sbmd/schema/CHANGELOG.md`

**Audience:** Driver authors

Record the additive contract change.

## Risks / Trade-offs

### Drivers keep writing whole values

**Risk:** Drivers continue `setTransientData(JSON.stringify(ctx.sessions))` and still lose updates.

**Mitigation:** Documentation, BARTON-425 migration, and BARTON-423 conflict detection.

### Merge patch array semantics

**Risk:** Arrays are replaced, not merged, which can surprise authors.

**Mitigation:** Documented; camera sessions are keyed objects, not arrays.
