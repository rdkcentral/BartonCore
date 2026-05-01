# SBMD Schema Changelog

## v2.1

- Add optional `vendorId` and `productId` fields to `matterMeta` for
  vendor-specific driver claiming (hex string or integer)
- Add `dependentRequired` constraint: if either `vendorId` or `productId`
  is present, both must be specified

## v2.0

- Initial versioned schema (migrated from unversioned `sbmd-spec-schema.json`)
