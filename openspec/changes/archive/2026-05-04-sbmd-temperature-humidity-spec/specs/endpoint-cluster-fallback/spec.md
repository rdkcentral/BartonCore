## ADDED Requirements

### Requirement: Endpoint resolution with cluster-based fallback
`MatterDevice` SHALL provide a `ResolveEndpointForCluster` method that resolves a Matter endpoint for a given cluster ID. When an SBMD endpoint index is provided, it SHALL first try the SBMD-mapped endpoint. If that endpoint does not host the required cluster (verified via `DeviceDataCache::EndpointHasServerCluster`), it SHALL fall back to cluster-based lookup via `GetEndpointForCluster`.

#### Scenario: Mapped endpoint hosts the cluster
- **WHEN** `ResolveEndpointForCluster` is called with cluster `0x0402` and SBMD endpoint index 0, and the mapped Matter endpoint hosts cluster `0x0402`
- **THEN** the method SHALL return the mapped endpoint ID

#### Scenario: Mapped endpoint does not host the cluster — fallback
- **WHEN** `ResolveEndpointForCluster` is called with cluster `0x0405` and SBMD endpoint index 0, and the mapped Matter endpoint does NOT host cluster `0x0405` but another endpoint does
- **THEN** the method SHALL fall back to `GetEndpointForCluster` and return the endpoint that hosts `0x0405`

#### Scenario: No SBMD endpoint index provided
- **WHEN** `ResolveEndpointForCluster` is called without an SBMD endpoint index (nullopt)
- **THEN** the method SHALL use `GetEndpointForCluster` directly

#### Scenario: No endpoint hosts the cluster
- **WHEN** `ResolveEndpointForCluster` is called with a cluster ID that no endpoint hosts
- **THEN** the method SHALL return false

#### Scenario: Cache data not yet available
- **WHEN** `ResolveEndpointForCluster` is called and `DeviceDataCache` is null or cannot verify cluster presence via Descriptor
- **THEN** the method SHALL use the SBMD-mapped endpoint directly without fallback

### Requirement: All resource and event binding uses ResolveEndpointForCluster
All SBMD resource binding methods (`BindResourceReadInfo`, `BindWriteInfo`, `BindExecuteInfo`, `BindResourceEventInfo`) SHALL use `ResolveEndpointForCluster` instead of the previous inline if/else pattern for endpoint resolution.

#### Scenario: Read binding on composite device
- **WHEN** `BindResourceReadInfo` is called for a resource whose cluster is on a different Matter endpoint than the SBMD-mapped one
- **THEN** the binding SHALL resolve to the correct endpoint via cluster-based fallback

#### Scenario: Event binding on composite device
- **WHEN** `BindResourceEventInfo` is called for an event whose cluster is on a different Matter endpoint than the SBMD-mapped one
- **THEN** the binding SHALL resolve to the correct endpoint via cluster-based fallback
