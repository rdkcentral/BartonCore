## MODIFIED Requirements

### Requirement: Matter metadata in SBMD
Each SBMD spec SHALL define a `matterMeta` section containing `revision` (integer, the Matter device revision) and `deviceTypes` (flat list of Matter device type IDs as hex or decimal values, e.g., `- 0x0100`). Optionally, `featureClusters` (list of cluster IDs whose FeatureMap attributes to read at init time). Optionally, `deviceTypeMatch` (string, `"any"` or `"all"`, default `"any"`) controlling how `deviceTypes` are matched against device endpoints during `ClaimDevice`.

#### Scenario: Multiple device type support
- **WHEN** an SBMD spec lists `deviceTypes` with IDs `0x0100` and `0x010a`
- **THEN** the resulting driver SHALL claim devices matching either Matter device type (default `"any"` semantics)

#### Scenario: Feature cluster discovery
- **WHEN** an SBMD spec includes `featureClusters: [0x0006, 0x0008]`
- **THEN** the driver SHALL read the FeatureMap attribute from clusters 6 and 8 at device initialization and make the feature maps available to scripts

#### Scenario: Device type match mode
- **WHEN** an SBMD spec includes `deviceTypeMatch: "all"`
- **THEN** the driver SHALL only claim devices where ALL listed device types are present across the device's non-root endpoints
