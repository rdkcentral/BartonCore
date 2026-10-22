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
- Per-resource "nullable" mode bit (drivers reject `NULL` writes individually instead).
- Renaming the legacy unprefixed error enums.
- Fixing Zilker (tracked in XHCPE).

## Decisions

### D1: `NULL` means "value unknown"; not-found is out-of-band

**Decision:** A `NULL` value always means the item exists and its value is unknown. Not-found is signaled with a `GError` (public API) or a `false` return (internal API). Empty string `""` is a known, empty value and must be preserved.

**Alternative considered:** Use `""` or a sentinel string (`"unknown"`) for unknown. Rejected: collides with legitimate values, forces every consumer to string-compare, and is invisible to type-aware bindings.

**Trade-offs:** Callers that treated `NULL` as not-found must be updated (breaking behavior, see Risks).

---

### D2: Clients may write `NULL` to resources; drivers decide whether it is meaningful

**Decision:** A client may write `NULL` to a writable resource to mean "set this value to unknown" (for example, clearing a cached label or credential). The public API and core pass `NULL` through to the driver unchanged. Each driver's `writeResource` SHALL either accept `NULL` (and call `updateResource(..., NULL, ...)`) or reject it by returning `false`; it SHALL never dereference `newValue` or `previousValue` without a check. The device service owns the contract, so a missing driver guard is a driver bug, not a client bug.

```c
// api/c/public/barton-core-client.h
/**
 * b_core_client_write_resource:
 * @uri: the resource URI
 * @resourceValue: (nullable): the new value, or NULL to set the value to "unknown"
 *
 * Returns: TRUE if the driver accepted the write. Drivers MAY reject NULL for resources
 *          where "unknown" is not meaningful.
 */
gboolean b_core_client_write_resource(BCoreClient *self, const gchar *uri, const gchar *resourceValue);
```

```c
// api/c/src/barton-core-client.c — unchanged; no NULL guard on resourceValue
gboolean b_core_client_write_resource(BCoreClient *self, const gchar *uri, const gchar *resourceValue)
{
    g_return_val_if_fail(self != NULL, FALSE);
    g_return_val_if_fail(uri != NULL, FALSE);

    return deviceServiceWriteResource(uri, resourceValue) ? TRUE : FALSE;
}
```

```c
// core/src/deviceService.c — deviceServiceWriteResourceNoPattern() (existing structure kept;
// only the logging around the driver call changes). Drivers without writeResource still fail.
    else
    {
        // The device driver's contract states that they will call us back at updateResource if successful
        //  we save the change there. value MAY be NULL ("set to unknown").
        result = driver->writeResource(driver->callbackContext, resource, resource->value, value);
        if (!result && value == NULL)
        {
            icLogDebug(LOG_TAG, "Driver %s rejected NULL (unknown) write to %s", driver->driverName, uri);
        }
    }
```

```c
// Driver pattern — e.g. core/deviceDrivers/zigbee/zigbeeLightDeviceDriver.c writeResource()
if (newValue == NULL)
{
    // "unknown" is not a commandable state for on/off/level; reject.
    return false;
}
```

Required driver work: Zigbee Light/LightController/DoorLock already reject (keep, document); `zigbeeDriverCommon.c:2122` and `philipsHueDeviceDriver.c:345` must add guards; Onvif (`OnvifDeviceDriver.cpp`, user/password at :418/:429 already guarded), `MatterDeviceDriver.cpp`, and `SpecBasedMatterDeviceDriver.cpp` must be audited. SBMD write mappers receive `null` as the script input value: today `SbmdHandlerInvoker.cpp:148-157` omits `input` when `std::nullopt` (JS sees `undefined`), so a NULL write must set `args.resource.input = null` explicitly to distinguish "unknown" from "no input" (execute without args). Scripts may throw/return an error to reject.

**Alternative considered:** Reject `NULL` centrally in `deviceServiceWriteResourceNoPattern`. Rejected by product decision on BARTON-389: clients need to be able to set a resource to unknown.

**Alternative considered:** Reject centrally and add an opt-in per-resource flag (`RESOURCE_MODE_NULLABLE`). Deferred: adds a mode bit and migration for every driver; can be layered on later if driver-level rejection proves inconsistent.

**Trade-offs:** Every driver becomes responsible for a `NULL` guard; a missed guard is a crash. Mitigated by a core unit test that writes `NULL` to every writable resource of every reference device class (task 3.1).

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
 * b_core_client_get_system_property:
 * @err: (nullable): return location for a #GError
 * Returns: (nullable) (transfer full): the value; NULL with @err unset means "value unknown",
 *          NULL with @err set to SYSTEM_PROPERTY_NOT_FOUND means not found.
 *
 * Since: 5.0.0 — @err added (signature change).
 */
gchar *b_core_client_get_system_property(BCoreClient *self, const gchar *key, GError **err);
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

gchar *b_core_client_get_system_property(BCoreClient *self, const gchar *key, GError **err)
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

`b_core_client_get_system_property` gains a `GError **err` parameter in place. Because 5.0.0 is already a breaking release, we change the existing function rather than adding a parallel `read_system_property`. That keeps a single getter and matches `read_resource`/`read_metadata`. This is a **source and ABI break**: every C caller must add the argument, and in GI bindings the call now raises on not-found. Known callers that must update:

- `api/c/test/barton-core-client-test.c:1477-1500`
- the reference app, if it is used there
- Zilker `services/device/core/src/ipc/deviceServiceIpcHandler.c:858`
- any Python tests that call `get_system_property`; they will raise on missing keys

The **internal** contract also changes: `deviceServiceGetSystemProperty` now returns `true` with `*value == NULL` for a stored unknown property.

- These callers are already NULL-safe: `deviceDescriptorHandler.c:664,672`, `deviceService.c:2213`, `zigbeeTelemetry.c:331,569`, `Matter.cpp:980`.
- `Matter.cpp:1006,1030,1055` (RCA/ICA/NOC) call `strlen()` whenever the getter returns `true`. Each must add `&& b64X != NULL` and treat `NULL` the same as not found.
- Zilker does not call the internal getter.

**Alternative considered:** Keep the old signature and add `b_core_client_read_system_property`. Rejected: the release is already breaking, and two getters with different semantics would cause confusion.

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
| `b_core_client_write_resource` | Accepts `NULL` (documented `(nullable)`); driver may reject → `FALSE` |
| `b_core_client_read_metadata` | Sets `METADATA_NOT_FOUND`; `NULL` without error = unknown |
| `b_core_client_write_metadata` | Accepts `NULL` |
| `b_core_client_delete_metadata` | New |
| `b_core_client_get_system_property` | **Signature change**: adds `GError **err`; sets `SYSTEM_PROPERTY_NOT_FOUND` |
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

Unit tests for not-found vs unknown vs not-readable on resource, metadata, and system-property reads; `NULL` write accepted for resources (passed to driver; rejecting drivers return `FALSE`), metadata, and system properties; delete functions. Re-enable/port the disabled `deviceServiceTest.c:308-312` case.

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

**Risk:** Existing clients treat "`NULL`, no error" from `read_resource`/`read_metadata` as not-found and will now also see a `GError`. In GObject Introspection bindings (Python/JS) the call now raises instead of returning `None`. Zilker is directly affected: `troubleState.c:1485` and Java `DeviceServiceDevice.java:441` stop being able to create first-time metadata (see Zilker regressions).

**Mitigation:** This is an accepted, intentional **breaking change**. Read and write for resources and metadata keep their C signatures; the change there is behavioral only. `b_core_client_get_system_property` changes signature, adding `GError **err`, so it breaks both source and ABI. Ship it as a **major version bump (4.4.0 → 5.0.0)** with a BREAKING CHANGE footer and a CHANGELOG entry. Consumers (Zilker, XHCPE-2742) must adopt the new not-found/unknown semantics and the new system-property signature before taking 5.0.0. The legacy error-enum rename TODO in `barton-core-client.h` could be bundled into the same major release, but it is out of scope here.

### Zilker regressions

**Risk:** A full audit of Zilker device-service internals and IPC clients (`/home/cleith065/src/core/source`) found the following. All paths are relative to `source/`.

*Crashes on a `NULL` value:*

- `services/device/core/src/clusters/zigbee/legacySecurityCluster.c:1793`: `strtoul(resource->value)`
- `libs/device/helper/c/src/sensorHelper.c:99`: `strcmp("true", NULL)` in `isEndpointFaulted`, reached from `services/security/core/src/zone/securityZone.c:2171`. A zone with an unknown `faulted` crashes the security service.
- `services/comm/core/src/userver/userverEventListener.c:1662`: `strcmp(value, "false")` on a `bypassed` event
- `services/comm/core/src/userver/event/cameraCrudEventMessage.c:872`: `strdup`
- `services/comm/core/src/aws/deviceHelper/awsDeviceThingHelper.c:544`: `strdup` in the CIE color path (via `:1178`)
- `integrations/xbb/libxbb/src/xbb.c:318,328`: `strncpy`
- `integrations/xbb/libxbb/src/xbb.c:338,348`: `sscanf`

*Behavior regressions caused by not-found now returning an error:*

- `services/security/src/trouble/troubleState.c:1485-1513`: the first device trouble metadata is never written. The helper needs a three-way result (value / unknown / not-found).
- Java `DeviceServiceDevice.java:441`: the first user property can never be set.
- Java `DeviceServiceSecurityController:82`, `DeviceServiceBridge.java:361`, `CameraManagerFactory.java:1400`: these now throw.
- About 102 Java `readEndpointResource`/`readDeviceResource` call sites throw on optional missing resources.
- `services/automation/src/actions/devices.c:106`: now returns a JSON-RPC error.

*Behavior regressions caused by `NULL` meaning unknown:*

- `stringToBool(NULL)` returns `false`, so unknown is reported upstream as false:
  - AWS encoders: `deviceMessageEncodeCommon.c:562` (reports `connected=true`), `lightMessageEncode.c:262`, `zoneMessageEncode.c:853`, `chimeMessageEncode.c:315`, `doorLockMessageEncode.c:255`, `awsRtspCameraThingHandler.c:194,200`
  - `xbb.c:217,228,250`
  - `userverEventListener.c:1628` (sends a false camera-motion restore)
- `put_metadataValue_in_*` (generated pojo) drops `NULL` map entries, so `QUERY_METADATA` disagrees with `READ_METADATA`.
- Java pojos use `optString()`, so `NULL` arrives as `""`.
- `deviceServiceIpcHandler.c:858-869` reports a `NULL` system property as an error.
- `services/automation/src/actions/devices.c:156`: a non-string JSON value now performs a successful `NULL` write that clears the resource.
- deviceUtil `wm <uri>` and `writeSystemProperty <key>` with no value used to delete and now write `NULL`. These are the only callers that depended on "`NULL` write means delete".
- `"(null)"` is printed into server payloads: `cameraAccessEventMessage.c:114,141,145`, `zoneCrudEventMessage.c:1389,1394`, `troubleEventMessage.c:926,931`.

About 70 call sites were found safe. These bugs exist independently of this change: a double `free` at `troubleState.c:1497/1517`, and the wrong variable parsed at `DeviceServiceDevice.java:445`.

**Mitigation:** Track all of the above in XHCPE-2742. Zilker needs:

- NULL guards at the crash sites
- a policy for unknown upstream (omit, last-known, or explicit unknown)
- a three-way C helper result
- `DELETE_METADATA` / `DELETE_SYSTEM_PROPERTY` IPC calls
- explicit-null support in the Java pojos

Zilker must not consume a Barton version with this change until the crash sites and the not-found handling are fixed. Several crash sites can already be reached today if a driver sets `NULL`.

### Divergent `simpleProtectConfig` copies

**Risk:** Zilker resolves `simpleProtectConfigData` from its own `libxhConfig`. New BartonCommon symbols compile but fail to resolve at runtime in Zilker.

**Mitigation:** Land Zilker parity first (XHCPE), or keep BartonCore on the legacy functions (D6 works without `Ex`).

### Downgrade data loss

**Risk:** Older builds drop explicit-`null` metadata and system properties on load.

**Mitigation:** Documented; values were unknown anyway.

### Encryption fail-closed blocks saves

**Risk:** A broken keystore prevents saving any device with sensitive resources.

**Mitigation:** Error log + metric; in-memory state continues; the prior on-disk record is preserved.
