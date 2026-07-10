# SBMD Schema Changelog

## v4.0

- First schema for the JavaScript-native driver format: specs are authored as
  `.sbmd.js` files registered via `SbmdDriver()`, replacing the v3 YAML format

## v3.0

- Mapper scripts must return `{ value: "..." }` instead of `{ output: "..." }`
  for read/event/command-response results (breaking change from v2.x)
- Scripts may return `{ error: "msg" }` to explicitly signal an error
- Scripts may return `{}` (empty object) to suppress the resource update
- `SbmdUtils.Response.value(v)` and `SbmdUtils.Response.error(msg)` helpers
  added to `sbmd-utils.js` for constructing the new result objects

## v2.1

- Add optional `vendorId` and `productId` fields to `matterMeta` for
  vendor-specific driver claiming (hex string or integer)
- Add `dependentRequired` constraint: if either `vendorId` or `productId`
  is present, both must be specified
- Make `revision` optional in `matterMeta` (previously required); a single
  revision doesn't apply to drivers that span multiple Matter device types

## v2.0

- Initial versioned schema (migrated from unversioned `sbmd-spec-schema.json`)
