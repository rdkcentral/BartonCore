## ADDED Requirements

### Requirement: Vendor ID and product ID in SBMD specs
The `matterMeta` section of an SBMD spec SHALL support optional `vendorId` and `productId` fields (unsigned 16-bit integers). When omitted, the driver SHALL use device-type matching as before.

#### Scenario: Both vendorId and productId specified
- **WHEN** an SBMD spec includes `matterMeta.vendorId: 0x117C` and `matterMeta.productId: 0x0001`
- **THEN** the parser SHALL store both values in `SbmdMatterMeta`

#### Scenario: Neither vendorId nor productId specified
- **WHEN** an SBMD spec omits both `vendorId` and `productId`
- **THEN** the parser SHALL leave both fields as empty optionals and the driver SHALL fall back to device-type matching

#### Scenario: Only one of vendorId or productId specified
- **WHEN** an SBMD spec includes `vendorId` but not `productId`, or vice versa
- **THEN** the parser SHALL reject the spec with an error

### Requirement: DeviceDataCache vendor and product ID accessors
`DeviceDataCache` SHALL provide `GetVendorId(uint16_t &outValue)` and `GetProductId(uint16_t &outValue)` methods that read the BasicInformation cluster's VendorID (attribute 0x0002) and ProductID (attribute 0x0004) from the cluster state cache.

#### Scenario: Vendor ID available in cache
- **WHEN** `GetVendorId` is called and the BasicInformation cluster has VendorID cached
- **THEN** the method SHALL return `CHIP_NO_ERROR` and set `outValue` to the vendor ID

#### Scenario: Cache not populated yet
- **WHEN** `GetVendorId` is called and the cluster state cache is null or does not contain the attribute
- **THEN** the method SHALL return an error code

### Requirement: Vendor/product ID claim semantics
When a driver has `vendorId` and `productId` set, `ClaimDevice` SHALL match against the device's BasicInformation VendorID and ProductID attributes. Both must match for the device to be claimed. The driver's `deviceTypes` list is NOT checked during vendor/product claiming.

#### Scenario: Vendor and product ID match
- **WHEN** a driver specifies `vendorId: 0x117C` and `productId: 0x0001`, and the device reports VendorID `0x117C` and ProductID `0x0001`
- **THEN** `ClaimDevice` SHALL return true

#### Scenario: Vendor ID matches but product ID does not
- **WHEN** a driver specifies `vendorId: 0x117C` and `productId: 0x0001`, and the device reports VendorID `0x117C` and ProductID `0x0002`
- **THEN** `ClaimDevice` SHALL return false

#### Scenario: Driver without vendor/product uses device-type matching
- **WHEN** a driver does not specify `vendorId` or `productId`
- **THEN** `ClaimDevice` SHALL use the existing device-type matching logic

#### Scenario: Vendor/product ID not available in cache
- **WHEN** a driver specifies `vendorId` and `productId` but the device's BasicInformation attributes are not yet cached
- **THEN** `ClaimDevice` SHALL return false

### Requirement: Vendor-specific driver priority
`MatterDriverFactory::GetDriver` SHALL try vendor-specific drivers (those with `vendorId`/`productId` set) before generic device-type drivers. This prevents a generic driver from claiming a device that a more specific vendor driver should handle.

#### Scenario: Vendor-specific driver wins over generic
- **WHEN** both a vendor-specific driver and a generic device-type driver could claim the same device
- **THEN** the vendor-specific driver SHALL be tried first and claim the device

#### Scenario: No vendor-specific driver matches — generic driver claims
- **WHEN** no vendor-specific driver matches a device but a generic device-type driver does
- **THEN** the generic driver SHALL claim the device

#### Scenario: Existing drivers unaffected
- **WHEN** an existing SBMD spec (e.g., `light.sbmd`) does not declare `vendorId`/`productId`
- **THEN** the driver SHALL continue to use device-type matching with no behavior change
