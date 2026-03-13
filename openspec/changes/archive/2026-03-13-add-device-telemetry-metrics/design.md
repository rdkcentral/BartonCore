## Context

BartonCore exposes device state through a URI-based resource model (`/deviceId/ep/N/r/resourceName`). All resource value changes — from both Zigbee (8 native C drivers) and Matter (5 SBMD YAML specs) — converge through a single function: `updateResource()` in `core/src/deviceService.c` (~line 2933). This function already performs value comparison, storage, database persistence, and conditional event emission.

The existing observability layer (33 OTel meters) instruments service internals but does not record per-device telemetry. The `updateResource()` convergence point and the existing `icDeviceResource` metadata (resource type, endpoint profile, device ID) make it possible to add device telemetry with minimal code changes and zero per-driver modifications.

```
                    Zigbee Drivers (8)              Matter SBMD (5 specs)
                    ┌──────────────┐                ┌──────────────────┐
                    │ onOffChanged │                │ OnAttributeChanged│
                    │ tempChanged  │                │ → JS mapper      │
                    │ rssiUpdated  │                │ → updateResource │
                    └──────┬───────┘                └────────┬─────────┘
                           │                                  │
                           ▼                                  ▼
                    ┌─────────────────────────────────────────────┐
                    │           updateResource()                   │
                    │     core/src/deviceService.c:2933            │
                    │                                             │
                    │  ┌─ value changed? ──┐                      │
                    │  │ YES: persist, emit│  NO: return           │
                    │  └──────┬────────────┘                      │
                    │         ▼                                    │
                    │  ┌─────────────────────────┐                │
                    │  │ NEW: emit OTel metrics   │◄── this change │
                    │  │  • numeric → gauge       │                │
                    │  │  • boolean → counter     │                │
                    │  │  • update rate counter   │                │
                    │  └─────────────────────────┘                │
                    └─────────────────────────────────────────────┘
                                     │
                           ┌─────────┼─────────┐
                           ▼         ▼         ▼
                      OTel SDK   Database   GSignal events
```

**Thread safety:** `updateResource()` runs on the GLib main loop thread. OTel SDK calls are thread-safe. No additional synchronization needed.

**Resource metadata available at `updateResource()`:**
- `resource->id` — resource name (e.g., "localTemperature", "isOn")
- `resource->type` — data type ("com.icontrol.boolean", "com.icontrol.integer", "com.icontrol.percentage", etc.)
- `resource->deviceUuid` — device identifier
- `resource->endpointId` — endpoint identifier
- Endpoint profile (via lookup): "light", "sensor", "thermostat", "doorLock", etc.
- Device class (via lookup): "light", "sensor", "doorLock", etc.
- Managing device driver (via lookup): "zigbeeDriver", "matterDriver", etc. → maps to `driver.type`
- Device-level resources (via lookup): `manufacturer`, `model`, `hardwareVersion`, `firmwareVersion` — populated from BasicInformation (Matter) / Basic cluster (Zigbee) at pairing time

## Goals / Non-Goals

**Goals:**
- Emit per-device numeric gauges (temperature, humidity, illuminance, power, battery, level, position) with device/profile/resource attributes
- Emit per-device state transition counters (on/off, locked/unlocked, faulted/clear, occupied/vacant) with state value attributes
- Track resource update rates per device for detecting silent failures or excessive reporting
- Instrument Zigbee link quality (RSSI/LQI) as per-device gauges
- Instrument Matter subscription lifecycle (establishment, resubscription) as counters
- Keep all instrumentation behind `BARTON_CONFIG_OBSERVABILITY` compile guard
- Zero changes to individual Zigbee drivers or SBMD spec files

**Non-Goals:**
- Creating new ZCL attributes or Matter subscriptions
- Grafana dashboards or alerting rules
- Modifying the resource model, event system, or public GObject API
- Historical aggregation beyond what Prometheus provides
- Per-driver OTel instrumentation (the common path covers everything)

## Decisions

### Decision 1: Instrument `updateResource()` as the single telemetry point

All device resource changes from both Zigbee and Matter converge at `updateResource()` in `deviceService.c`. Rather than adding metrics to each of the 8 Zigbee drivers and 5 SBMD handling points, we instrument this one function.

**What gets emitted (conditional on resource type):**

| Resource Type | OTel Instrument | Metric Name | Attributes |
|---|---|---|---|
| `com.icontrol.integer`, `com.icontrol.percentage` | Gauge | `device.resource.value` | `device.id`, `device.class`, `endpoint.profile`, `resource.name`, `device.manufacturer`, `device.model`, `device.hardware_version`, `device.firmware_version`, `driver.type` |
| `com.icontrol.boolean` | Counter | `device.resource.state_change` | same as above + `state` |
| All types | Counter | `device.resource.update` | same as value gauge (without `state`) |

**Full attribute set for resource metrics:**

| Attribute | Source | Example Values |
|---|---|---|
| `device.id` | `resource->deviceUuid` | device UUID |
| `device.class` | `device->deviceClass` | "light", "sensor", "doorLock" |
| `endpoint.profile` | `endpoint->profile` | "light", "sensor", "thermostat" |
| `resource.name` | `resource->id` | "localTemperature", "isOn", "locked" |
| `device.manufacturer` | device resource "manufacturer" | "Philips", "Yale", "Eve" |
| `device.model` | device resource "model" | "LWA004", "YRD256" |
| `device.hardware_version` | device resource "hardwareVersion" | "1", "3" |
| `device.firmware_version` | device resource "firmwareVersion" | "2.1.0", "0x01020310" |
| `driver.type` | `device->managingDeviceDriver` | "zigbee", "matter" |
| `state` | new resource value (boolean metrics only) | "true", "false" |

**Why over per-driver instrumentation:**
- One instrumentation point vs. 13+ (8 Zigbee drivers + MatterDevice + cluster callbacks)
- Automatically covers future drivers and SBMD specs
- Consistent attribute tagging across all device types
- Dramatically less code to write and maintain

**Trade-off:** We lose protocol-specific context (e.g., which ZCL cluster originated the value). This is acceptable because the resource name (e.g., "localTemperature") is meaningful on its own, and operators care about the device-level view, not the protocol layer.

### Decision 2: Rich device metadata attributes with per-device caching

Rather than minimal attributes, we include a rich set of device metadata on every metric — device class, endpoint profile, manufacturer, model, hardware/firmware versions, and driver type. This enables powerful fleet queries like "average temperature by manufacturer" or "firmware version distribution across thermostats" without correlating separate data sources.

**Source of metadata:** Manufacturer, model, and version info are stored as device-level resources (populated from BasicInformation on Matter and Basic cluster on Zigbee at pairing time). Device class and managing driver are properties on the device object. Endpoint profile is a property on the endpoint.

**Caching strategy:** Looking up device resources on every `updateResource()` call would be expensive (database reads). Instead, we cache device metadata attributes per device UUID in a hash map on first access. The cache is populated via:
1. `deviceServiceGetEndpointById()` → endpoint profile (O(1))
2. `deviceServiceGetDeviceById()` → device class, managing driver (O(1))
3. Device resource lookups for manufacturer, model, hardwareVersion, firmwareVersion

The cache is invalidated/updated when a device's metadata resources change (firmware upgrade updates `firmwareVersion`). This keeps per-update overhead to a hash map lookup after the first access.

**Device-add sequence safety:** During device pairing, `updateResource()` fires for many resources before metadata (manufacturer, model) is fully populated. The cache **only persists entries that pass a completeness check** (device class and manufacturer both non-empty). Incomplete entries are returned for best-effort attribute tagging on that single call but are freed immediately and not stored. On subsequent calls after setup finishes, the lookup repopulates with complete data and caches normally. This avoids permanently caching empty/partial metadata from the add sequence.

**Why all these attributes rather than just profile:**
- Enables fleet-level PromQL grouping: `avg by (device_manufacturer) (device_resource_value{resource_name="localTemperature"})`
- Firmware version tracking: detect which firmware versions have anomalous behavior
- Protocol comparison: `driver.type` enables Zigbee vs. Matter analysis
- Endpoint profile remains included for multi-endpoint devices

### Decision 3: Separate Zigbee link quality metrics at `zigbeeDriverCommon.c`

RSSI and LQI values don't flow through `updateResource()` — they're collected via the diagnostics cluster and handled in `zigbeeDriverCommon.c` `handleRssiLqiUpdated()`. These need direct instrumentation at the callback level.

**Metrics:**
- `zigbee.device.rssi` (Gauge, dBm) — per-device received signal strength
- `zigbee.device.lqi` (Gauge, 1) — per-device link quality indicator (0-255)

**Attributes:** `device.id`

**Why not at updateResource():** RSSI/LQI are diagnostic values stored differently from standard resources. They flow through the diagnostics cluster callback, not the resource update path.

### Decision 4: Matter subscription and CASE session metrics at `DeviceDataCache.cpp`

Matter subscription events (establishment, resubscription, report cycles) and CASE session lifecycle are protocol-level signals that don't generate resource updates. These need instrumentation at the `DeviceDataCache` callback level.

**Subscription metrics:**
- `matter.subscription.established` (Counter) — subscription (re)establishment events
- `matter.subscription.report` (Counter) — subscription report cycles received
- `matter.subscription.error` (Counter) — subscription errors

**CASE session metrics (using existing callbacks):**
- `matter.case.session.duration` (Histogram, s) — time from `GetConnectedDevice()` call to `OnDeviceConnected` callback, capturing the full CASE handshake + discovery time
- `matter.case.session.error` (Counter) — connection failures from `OnDeviceConnectionFailure`, tagged with error code

**Attributes:** `device.id` for all; `error` attribute on `matter.case.session.error`

**Implementation:** Capture a monotonic clock timestamp before the `GetConnectedDevice()` call in `DeviceDataCache::Start()`. In `OnDeviceConnectedCallback()`, compute the elapsed duration and record to the histogram. In `OnDeviceConnectionFailureCallback()`, increment the error counter with the CHIP_ERROR as an attribute.

**Why here instead of `updateResource()`:** These are connection-health signals, not resource values. A device that stops reporting produces zero resource updates — the subscription error counter and CASE session error counter are the only ways to detect it.

**Why not Sigma-stage-level metrics:** The CHIP SDK exposes `SessionEstablishmentDelegate` with per-Sigma callbacks, but hooking those requires subclassing into the SDK's session establishment flow. The existing `OnDeviceConnected`/`OnDeviceConnectionFailure` callbacks in `DeviceDataCache` give us the most value (overall duration + success/failure) without SDK-layer surgery. Sigma-stage metrics can be a follow-up.

### Decision 5: Lazy meter creation to avoid startup cost for unused profiles

Not all resource types and profiles exist in every deployment. Creating OTel instruments eagerly for all possible combinations wastes memory. Instead, use a lazy pattern: create meters on first use and cache them.

```c
static ObservabilityGauge *resourceValueGauge = NULL;

static void ensureDeviceMetersCreated(void)
{
    if (resourceValueGauge == NULL)
    {
        resourceValueGauge = observabilityGaugeCreate(
            "device.resource.value", "Current numeric resource value", "1");
        // ... other meters
    }
}
```

**Why lazy:** The observability layer may not be initialized when the first driver registers. Lazy creation ensures meters are created only when actually needed and after OTel is ready.

### Decision 6: Cardinality controls via attribute selection

OTel metric cardinality is the product of unique attribute value combinations. With the rich attribute set, cardinality is still bounded:

- **`device.id`**: Bounded by fleet size (typically 10-100 devices in a home)
- **`device.class`**: Bounded by device classes (~10)
- **`endpoint.profile`**: Bounded by profile types (~10 known profiles)
- **`resource.name`**: Bounded by resources per profile (~5-15 per profile)
- **`device.manufacturer`**: Bounded by vendor count in fleet (~5-20)
- **`device.model`**: Bounded by model count (~10-50)
- **`device.hardware_version`**: Bounded (~1-5 per model)
- **`device.firmware_version`**: Bounded (~1-5 per model)
- **`driver.type`**: 2-3 values ("zigbee", "matter", "philipsHue")
- **`state`** (for state_change counter only): Bounded by possible states per resource (2-5)

The additional attributes don't multiply cardinality because they're fully determined by `device.id` — each device has exactly one manufacturer, one model, one firmware version, etc. The dimension explosion only happens if attributes are orthogonal; here they're hierarchically dependent on device ID.

Worst case: 100 devices × 15 resources = 1,500 time series — same as before, with richer labels on each series.

**What we deliberately exclude from attributes:** We do NOT include `resource.value` as an attribute (that would create unbounded cardinality for numeric gauges — the value is the metric data point, not a label). We do NOT include `serialNumber` or `macAddress` (redundant with `device.id` and potentially sensitive).

## Risks / Trade-offs

**[Endpoint profile lookup adds per-update overhead]** → `deviceServiceGetEndpointById()` is an O(1) hash map lookup. Post-caching, it's only called once per device. Negligible.

**[Device metadata resource lookups on first access]** → The first resource update for a new device triggers ~4 resource lookups (manufacturer, model, hardwareVersion, firmwareVersion) via `deviceServiceGetDeviceById()`. These are string values read from the in-memory device model. After caching, subsequent updates for the same device are a hash map lookup only.

**[Device metadata can change (firmware upgrade)]** → In practice, manufacturer/model never change and firmware version changes only on OTA upgrade (very rare). A simple approach: invalidate the cache entry when `firmwareVersion` resource is updated. Alternatively, accept slightly stale values for the PoC.

**[String comparison for resource type on every update]** → We compare `resource->type` against known type constants. This is a pointer comparison if constants are interned, or ~20-byte strcmp otherwise. At IoT reporting rates, this is invisible.

**[New OTel attribute values for every device joined]** → Device join is rare (minutes to days between events). New attribute values are handled by OTel's aggregator without issue. Device removal does not automatically prune time series — Prometheus handles staleness via its 5-minute mark.

**[RSSI/LQI collection interval is 30 minutes by default]** → This is configurable in `zigbeeDriverCommon.c` and cannot be changed from the metrics layer. The gauge will update infrequently. This matches the diagnostics collection design and is fine for fleet-level monitoring.

**[No backward API compatibility concerns]** → All changes are internal (conditional, `#ifdef`-guarded). No public GObject API, GIR interface, or signal changes. No impact on clients.

## Open Questions

1. ~~**Should we include `driver.type` ("zigbee"/"matter") as a metric attribute?**~~ **Resolved: YES.** Included in the standard attribute set. Low cardinality cost, high query value.
2. **Should the `device.resource.value` gauge emit for string-type resources?** Currently excluded because string values can't be numeric gauge values. Could parse known numeric-as-string resources (e.g., color "0.3,0.5") but adds complexity for marginal value.
3. **Should device metadata cache eviction be event-driven?** The cache could listen for device-updated events to invalidate on firmware upgrades. Alternatively, a simple TTL or no eviction (process-lifetime cache) may suffice for a PoC since firmware upgrades are rare.
