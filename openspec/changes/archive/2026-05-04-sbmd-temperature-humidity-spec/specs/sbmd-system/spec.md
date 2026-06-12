## MODIFIED Requirements

### Requirement: Matter metadata in SBMD
Each SBMD spec SHALL define a `matterMeta` section containing `revision` (integer, the Matter device revision) and `deviceTypes` (flat list of Matter device type IDs as hex or decimal values, e.g., `- 0x0100`). Optionally, `featureClusters` (list of cluster IDs whose FeatureMap attributes to read at init time). Optionally, `vendorId` (unsigned 16-bit integer) and `productId` (unsigned 16-bit integer) for vendor-specific device claiming. When `vendorId` and `productId` are set, the driver claims by vendor/product identity rather than by device types.

#### Scenario: Multiple device type support
- **WHEN** an SBMD spec lists `deviceTypes` with IDs `0x0100` and `0x010a`
- **THEN** the resulting driver SHALL claim devices matching either Matter device type

#### Scenario: Feature cluster discovery
- **WHEN** an SBMD spec includes `featureClusters: [0x0006, 0x0008]`
- **THEN** the driver SHALL read the FeatureMap attribute from clusters 6 and 8 at device initialization and make the feature maps available to scripts

#### Scenario: Vendor-specific claiming
- **WHEN** an SBMD spec includes `vendorId: 0x117C` and `productId: 0x0001`
- **THEN** the driver SHALL claim devices by matching BasicInformation VendorID and ProductID instead of device types
