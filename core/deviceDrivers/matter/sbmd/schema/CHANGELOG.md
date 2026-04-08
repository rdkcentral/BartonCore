# SBMD Schema Changelog

## v2.1

- Add optional `vendorId` and `productId` fields to `matterMeta` for
  vendor-specific driver claiming (hex string or integer)
- Add `dependentRequired` constraint: if either `vendorId` or `productId`
  is present, both must be specified
- Make `revision` optional in `matterMeta` (previously required); a single
  revision doesn't apply to drivers that span multiple Matter device types

## v2.0

- Initial versioned schema (migrated from unversioned `sbmd-spec-schema.json`)
