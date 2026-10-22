# Design: null-unknown-values (BARTON-389)

Define `NULL` as "value unknown" for resources, metadata, and system properties, and make every layer of BartonCore preserve that meaning.

## Context

All Barton values are strings. `NULL` already flows through parts of the resource path (`updateResource()` in `core/src/deviceService.c:2839` treats value→`NULL` and `NULL`→value as changes, persists, and emits events), but nothing documents what it means, and the rest of the stack is inconsistent:

| Area | Write `NULL` | Read `NULL` | Persist `NULL` | Distinguish from not-found |
|---|---|---|---|---|
| Resources | Allowed by API (no guard), driver-dependent | Returned as `NULL`, no error | Key omitted (`cJSON_AddStringToObject(..., NULL)`) | No |
| Sensitive resources | Encryption returns `NULL`, falls back | Works by accident | Plaintext written under encrypted key on encryption failure | No |
| Metadata | Rejected (`barton-core-client.c:517`), dropped in `setMetadata` (`deviceService.c:2960`) while `deviceServiceSetMetadata` returns `true` | `NULL` = not set | Dropped; empty also dropped on load (`icDeviceMetadata.c:~192`) | No |
| System properties | Rejected (`barton-core-client.c:505`), accepted internally | Stored `NULL` treated as not found (`jsonDatabase.c:~945`); get-all copies it through | Key dropped (`jsonDatabase.c:172`); load rejects non-strings (`:565-582`) | No |
| SBMD (mquickjs) | `GetStringProp` maps `null`/`undefined` → `""` (`SbmdJsUtil.cpp:34-49`) | — | — | — |

The encryption helpers live in BartonCommon (`libs/config/c/src/simpleProtectConfig.c`). Zilker carries a byte-for-byte equivalent copy (`source/libs/config/c/src/simpleProtectConfig.c`) and, because Zilker patches BartonCommon libs to shared and stages them privately, the **running** binary resolves `simpleProtectConfigData` from Zilker's `libxhConfig.so`.

## References

- [cJSON README](https://github.com/DaveGamble/cJSON#readme) — `cJSON_AddNullToObject` / `cJSON_IsNull` semantics used for explicit `null` persistence.
- [GObject Introspection annotations](https://gi.readthedocs.io/en/latest/annotations/giannotations.html) — `(nullable)` / `(transfer full)` annotations required for language bindings to model unknown values.
- [GLib GError guidelines](https://docs.gtk.org/glib/error-reporting.html) — Domain/code rules for adding new error codes to `B_CORE_CLIENT_ERROR`.
- `docs/SBMD.md` — SBMD script result contract that must document `value: null`.
- `openspec/specs/resource-model/spec.md` — Existing "String-serialized resource values" requirement being modified.

## Goals / Non-Goals

**Goals:**

- One documented meaning for `NULL`: exists, value unknown.
- Not-found is always reported out-of-band (`GError` or `false`), never as a bare `NULL`.
- `NULL` survives a save/load round trip for resources, metadata, and system properties, including sensitive resources.
- Encryption failure never writes plaintext.
- SBMD scripts can report unknown explicitly.
- Backward-compatible load of existing on-disk data.

**Non-Goals:**

- Typed values or a schema change beyond explicit JSON `null`.
- Clients writing `NULL` to resources.
- Renaming the legacy unprefixed error enums.
- Fixing Zilker (tracked in XHCPE).

## Decisions

### D1: `NULL` means "value unknown"; not-found is out-of-band

**Decision:** A `NULL` value always means the item exists and its value is unknown. Not-found is signaled with a `GError` (public API) or a `false` return (internal API). Empty string `""` is a known, empty value and must be preserved.

**Alternative considered:** Use `""` or a sentinel string (`"unknown"`) for unknown. Rejected: collides with legitimate values, forces every consumer to string-compare, and is invisible to type-aware bindings.

**Trade-offs:** Callers that treated `NULL` as not-found must be updated (breaking behavior, see Risks).

---

### D2: Clients cannot write `NULL` to resources; drivers can

**Decision:** Unknown resource state is reported by devices, not commanded by clients. `deviceServiceWriteResourceNoPattern` rejects `NULL` once, centrally, so no driver `writeResource` sees a `NULL` `newValue` from a client. Drivers still update resources to `NULL` via `updateResource()`.

```c
// core/src/deviceService.c — deviceServiceWriteResourceNoPattern()
if (value == NULL)
{
    icLogWarn(LOG_TAG, "%s: refusing NULL write to %s", __func__, uri);
    return false;
}
```

```c
// api/c/src/barton-core-client.c
gboolean b_core_client_write_resource(BCoreClient *self, const gchar *uri, const gchar *resourceValue)
{
    g_return_val_if_fail(self != NULL, FALSE);
    g_return_val_if_fail(uri != NULL, FALSE);
    g_return_val_if_fail(resourceValue != NULL, FALSE);

    return deviceServiceWriteResource(uri, resourceValue) ? TRUE : FALSE;
}
```

**Alternative considered:** Let drivers decide per resource. Rejected: already inconsistent today (Light/LightController/DoorLock reject; `zigbeeDriverCommon.c:2122` and `philipsHueDeviceDriver.c:345` crash or misbehave).

**Trade-offs:** If a future resource needs "reset to unknown" from the client, a dedicated execute resource is needed.

---

### D3: Clients may write `NULL` to metadata and system properties; deletion is explicit

**Decision:** Metadata and system properties are client-owned state, so a client may record "unknown". Because `NULL` no longer means "remove", add explicit delete calls.

```c
// api/c/public/barton-core-client.h
/**
 * b_core_client_write_metadata:
 * @value: (nullable): the value, or NULL to record "value unknown"
 */
gboolean b_core_client_write_metadata(BCoreClient *self, const gchar *uri, const gchar *value);

/**
 * b_core_client_delete_metadata:
 * Returns: TRUE if the entry existed and was removed
 */
gboolean b_core_client_delete_metadata(BCoreClient *self, const gchar *uri);

/**
 * b_core_client_set_system_property:
 * @value: (nullable): the value, or NULL to record "value unknown"
 */
gboolean b_core_client_set_system_property(BCoreClient *self, const gchar *name, const gchar *value);

/**
 * b_core_client_delete_system_property:
 * Returns: TRUE if the property existed and was removed
 */
gboolean b_core_client_delete_system_property(BCoreClient *self, const gchar *name);
```

**Alternative considered:** Keep metadata/system properties `NULL`-free. Rejected by product decision on BARTON-389: unknown must be representable for all three.

---

### D4: New not-found error codes and a GError-returning system-property read

**Decision:** Extend existing error ranges; add a new range for system properties.

```c
// api/c/public/barton-core-client.h
typedef enum
{
    RESOURCE_NOT_READABLE = 1,
    RESOURCE_NOT_FOUND = 2
} BCoreReadResourceError;

typedef enum
{
    METADATA_NOT_ACCESSIBLE = 200,
    METADATA_NOT_FOUND = 201
} BCoreReadMetadataError;

// - BCoreSystemPropertyError: 300-399
typedef enum
{
    SYSTEM_PROPERTY_NOT_FOUND = 300
} BCoreSystemPropertyError;

/**
 * b_core_client_read_system_property:
 * Returns: (nullable) (transfer full): the value; NULL with @err unset means "value unknown",
 *          NULL with @err set to SYSTEM_PROPERTY_NOT_FOUND means not found.
 */
gchar *b_core_client_read_system_property(BCoreClient *self, const gchar *key, GError **err);
```

```c
// api/c/src/barton-core-client.c
gchar *b_core_client_read_resource(BCoreClient *self, const gchar *uri, GError **err)
{
    g_return_val_if_fail(self != NULL, NULL);
    g_return_val_if_fail(uri != NULL, NULL);

    scoped_icDeviceResource *resource = deviceServiceGetResourceByUri(uri);
    if (resource == NULL)
    {
        g_set_error_literal(err, B_CORE_CLIENT_ERROR, RESOURCE_NOT_FOUND, "Resource not found");
        return NULL;
    }
    if ((resource->mode & RESOURCE_MODE_READABLE) == 0)
    {
        g_set_error_literal(err, B_CORE_CLIENT_ERROR, RESOURCE_NOT_READABLE, "Resource is not readable");
        return NULL;
    }
    return g_strdup(resource->value);
}

gchar *b_core_client_read_system_property(BCoreClient *self, const gchar *key, GError **err)
{
    g_return_val_if_fail(self != NULL, NULL);
    g_return_val_if_fail(key != NULL, NULL);

    gchar *value = NULL;
    if (!deviceServiceGetSystemProperty(key, &value))
    {
        g_set_error_literal(err, B_CORE_CLIENT_ERROR, SYSTEM_PROPERTY_NOT_FOUND, "System property not found");
    }
    return value;
}
```

`b_core_client_get_system_property` is kept unchanged and documented as unable to distinguish unknown from not-found.

**Alternative considered:** Add `GError **` to `b_core_client_get_system_property`. Rejected: ABI/API break.

---

### D5: Explicit JSON `null` on disk; missing key loads as `NULL`

**Decision:** All three stores write `cJSON_AddNullToObject` for `NULL`. Loaders accept explicit `null` and a missing key as `NULL` (backward compatible with existing files where the key was dropped).

```c
// core/src/database/jsonDatabase.c — stringHashMapToJson()
if (value == NULL)
{
    cJSON_AddNullToObject(body, key);
}
else
{
    cJSON_AddStringToObject(body, key, value);
}
```

```c
// core/src/database/jsonDatabase.c — loadSystemProperties()
if (cJSON_IsString(item) && item->valuestring != NULL)
{
    stringHashMapPut(systemProperties, strdup(item->string), strdup(item->valuestring));
}
else if (cJSON_IsNull(item))
{
    stringHashMapPut(systemProperties, strdup(item->string), NULL);
}
else
{
    icLogWarn(LOG_TAG, "Skipping unreadable system property %s", item->string);
}
```

```c
// core/src/database/jsonDatabase.c — jsonDatabaseGetSystemPropertyNoLock()
static bool jsonDatabaseGetSystemPropertyNoLock(const char *key, char **value)
{
    if (!stringHashMapContains(systemProperties, key))
    {
        return false;
    }
    char *propertyValue = stringHashMapGet(systemProperties, key);
    *value = propertyValue == NULL ? NULL : strdup(propertyValue);
    return true;
}
```

Resources (`icDeviceResource.c` `resourceToJSON`) and metadata (`icDeviceMetadata.c` `metadataToJSON`) use the same `NULL` → `cJSON_AddNullToObject` pattern. `metadataFromJSON` stops rejecting `NULL` and `""` values (only `id`/`uri` are required).

**Alternative considered:** Keep omitting the key. Rejected: indistinguishable from a corrupted/old record and invisible when inspecting files.

**Trade-offs:** Downgrade drops unknown metadata/system-property entries (see Risks).

---

### D6: Sensitive resources never write plaintext

**Decision:** `NULL` skips encryption and writes explicit `null` under the encrypted key. Encryption failure on a non-`NULL` value fails serialization (returns `NULL`) so the caller does not persist; the previous on-disk value is kept.

```c
// core/src/device/icDeviceResource.c — resourceToJSON()
const char *valueKey = RESOURCE_VALUE_KEY;
char *resourceValue = NULL;

if (resource->mode & RESOURCE_MODE_SENSITIVE)
{
    if (!serDesHasContextValue(context, RESOURCE_NAMESPACE_KEY))
    {
        icLogError(LOG_TAG, "Cannot encrypt resource \"%s\": missing namespace context value", resource->id);
        return NULL;
    }
    valueKey = RESOURCE_ENCRYPTED_VALUE_KEY;
    if (resource->value != NULL)
    {
        resourceValue =
            simpleProtectConfigData(serDesGetContextValue(context, RESOURCE_NAMESPACE_KEY), resource->value);
        if (resourceValue == NULL)
        {
            icLogError(LOG_TAG, "Encryption failed for resource \"%s\"; not saving", resource->id);
            return NULL;
        }
    }
}
else if (resource->value != NULL)
{
    resourceValue = strdup(resource->value);
}

cJSON *json = cJSON_CreateObject();
/* ... id/uri/mode/cachingPolicy/dateOfLastSync/type as today ... */
if (resourceValue == NULL)
{
    cJSON_AddNullToObject(json, valueKey);
}
else
{
    cJSON_AddStringToObject(json, valueKey, resourceValue);
}
free(resourceValue);
return json;
```

On load, an explicit `null` or missing encrypted key yields `NULL` without the "using unencrypted value" warning; a legacy plaintext `value` key on a sensitive resource is still honored once (migration) and re-encrypted on next save. All callers of `resourceToJSON`/`resourcesToJSON` must handle a `NULL` return (abort the device save, keep the old file).

**Alternative considered:** Keep plaintext fallback but log loudly. Rejected: leaks secrets to disk and the value is unrecoverable on reload anyway.

**Trade-offs:** A persistent encryption fault now blocks persisting the whole device record until resolved. That is preferable to silent secret leakage.

---

### D7: SBMD results preserve `null`

**Decision:** Result structs use `std::optional<std::string>`. A JS `null` maps to `std::nullopt` (unknown). A missing/`undefined` `value` is a script error.

```cpp
// core/deviceDrivers/matter/sbmd/mquickjs/SbmdJsUtil.cpp
enum class NullableStringResult { Value, Null, Missing, Error };

NullableStringResult GetNullableStringProp(JSContext *ctx, JSValue obj, const char *name, std::string &out);
```

`SbmdResultExecutor.h` fields (`:69,76,82,88,108`) become `std::optional<std::string> value`. `SpecBasedMatterDeviceDriver.cpp:786-799, 1035-1039, 1578-1582` return `nullptr` for `std::nullopt` instead of `strdup("")`. `SbmdHandlerInvoker.cpp:389,394-396` pass `NULL` through to `setMetadata`/`deviceServiceSetMetadata`.

**Alternative considered:** Keep `""` and add a separate `unknown: true` flag. Rejected: two ways to say the same thing; `null` is the idiomatic JS signal.

---

### D8: BartonCommon `simpleProtectConfig` gets documented NULL semantics and a status variant

**Decision:** Do not change existing return values (Zilker callers depend on them). Document that `NULL` input returns `NULL`, and add:

```c
// libs/config/c/public/xhConfig/simpleProtectConfig.h (BartonCommon)
typedef enum
{
    SIMPLE_PROTECT_OK = 0,
    SIMPLE_PROTECT_NULL_INPUT,
    SIMPLE_PROTECT_FAILED
} SimpleProtectStatus;

SimpleProtectStatus simpleProtectConfigDataEx(const char *namespace, const char *data, char **out);
SimpleProtectStatus simpleUnprotectConfigDataEx(const char *namespace, const char *data, char **out);
```

Also fix the session leak (`openProtectConfigSession()` without close at `:74`). BartonCore uses the `Ex` variants. **Because Zilker links its own copy of this library, the same change must land in Zilker before BartonCore can depend on the new symbols** — otherwise Zilker builds succeed against BartonCommon headers and fail at runtime with an unresolved symbol.

**Alternative considered:** Do everything in BartonCore with `value == NULL` checks only. Viable as a stop-gap (D6 does not strictly need `Ex`), and is the fallback if Zilker parity slips.

## Data Storage

Resource record (`<deviceId>.json`, per resource):

```json
{ "id": "lockCode", "uri": "/d/ep/1/r/lockCode", "mode": 135, "cachingPolicy": 1,
  "dateOfLastSyncMillis": 0, "type": "com.icontrol.string", "encryptedValue": null }
```

Metadata entry: `{ "id": "x", "uri": "/d/m/x", "value": null }`.

System properties file: `{ "propA": "1", "propB": null }`.

## Interfaces

| Function | Change |
|---|---|
| `b_core_client_read_resource` | Sets `RESOURCE_NOT_FOUND`; returns `(nullable)` unknown with no error |
| `b_core_client_write_resource` | Rejects `NULL` |
| `b_core_client_read_metadata` | Sets `METADATA_NOT_FOUND`; `NULL` without error = unknown |
| `b_core_client_write_metadata` | Accepts `NULL` |
| `b_core_client_delete_metadata` | New |
| `b_core_client_read_system_property` | New, GError variant |
| `b_core_client_get_system_property` | Unchanged; doc notes ambiguity |
| `b_core_client_set_system_property` | Accepts `NULL` |
| `b_core_client_delete_system_property` | New |
| `BCoreResource:value`, resource/metadata updated event `value` props | `(nullable)` annotations |
| Reference CLI (`reference/src/coreCategory.c:305,350`) | Print `<unknown>` instead of `(null)` |

## Implementation Detail

Driver audit fixes:

- `core/deviceDrivers/philipsHue/philipsHueDeviceDriver.c:142,382,387,463` — guard `resource->value`.
- `core/deviceDrivers/zigbee/zigbeeDriverCommon.c:2122` — guard `newValue`/`previousValue`.
- `MatterDeviceDriver.cpp:1545` — Matter `Nullable` `IsNull()` → `updateResource(..., NULL, ...)` instead of `0`.
- `OnvifDeviceDriver.cpp`, `MatterDeviceDriver.cpp`, `SpecBasedMatterDeviceDriver.cpp` `writeResource` — confirm `previousValue == NULL` is safe.
- `deviceService.c:2975` `setMetadata` — use `stringCompare` (NULL-safe) and `strdupOpt`; `jsonDatabase.c:1778` `jsonDatabaseSaveMetadata` — `strdupOpt`.
- `deviceService.c:4276` `deviceServiceSetMetadata` — return the real result.
- `deviceService.c:4332` `deviceServiceGetDevicesByMetadata` — existence query matches `NULL`-valued entries.
- `icInitialResourceValues.h:117-135` — callers use `initialResourceValuesHas*` before `Get*` so "no entry" ≠ "unknown".

## Migrations

No schema version bump. Loaders accept old (missing key) and new (explicit `null`) forms. Sensitive resources stored with a plaintext `value` key are read once and re-encrypted on the next save.

Order: (1) BartonCommon `Ex` + docs, (2) Zilker parity (XHCPE), (3) BartonCore storage/encryption, (4) core semantics, (5) public API, (6) drivers/SBMD, (7) docs.

## Testing Philosophy

### Storage round trip

For each store, save `NULL`, `""`, and a value; reload; assert all three are distinct. Load legacy files with the key missing and assert `NULL`.

### Sensitive resources

Fix `__wrap_simpleProtectConfigData` in `core/test/src/database/jsonDatabaseTest.c:2417` to handle `NULL`. Test `NULL` round trip, a forced encryption failure (assert no plaintext in the serialized JSON and that save fails), and legacy plaintext migration.

### Public API

Unit tests for not-found vs unknown vs not-readable on resource, metadata, and system-property reads; `NULL` write rejected for resources and accepted for metadata/system properties; delete functions. Re-enable/port the disabled `deviceServiceTest.c:308-312` case.

### SBMD

mquickjs tests for `value: null` → unknown, missing `value` → error, `""` preserved. Integration test (Docker) with a Matter virtual device reporting a nullable attribute as null.

### Drivers

Unit tests for Philips Hue and Zigbee common with `NULL` previous values. No hardware required.

## Documentation Plan

### `docs/USING_BARTON_GUIDE.md`

**Audience:** Client developers

Add a "Value unknown" section: meaning of `NULL`, how to detect not-found via `GError`, `""` vs `NULL`, and which items clients may set to `NULL`.

### `docs/SBMD.md`

**Audience:** SBMD authors

Document `return { value: null }` for unknown and that omitting `value` is an error.

### `docs/barton-device-service-architecture.md`

**Audience:** Core developers

Note that sensitive resource persistence fails closed on encryption errors. No `docs/dev/architecture.md` exists in the project.

### Public headers

`(nullable)` annotations and gtk-doc text for every affected function and property.

## Risks / Trade-offs

### Breaking behavior for not-found reads

**Risk:** Existing clients treat "`NULL`, no error" from `read_resource`/`read_metadata` as not-found and will now also see a `GError`; some may treat any error as fatal.

**Mitigation:** Call out in CHANGELOG as breaking; ship in a minor bump with release notes. Zilker's `deviceServiceIpcHandler.c` already keys success off `err`, so it improves.

### Zilker regressions

**Risk:** Zilker dereferences resource values without NULL checks: `services/comm/core/src/userver/event/cameraCrudEventMessage.c:872` (`strdup`) and `services/device/core/src/clusters/zigbee/legacySecurityCluster.c:1793` (`strtoul`). Zilker's IPC handler maps a `NULL` system property to `IPC_GENERAL_ERROR` (`deviceServiceIpcHandler.c:858-870`) and passes `NULL` metadata/resource values to IPC clients (`deviceServiceIpcHelper.c:52-108`, `deviceHelper.c:294-434`). About 50 IPC client files were not audited for pass-through.

**Mitigation:** Track in XHCPE; fix the two crashes before consuming this Barton version. Today these paths are reachable only if a driver already sets `NULL` (resources), so the crash risk exists now and grows with D7.

### Divergent `simpleProtectConfig` copies

**Risk:** Zilker resolves `simpleProtectConfigData` from its own `libxhConfig`. New BartonCommon symbols compile but fail to resolve at runtime in Zilker.

**Mitigation:** Land Zilker parity first (XHCPE), or keep BartonCore on the legacy functions (D6 works without `Ex`).

### Downgrade data loss

**Risk:** Older builds drop explicit-`null` metadata and system properties on load.

**Mitigation:** Documented; values were unknown anyway.

### Encryption fail-closed blocks saves

**Risk:** A broken keystore prevents saving any device with sensitive resources.

**Mitigation:** Error log + metric; in-memory state continues; the prior on-disk record is preserved.
