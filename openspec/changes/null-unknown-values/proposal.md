## Why

BARTON-389 asks where BartonCore stands on `NULL` as a valid value and how it should work going forward. Today the answer is "it depends on the path": resources accept `NULL` in memory but lose the distinction on disk, metadata rejects `NULL` at the API and silently drops it in `setMetadata`, system properties reject `NULL` at the API but accept it internally and then treat it as "not found", SBMD scripts collapse JS `null` to `""`, and the sensitive-resource encryption path writes plaintext secrets to disk when encryption fails. Clients cannot tell "value unknown" from "not found" or from an empty string anywhere in the public API.

This change defines `NULL` as a first-class value meaning **"value unknown"** for resources, metadata, and system properties, and closes every gap needed to make that contract hold end to end: API, core, storage, encryption, drivers, and SBMD.

## What Changes

- **Contract**: `NULL` value = "the item exists but its value is unknown". It is distinct from "not found" (reported via `GError`/return code) and from `""` (a known empty value).
- **Public API (api/c)**:
  - Add `RESOURCE_NOT_FOUND` (2) to `BCoreReadResourceError` and `METADATA_NOT_FOUND` (201) to `BCoreReadMetadataError`; set them on not-found instead of returning bare `NULL`.
  - Add `BCoreSystemPropertyError` (300-399); **change the signature** of `b_core_client_get_system_property` to `(self, key, GError **err)`.
  - Allow `NULL` in `b_core_client_write_metadata` and `b_core_client_set_system_property`; add explicit delete functions so `NULL` no longer doubles as "remove".
  - `b_core_client_write_resource` accepts `NULL` ("set to unknown") and passes it to the driver; each driver accepts it or rejects with `FALSE`, and every driver `writeResource` must NULL-guard.
  - Add `(nullable)` GIR annotations to all value getters, properties, and event value properties.
- **Core (core/src)**:
  - Storage writes explicit JSON `null` (`cJSON_AddNullToObject`) for resources, metadata, and system properties; load treats explicit `null` and a missing key as `NULL` (backward compatible).
  - `setMetadata`/`jsonDatabaseSaveMetadata`/`metadataFromJSON` accept and persist `NULL`; `deviceServiceSetMetadata` stops reporting success for dropped writes.
  - System property get returns `true` with `*value == NULL` for a stored unknown; get-all is made consistent.
  - `deviceServiceGetDevicesByMetadata` matches existence of a `NULL`-valued entry.
- **Sensitive resources**: skip encryption for `NULL` and write explicit `null`; **fail the save** if encryption fails (never write plaintext under the encrypted key); remove the misleading "using unencrypted value" warning for `NULL`. Fix the `__wrap_simpleProtectConfigData` test mock (`strlen(NULL)`).
- **Drivers**: every `writeResource` and `getMetadata`/`resource->value` consumer null-checks; Philips Hue and Zigbee common fixed; Matter driver stops collapsing Matter `null` to `0`.
- **SBMD (mquickjs)**: results carry `std::optional<std::string>`; JS `null` → `NULL` (unknown), JS `undefined`/missing → handler error. Document `value: null` in `docs/SBMD.md`.
- **BartonCommon**: document `simpleProtectConfigData`/`simpleUnprotectConfigData` `NULL` semantics; add a status-returning variant so callers can distinguish "input was NULL" from "encryption failed". Zilker carries a duplicate of this file and the running binary links Zilker's copy, so parity is tracked separately (XHCPE ticket).
- **BREAKING (major bump 4.4.0 → 5.0.0)**: `b_core_client_read_resource` / `read_metadata` now set a `GError` for not-found (GI bindings raise); callers that treated "NULL, no error" as "not found" must check the error (behavioral only, no signature change). `b_core_client_get_system_property` gains a `GError **err` parameter (source/ABI break).

### Non-goals

- Changing the string-serialized value model or adding typed values.
- A per-resource "nullable" mode bit; drivers decide per resource whether a `NULL` write is meaningful.
- Renaming the existing unprefixed error enum values (tracked by the TODO in `barton-core-client.h`).
- Fixing Zilker code. This proposal only identifies Zilker regression risks; Zilker work is tracked in XHCPE.
- Auditing every IPC client in Zilker for pass-through of `NULL` values.

## Capabilities

### New Capabilities

- `unknown-values`: Cross-cutting contract for `NULL` meaning "value unknown" across resources, metadata, and system properties, including persistence, encryption, events, and SBMD.

### Modified Capabilities

- `resource-model`: String-serialized values now explicitly allow `NULL` (unknown).
- `public-api`: Not-found errors for read resource/metadata, new system-property read/delete APIs, nullable annotations.

## Impact

- **Layers**: Public API (`api/c/public`, `api/c/src`), core (`core/src/deviceService.c`, `core/src/device/icDeviceResource.c`, `core/src/device/icDeviceMetadata.c`, `core/src/database/jsonDatabase.c`), drivers (`core/deviceDrivers/zigbee*`, `philipsHue`, `matter`, `matter/sbmd/mquickjs`), reference app (`reference/src/coreCategory.c`), docs, BartonCommon (`libs/config/c/src/simpleProtectConfig.c`).
- **CMake flags**: `BCORE_ZIGBEE`, `BCORE_MATTER`, `BCORE_PHILIPS_HUE` (driver fixes), `BCORE_ONVIF` (driver audit). Core and API changes are unconditional.
- **Storage**: On-disk JSON gains explicit `null` values. Older Barton builds load explicit `null` via `getCJSONString` as `NULL` for resources; for system properties older builds log "Skipping unreadable system property" and drop the entry; for metadata older builds drop the entry. Downgrade therefore loses unknown entries but does not crash.
- **Downstream (Zilker)**: Links Zilker's own `libxhConfig` (`simpleProtectConfig`), not BartonCommon's. Two hard crashes on `NULL` resource values exist today (`cameraCrudEventMessage.c:872`, `legacySecurityCluster.c:1793`), and Zilker's IPC handler maps a `NULL` system property to `IPC_GENERAL_ERROR`. See design for the full list.
