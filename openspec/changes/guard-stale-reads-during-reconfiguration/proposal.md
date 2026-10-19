## Why

Public query APIs (`b_core_client_get_device_by_id()`, `get_resource_by_uri()`, etc.) return
device data without checking whether the service — or the specific device being asked about — is
in a consistent state. When a device undergoes reconfiguration (device-class version change,
endpoint profile version revision, or a `reconfigurationRequired` metadata flag), the service
rebuilds the device in the background: it removes the old `icDevice` from the database and persists
a freshly constructed one. A client that queries that device during this window receives data that
is valid at read time but almost immediately stale. This has been observed in Zilker during
startup, where one service reconfigures a device while another queries it.

Crucially, the existing `READY_FOR_DEVICE_OPERATION` status event does **not** protect against
this: that event is emitted *before* startup device initialization/reconfiguration begins, and
`deviceServiceIsReadyForDeviceOperation()` never returns `false` again until shutdown. A client
that correctly awaits readiness can still read a device mid-reconfiguration.

## What Changes

- Add a new error domain to the public client API covering service/device state:
  `B_CORE_SERVICE_STATE_ERROR_NOT_READY` and `B_CORE_SERVICE_STATE_ERROR_DEVICE_RECONFIGURING` (error-code range 300–399).
- **BREAKING**: Add `GError **err` to the single-device functions that lack an error channel
  (`get_device_by_id`, `get_device_by_uri`, `get_endpoint_by_id`, `get_endpoint_by_uri`,
  `get_resource_by_uri`, `write_metadata`). `read_resource` and `read_metadata` already have one.
  This is an ABI/API break requiring a major version bump, already anticipated by the error-code
  namespacing TODO in `barton-core-client.h`.
- Those eight functions SHALL fail with `B_CORE_SERVICE_STATE_ERROR_NOT_READY` until the service reports itself
  ready for device operation, and with `B_CORE_SERVICE_STATE_ERROR_DEVICE_RECONFIGURING` when the targeted device has
  a reconfiguration pending or in progress.
- List getters, `write_resource` and `execute_resource` are **not** gated and keep their signatures.
  A reconfiguring device still exists and is still operable, and an empty list before readiness would
  look like "no devices" to clients that mirror the list (e.g. a cloud sync that deletes what is
  missing). `write_metadata` is gated because reconfiguration copies metadata from a snapshot taken
  when it starts, so a write made while pending would be silently lost.
- Announce when a reconfiguration finishes (`device-configuration-completed`/`-failed`, session
  discovery type `NONE`). Reconfiguration was previously silent, so clients had no signal to re-read
  a device whose data had just changed.
- Gating lives exclusively in the public API layer (`api/c/src/barton-core-client.c`); the internal
  `deviceServiceGet*` backend remains ungated because reconfiguration itself depends on it.
- Close the startup race: flag devices needing reconfiguration as pending up front, and only then
  publish readiness (status `isReadyForDeviceOperation`, driver notification, and the
  `READY_FOR_DEVICE_OPERATION` event), so no observer sees "ready" before affected devices are gated.
- Make the pending state trustworthy, since reads now depend on it: fix a use-after-free/data race on
  the reconfiguration task handle when a zero-delay task finishes before scheduling returns (and the
  related window in which a removal could not cancel a just-scheduled reconfiguration), clear the
  pending state if scheduling fails, release the context reference that the cancel paths leaked, and
  stop treating a reconfiguration that had already started as cancelled. Report the number of pending
  reconfigurations as a metric.
- Make the reconfiguration timed waits actually block (they used the wrong clock and returned at
  once), so shutdown really waits for a running reconfiguration and removal and sleepy-device waits
  no longer busy-spin.
- Do not publish readiness if shutdown begins during the startup pass.

## Capabilities

### New Capabilities
<!-- None. This change modifies existing API and core-service behavior. -->

### Modified Capabilities
- `public-api`: Single-device reads and `write_metadata` gain readiness/reconfiguration preconditions
  and a new error domain; six signatures gain `GError **err` (breaking). Reconfiguration completion is
  announced through the existing configuration signals.
- `core-services`: Readiness is published only after devices requiring startup reconfiguration are
  marked pending; the pending-reconfiguration state is cleared on every exit path.

## Impact

- **Affected layers**: Public API (`api/c/public/barton-core-client.h`,
  `api/c/src/barton-core-client.c`); Core service (`core/src/deviceService.c` readiness, reconfiguration
  flagging and scheduling; `core/src/device/icDevice.c` URI parsing;
  `core/src/event/deviceEventProducer.c` internal device lookups and the completion event).
- **Affected APIs**: Breaking signature changes on the six functions above; new
  `BCoreServiceStateError` enum and quark usage. `deviceServiceIsReadyForDeviceOperation()` and the
  status's `isReadyForDeviceOperation` turn true slightly later (after the startup flagging pass).
- **Consumers**: C callers (the in-repo reference app, and out-of-repo consumers such as Zilker) must
  pass the new `GError **` argument. Python/GObject-Introspection callers need no code changes beyond
  handling `GLib.Error`; the GIR typelib regenerates on install. See design.md "Migration Plan" for
  consumer guidance, including Zilker-specific follow-ups.
- **CMake flags**: No new flags. Behavior is protocol-agnostic and applies regardless of
  `BCORE_ZIGBEE`, `BCORE_MATTER`, or `BCORE_THREAD`.
- **Versioning**: Major version bump required due to the breaking API/ABI change.

## Non-goals

- Making the reconfiguration database swap itself atomic (removal + re-persist) — this change
  surfaces an error/exclusion to callers rather than redesigning persistence.
- Changing reconfiguration triggers or the reconfiguration algorithm.
- Adding retry/backoff logic in clients; callers decide how to react to `B_CORE_SERVICE_STATE_ERROR_DEVICE_RECONFIGURING`.
- Gating purely internal (non-public) call paths.
- Gating list getters, resource writes/executes, `change_resource_mode()`, or
  `open_commissioning_window()` (see design.md D4/D5).
