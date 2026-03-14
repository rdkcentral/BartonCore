# Observability Spans Reference

This document is the authoritative reference for all distributed tracing
spans emitted by BartonCore. It covers span hierarchies, context
propagation mechanisms, attributes, and error conditions for every
instrumented operation.

For build instructions, collector setup, and general observability
architecture see [OBSERVABILITY.md](OBSERVABILITY.md).

---

## Overview

All spans are created via the C adapter layer in `observabilityTracing.h`.
When `BARTON_CONFIG_OBSERVABILITY` is undefined every call compiles to a
zero-cost no-op inline stub.

### Context Propagation

Spans form parent-child hierarchies using three propagation mechanisms:

| Mechanism | Description |
|---|---|
| **Root** | New trace with no parent. Created via `observabilitySpanStart()`. |
| **TLS (thread-local storage)** | Child inherits parent from thread-local context set via `observabilitySpanContextSetCurrent()`. Created via `observabilitySpanStartWithParent(..., observabilitySpanContextGetCurrent())`. |
| **Stored context** | Child inherits parent from a context stored in a struct or object member, used when spans cross thread boundaries. |
| **Span link** | New root trace carrying an OTel link reference to another span, used when operations are causally related but have independent lifecycles. Created via `observabilitySpanStartWithLink()`. |

### Memory Management

Spans use two ownership patterns:

- **`g_autoptr`** — Automatic cleanup via GLib's scoped destructor. The span
  ends when the variable goes out of scope. Used for short-lived spans
  contained within a single function.
- **Manual** — Stored in struct/object members for long-lived spans that
  outlive the creating function (e.g., background thread operations,
  subscription lifecycles). Released explicitly via
  `observabilitySpanRelease()`.

---

## Span Hierarchy

```
subsystem.init                          (root)
matter.init                             (root, linked to subsystem.init)
├── matter.init.stack                   (child)
├── matter.init.server                  (child)
└── matter.init.commissioner            (child)
    ├── matter.fabric.create            (child, conditional)
    └── matter.init.advertise           (child)
subsystem.shutdown                      (root)
device.discovery                        (root)
└── device.found                        (child)
    ├── device.configure                (child)
    └── device.persist                  (child)
device.commission                       (root)
├── device.found                        (child)
│   ├── device.configure                (child)
│   └── device.persist                  (child)
├── matter.case.connect                 (child)
├── matter.subscribe                    (child)
└── matter.report                       (child)
    └── resource.update                 (child, Matter)
device.pair                             (root)
├── matter.case.connect                 (child)
├── matter.subscribe                    (child)
└── matter.report                       (child)
    └── resource.update                 (child, Matter)
device.remove                           (root)
├── device.driver.remove                (child)
└── matter.fabric.remove                (child, Matter)
device.commission_window.open           (root)
resource.read                           (root)
└── zigbee.cluster.read                 (child, Zigbee)
resource.write                          (root)
└── zigbee.cluster.write                (child, Zigbee)
    └── resource.update                 (child, Zigbee)
zigbee.device.discovered                (child of device.found)
```

---

## Core Lifecycle Spans

### `subsystem.init`

Traces the full subsystem initialization sequence including metric
creation, subsystem registration, and driver startup.

| Property | Value |
|---|---|
| **Location** | `subsystemManager.c` — `subsystemManagerInitialize()` |
| **Parent** | Root |
| **Attributes** | — |
| **Error conditions** | — |
| **Context propagation** | Context stored globally via `subsystemManagerGetInitSpanContext()` for `matter.init` span link |
| **Lifetime** | `g_autoptr` — ends when `subsystemManagerInitialize()` returns |

### `subsystem.shutdown`

Traces subsystem teardown.

| Property | Value |
|---|---|
| **Location** | `subsystemManager.c` — `subsystemManagerShutdown()` |
| **Parent** | Root |
| **Attributes** | — |
| **Error conditions** | — |
| **Context propagation** | — |
| **Lifetime** | `g_autoptr` — ends when `subsystemManagerShutdown()` returns |

---

## Matter Initialization Spans

These spans trace the Matter subsystem startup. The `matter.init` root span
is linked (not parented) to `subsystem.init` because Matter initialization
runs asynchronously on a separate thread with exponential backoff retries,
potentially seconds or minutes after `subsystem.init` completes.

### `matter.init`

Top-level Matter initialization span. Created on each attempt (including
retries).

| Property | Value |
|---|---|
| **Location** | `matterSubsystem.cpp` — `maybeInitMatter()` |
| **Parent** | Root (linked to `subsystem.init`) |
| **Attributes** | `retry.attempt` (int) — zero-based retry counter |
| **Error conditions** | `"matter initialization failed"` — set when init or start fails |
| **Context propagation** | TLS — set as current for child span inheritance, cleared on all exit paths |
| **Lifetime** | `g_autoptr` — ends when `maybeInitMatter()` returns |

### `matter.init.stack`

CHIP platform stack initialization including account ID retrieval and
attestation trust store setup.

| Property | Value |
|---|---|
| **Location** | `matterSubsystem.cpp` — `maybeInitMatter()` |
| **Parent** | TLS (`matter.init`) |
| **Attributes** | — |
| **Error conditions** | `"matter stack init failed"` — set when `Matter::Init()` returns false |
| **Context propagation** | Inherited from parent |
| **Lifetime** | `g_autoptr` — ends when Init block scope exits |

### `matter.init.server`

CHIP server initialization including static resource setup,
`Server::GetInstance().Init()`, and related configuration. Only created
when the server has not already been initialized in a previous attempt.

| Property | Value |
|---|---|
| **Location** | `Matter.cpp` — `Matter::Start()` |
| **Parent** | TLS (`matter.init`) |
| **Attributes** | — |
| **Error conditions** | CHIP error string via `err.AsString()` — set on `InitializeStaticResourcesBeforeServerInit` or `Server::Init` failure |
| **Context propagation** | Inherited from parent |
| **Lifetime** | `g_autoptr` — ends when `!serverIsInitialized` block scope exits |
| **Conditional** | Only created when `!serverIsInitialized` (skipped on retry after prior server success) |

### `matter.init.commissioner`

Commissioner controller setup including fabric table initialization,
credential issuance, and device controller factory setup.

| Property | Value |
|---|---|
| **Location** | `Matter.cpp` — `Matter::Start()` |
| **Parent** | TLS (`matter.init`) |
| **Attributes** | — |
| **Error conditions** | CHIP error string via `err.AsString()` — set when `InitCommissioner()` fails |
| **Context propagation** | Inherited from parent |
| **Lifetime** | `g_autoptr` — ends when commissioner block scope exits |

### `matter.fabric.create`

NOC chain generation and fabric creation. Only created on first run or when
no existing chain/fabric is found; skipped on subsequent startups that
already have a valid fabric.

| Property | Value |
|---|---|
| **Location** | `Matter.cpp` — `Matter::InitCommissioner()` |
| **Parent** | TLS (`matter.init.commissioner`) |
| **Attributes** | — |
| **Error conditions** | — |
| **Context propagation** | Inherited from parent |
| **Lifetime** | `g_autoptr` — ends when NOC chain generation block scope exits |
| **Conditional** | Only created when `chainAvailable == CHIP_ERROR_NOT_FOUND || !fabricFound` |

### `matter.init.advertise`

mDNS operational advertisement via `DnssdServer::AdvertiseOperational()`.

| Property | Value |
|---|---|
| **Location** | `Matter.cpp` — `Matter::InitCommissioner()` |
| **Parent** | TLS (`matter.init.commissioner`) |
| **Attributes** | — |
| **Error conditions** | — |
| **Context propagation** | Inherited from parent |
| **Lifetime** | `g_autoptr` — ends when advertise block scope exits |

---

## Device Discovery Spans

### `device.discovery`

Traces a driver discovery session started by `startDriverDiscoveryForDeviceClass()`.

| Property | Value |
|---|---|
| **Location** | `deviceService.c` — `startDriverDiscoveryForDeviceClass()` |
| **Parent** | Root |
| **Attributes** | `device.class` (string), `discovery.recovery_mode` (int: 0 or 1) |
| **Error conditions** | — |
| **Context propagation** | TLS — set as current so `deviceServiceDeviceFound` callbacks inherit it |
| **Lifetime** | `g_autoptr` — ends when discovery function returns |

### `device.found`

Traces a single device found during discovery or commissioning. Inherits
the `device.discovery` or `device.commission` context from TLS.

| Property | Value |
|---|---|
| **Location** | `deviceService.c` — `deviceServiceDeviceFound()` |
| **Parent** | TLS (from `device.discovery` or `device.commission`) |
| **Attributes** | `device.class` (string), `device.uuid` (string), `device.manufacturer` (string), `device.model` (string) |
| **Error conditions** | — |
| **Context propagation** | Context stored as `foundCtx` for child spans (`device.configure`, `device.persist`) |
| **Lifetime** | `g_autoptr` — ends when `deviceServiceDeviceFound()` returns |

### `device.configure`

Traces the driver-specific device configuration callback.

| Property | Value |
|---|---|
| **Location** | `deviceService.c` — `deviceServiceDeviceFound()` |
| **Parent** | `foundCtx` (from `device.found`) |
| **Attributes** | `device.class` (string) |
| **Error conditions** | `"device configuration failed"` — set when `configureFunc()` returns false |
| **Context propagation** | Inherited from parent |
| **Lifetime** | `g_autoptr` |

### `device.persist`

Traces database persistence of a discovered device.

| Property | Value |
|---|---|
| **Location** | `deviceService.c` — `deviceServiceDeviceFound()` |
| **Parent** | TLS (from `device.found` context) |
| **Attributes** | `device.uuid` (string) |
| **Error conditions** | `"database persist failed"` — set when `jsonDatabaseAddDevice()` returns false |
| **Context propagation** | Inherited from parent |
| **Lifetime** | `g_autoptr` |

---

## Device Commissioning & Pairing Spans

### `device.commission`

Traces a full commissioning operation. The span outlives the creating
function — it is passed to a background thread via a struct and released
when commissioning completes.

| Property | Value |
|---|---|
| **Location** | `deviceService.c` — `deviceServiceCommissionDevice()` |
| **Parent** | Root |
| **Attributes** | `commissioning.payload` (string), `commissioning.timeout_seconds` (int) |
| **Error conditions** | `"thread creation failed"` — set when `createDetachedThread()` returns false |
| **Context propagation** | Stored in `args->spanCtx` and passed to background thread |
| **Lifetime** | Manual — stored in `commissionMatterDeviceArgs`, released on completion or error |

### `device.pair`

Traces a device pairing operation. Same cross-thread pattern as
`device.commission`.

| Property | Value |
|---|---|
| **Location** | `deviceService.c` — `deviceServiceAddMatterDevice()` |
| **Parent** | Root |
| **Attributes** | `device.node_id` (int64), `pairing.timeout_seconds` (int) |
| **Error conditions** | `"thread creation failed"` — set when `createDetachedThread()` returns false |
| **Context propagation** | Stored in `args->spanCtx` and passed to background thread |
| **Lifetime** | Manual — stored in `OnboardMatterDeviceArgs`, released on completion or error |

### `device.commission_window.open`

Traces a commissioning window open request.

| Property | Value |
|---|---|
| **Location** | `deviceService.c` — `deviceServiceOpenCommissioningWindow()` |
| **Parent** | Root |
| **Attributes** | `device.node_id` (string), `window.timeout_seconds` (int) |
| **Error conditions** | `"open commissioning window failed"` — set when `matterSubsystemOpenCommissioningWindow()` returns false |
| **Context propagation** | — |
| **Lifetime** | `g_autoptr` |

---

## Device Removal Spans

### `device.remove`

Traces a device removal including driver callback and database cleanup.

| Property | Value |
|---|---|
| **Location** | `deviceService.c` — `deviceServiceRemoveDevice()` |
| **Parent** | Root |
| **Attributes** | `device.uuid` (string) |
| **Error conditions** | `"database removal failed"` — set when `jsonDatabaseRemoveDeviceById()` returns false |
| **Context propagation** | Context stored as `removeCtx` for child span `device.driver.remove` |
| **Lifetime** | `g_autoptr` |

### `device.driver.remove`

Traces the driver-specific device removal callback. Sets TLS for the
duration of the callback so subsystem-specific child spans (e.g.,
`matter.fabric.remove`) can inherit the context.

| Property | Value |
|---|---|
| **Location** | `deviceService.c` — `deviceServiceRemoveDevice()` |
| **Parent** | `removeCtx` (from `device.remove`) |
| **Attributes** | — |
| **Error conditions** | — |
| **Context propagation** | TLS — set for driver callback duration, cleared after |
| **Lifetime** | `g_autoptr` |

---

## Resource Operation Spans

### `resource.read`

Traces a resource read operation by URI.

| Property | Value |
|---|---|
| **Location** | `deviceService.c` — `deviceServiceGetResourceByUri()` |
| **Parent** | Root |
| **Attributes** | `resource.uri` (string) |
| **Error conditions** | `"resource not accessible"` — set when `deviceServiceIsUriAccessible()` returns false |
| **Context propagation** | TLS — set for driver read callback, cleared on all exit paths |
| **Lifetime** | `g_autoptr` |

### `resource.write`

Traces a resource write operation by URI.

| Property | Value |
|---|---|
| **Location** | `deviceService.c` — `deviceServiceWriteResource()` |
| **Parent** | Root |
| **Attributes** | `resource.uri` (string) |
| **Error conditions** | `"write failed"` — set on write errors or inaccessible resource |
| **Context propagation** | TLS — set for driver write callback, cleared on all exit paths |
| **Lifetime** | `g_autoptr` |

### `resource.update`

Traces a resource update originating from a device report (Matter) or a
write callback (Zigbee). Created in two locations depending on the
subsystem.

| Property | Value |
|---|---|
| **Location (Matter)** | `MatterDevice.cpp` — `MatterDevice::CacheCallback::OnAttributeData()` |
| **Location (Zigbee)** | `zigbeeDriverCommon.c` — `writeResource()` |
| **Parent** | TLS (from `matter.report` or `zigbee.cluster.write`) |
| **Attributes** | `device.uuid` (string), `resource.name` (string), `endpoint.profile` (string, conditional on non-NULL endpoint ID) |
| **Error conditions** | — |
| **Context propagation** | Inherited from parent |
| **Lifetime** | `g_autoptr` |

---

## Matter Subsystem Spans

### `matter.case.connect`

Traces a CASE (Certificate Authenticated Session Establishment) session
setup with a Matter device.

| Property | Value |
|---|---|
| **Location** | `DeviceDataCache.cpp` — `DeviceDataCache::Start()` |
| **Parent** | Stored context (`spanCtx` member) |
| **Attributes** | — |
| **Error conditions** | `"GetConnectedDevice failed"` — set when connection fails |
| **Context propagation** | TLS set from stored `spanCtx` before span creation |
| **Lifetime** | Manual — stored in `caseConnectSpan` member, released on completion or error |

### `matter.subscribe`

Traces a Matter subscription setup with a connected device.

| Property | Value |
|---|---|
| **Location** | `DeviceDataCache.cpp` — subscription setup callback |
| **Parent** | Stored context (`spanCtx` member) |
| **Attributes** | — |
| **Error conditions** | — |
| **Context propagation** | Implicit via `spanCtx` |
| **Lifetime** | Manual — stored in `subscribeSpan` member |
| **Conditional** | Only created on successful `readClient->SendAutoResubscribeRequest()` |

### `matter.report`

Traces a single Matter subscription report cycle (a batch of attribute
data from a device).

| Property | Value |
|---|---|
| **Location** | `DeviceDataCache.cpp` — `DeviceDataCache::OnReportBegin()` |
| **Parent** | Stored context (`spanCtx` member) |
| **Attributes** | `device.id` (string) |
| **Error conditions** | — |
| **Context propagation** | TLS set from stored `spanCtx` for child `resource.update` spans |
| **Lifetime** | Manual — stored in `reportSpan` member |

### `matter.fabric.remove`

Traces fabric removal during device removal.

| Property | Value |
|---|---|
| **Location** | `MatterDeviceDriver.cpp` — `MatterDeviceDriver::DeviceRemoved()` |
| **Parent** | TLS (from `device.driver.remove`) |
| **Attributes** | `device.uuid` (string) |
| **Error conditions** | — |
| **Context propagation** | Inherited from parent |
| **Lifetime** | `g_autoptr` |

---

## Zigbee Subsystem Spans

### `zigbee.device.discovered`

Traces a Zigbee device discovery callback.

| Property | Value |
|---|---|
| **Location** | `zigbeeDriverCommon.c` — device discovered callback |
| **Parent** | TLS (from `device.found`) |
| **Attributes** | `device.eui64` (string), `device.manufacturer` (string) |
| **Error conditions** | — |
| **Context propagation** | Inherited from parent |
| **Lifetime** | `g_autoptr` |

### `zigbee.cluster.read`

Traces a Zigbee cluster read operation during a resource read.

| Property | Value |
|---|---|
| **Location** | `zigbeeDriverCommon.c` — `readResource()` |
| **Parent** | TLS (from `resource.read`) |
| **Attributes** | `device.uuid` (string), `resource.uri` (string) |
| **Error conditions** | — |
| **Context propagation** | Inherited from parent |
| **Lifetime** | `g_autoptr` |

### `zigbee.cluster.write`

Traces a Zigbee cluster write operation during a resource write. Sets
TLS for the callback so the child `resource.update` span inherits the
context.

| Property | Value |
|---|---|
| **Location** | `zigbeeDriverCommon.c` — `writeResource()` |
| **Parent** | TLS (from `resource.write`) |
| **Attributes** | `device.uuid` (string), `resource.uri` (string) |
| **Error conditions** | — |
| **Context propagation** | TLS — set for write callback, cleared after |
| **Lifetime** | `g_autoptr` |

---

## Span Attribute Reference

All attributes used across spans, grouped by semantic domain.

### Device Attributes

| Attribute | Type | Description | Used by |
|---|---|---|---|
| `device.class` | string | Device class identifier (e.g., "light", "sensor") | `device.discovery`, `device.found`, `device.configure` |
| `device.uuid` | string | Device unique identifier | `device.found`, `device.persist`, `device.remove`, `resource.update`, `matter.fabric.remove`, `zigbee.cluster.read`, `zigbee.cluster.write` |
| `device.manufacturer` | string | Device manufacturer name | `device.found`, `zigbee.device.discovered` |
| `device.model` | string | Device model name | `device.found` |
| `device.id` | string | Matter device identifier | `matter.report` |
| `device.eui64` | string | Zigbee EUI-64 address | `zigbee.device.discovered` |
| `device.node_id` | string / int64 | Matter node identifier | `device.commission_window.open`, `device.pair` |

### Resource Attributes

| Attribute | Type | Description | Used by |
|---|---|---|---|
| `resource.uri` | string | Full resource URI path | `resource.read`, `resource.write`, `zigbee.cluster.read`, `zigbee.cluster.write` |
| `resource.name` | string | Resource name within endpoint | `resource.update` |
| `endpoint.profile` | string | Endpoint profile identifier (conditional) | `resource.update` |

### Operation Attributes

| Attribute | Type | Description | Used by |
|---|---|---|---|
| `retry.attempt` | int | Zero-based retry counter | `matter.init` |
| `discovery.recovery_mode` | int | Whether discovery is in recovery mode (0 or 1) | `device.discovery` |
| `commissioning.payload` | string | Commissioning payload string | `device.commission` |
| `commissioning.timeout_seconds` | int | Commissioning timeout | `device.commission` |
| `pairing.timeout_seconds` | int | Pairing timeout | `device.pair` |
| `window.timeout_seconds` | int | Commissioning window timeout | `device.commission_window.open` |
