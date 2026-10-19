## Context

`BCoreClient` is the public GObject API through which clients query and mutate device state. Most
query functions (`b_core_client_get_device_by_id()`, `get_device_by_uri()`, `get_endpoint_by_id()`,
`get_endpoint_by_uri()`, `get_resource_by_uri()`, and the list getters) call directly into the
internal `deviceServiceGet*` backend and return a GObject or `GList*`, with no notion of service or
device state. Only `read_resource()` and `read_metadata()` currently expose a `GError **`.

Reconfiguration (`reconfigureDeviceTask()` in `core/src/deviceService.c`) rebuilds a device in a
background thread pool: it constructs a new `icDevice`, then `jsonDatabaseRemoveDeviceById()` the old
one and `finalizeNewDevice()` the new one. During that window a concurrent read can observe the old
device (about to be replaced), nothing, or a partially-updated view — all "stale".

Current readiness flow in `subsystemManagerReadyForDevicesCallback()`:

```
set subsystemsReadyForDevices = true
sendDeviceServiceStatusEvent(ReadyForDeviceOperation)   <-- clients unblock here
startDeviceInitialization()                             <-- queues async per-device reconfig
```

`deviceServiceIsReadyForDeviceOperation()` returns `true` from that point until shutdown, so a
global readiness check cannot detect an in-progress reconfiguration. A per-device primitive already
exists — `deviceServiceIsReconfigurationPending(uuid)`, backed by the `pendingReconfiguration`
hash map, already used by `deviceServiceRemoveDevice()` via `waitForOrCancelPendingReconfiguration()`.

## Goals / Non-Goals

**Goals:**
- Public single-device reads never hand back data for a device that is mid-reconfiguration; instead
  they return a distinct, actionable error (`B_CORE_SERVICE_STATE_ERROR_DEVICE_RECONFIGURING`).
- The same reads return `B_CORE_SERVICE_STATE_ERROR_NOT_READY` before the service is ready for device operation.
- Do not make anything worse for clients that ignore the new errors: no empty lists, no refused commands.
- Close the startup race so no observer sees "ready" before affected devices are gated.

**Non-Goals:**
- Making the reconfiguration DB swap atomic.
- Changing reconfiguration triggers or algorithm.
- Gating internal (non-public) call paths.

## Decisions

### D1: Gate in the public API layer, never the internal backend
`reconfigureDeviceTask()` itself calls `deviceServiceGetDevice(uuid)` to build the replacement
device. Gating `deviceServiceGetDevice()` / `deviceServiceGetResourceByUri()` would break
reconfiguration. Therefore all gating lives in `api/c/src/barton-core-client.c`, which calls the
existing readiness/pending predicates before delegating to the backend. Internal callers that
consume device data (e.g. `deviceEventProducer.c` emitting `device-added`/`device-recovered`) use the
backend directly rather than the gated public getters.

The gate finds the owning device of a URI with `deviceUriGetDeviceUuid()`, which lives in
`core/src/device/icDevice.c` next to `deviceUriIsValid()` and the other URI builders, so the URI
layout rule is coded once in core rather than re-derived in the API layer.

```
   client (Python / reference app / cloud)
        | g_object / GError **
        v
  +-------------------------------+
  |  barton-core-client.c         |   <-- NEW: readiness + per-device gate here
  |  b_core_client_get_*()        |
  +-------------------------------+
        | (only when allowed)
        v
  +-------------------------------+
  |  core/src/deviceService.c     |   <-- UNCHANGED gating; used by reconfig task
  |  deviceServiceGet*            |
  +-------------------------------+
```

### D2: New error domain `BCoreServiceStateError`, range 300–399
Follows the existing interval-of-100 convention in `barton-core-client.h` (ReadResource 1–99,
Zigbee 100–199, ReadMetadata 200–299). New codes: `B_CORE_SERVICE_STATE_ERROR_NOT_READY = 300`,
`B_CORE_SERVICE_STATE_ERROR_DEVICE_RECONFIGURING = 301`, emitted via the existing `B_CORE_CLIENT_ERROR` quark.

### D3: Add `GError **err` only to gated functions that lack one (BREAKING)
The gated functions are `get_device_by_id/uri`, `get_endpoint_by_id/uri`, `get_resource_by_uri`,
`read_resource`, `read_metadata` and `write_metadata`; six of them gain `GError **err`
(`read_resource`/`read_metadata` already had it). This is the only mechanism that lets callers
distinguish "busy, retry" from "not found" (`NULL`). The header already carries a TODO anticipating a
breaking major bump for error-code namespacing, so this aligns. Alternatives considered and rejected:
- *Return `NULL` only*: cannot distinguish reconfiguring from not-found; silent data loss.
- *Signal-only (emit an event)*: asynchronous, racy, and doesn't bind the error to the call.
- *`GError **` on every getter/mutator for uniformity*: functions that are never gated (D4, D5)
  would carry an error that never fires, breaking every caller for no behavior.

The new `GError **` reports only the two service-state codes. A missing target, or a backend write
that fails, still returns `NULL`/`FALSE` without setting it, as before this change (`read_resource`
already returns `NULL` without an error for a missing resource). This departs from the GLib
convention that a `NULL`/`FALSE` return from a function taking a `GError **` always sets it. Adding
codes such as "not found" and "write failed" was considered and deliberately left out: callers that
treat `NULL`/`None` as "not found" would then get errors (in Python, exceptions) for an ordinary miss,
which is a further behavior change beyond what stale-read protection needs. Callers distinguish the
outcomes by checking `err` first: set means retry later, unset with `NULL`/`FALSE` means not found or
failed.

### D4: Single-device reads are gated; list getters are not
- Single-device reads (`get_device_by_id/uri`, `get_endpoint_by_id/uri`, `get_resource_by_uri`,
  `read_resource`, `read_metadata`) check readiness, then resolve the owning device UUID; if it is
  pending → `B_CORE_SERVICE_STATE_ERROR_DEVICE_RECONFIGURING`. Readiness is checked first, so a call that fails both
  reports `NOT_READY`.
- List getters (`get_devices`, `get_devices_by_device_class`, `get_devices_by_subsystem`,
  `get_endpoints_by_profile`, `get_metadata_by_uri`, `query_resources_by_uri`) are not gated at all
  and keep their signatures. They include reconfiguring devices: such a device still exists (it can
  be removed, operated, and has metadata), so omitting it makes "busy" indistinguishable from
  "removed" with no accompanying `device-removed` event. Clients that reconcile their own records
  against a list would delete it, and because reconfiguration used to announce nothing, a client whose
  initial load missed the device would never learn it exists.
- List getters do not get a `NOT_READY` gate either. The failure mode is the same: a caller that
  ignores the error sees an empty list, which looks like "no devices". This is not hypothetical:
  Zilker's cloud thing helper loads the device list at startup without waiting for readiness, and its
  IPC handler drops list errors, so a gated list would be read as zero devices and the cloud copies
  deleted. Before readiness the database is already loaded, so the list is accurate apart from details
  that a pending reconfiguration is about to change, which D7 makes recoverable.

### D5: Gate metadata writes, not resource writes or commands
- `write_resource` and `execute_resource` are not gated and keep their signatures. They act on the
  live device, and the data they change survives the swap: the rebuilt device re-reads resource values
  from the device, and labels are re-read at swap time. A device is "pending" from the moment
  reconfiguration is *scheduled*, which includes the 60 s post-OTA delay and, for sleepy Zigbee
  devices, up to `RECONFIGURATION_TIMEOUT_SEC` (30 min) waiting for a check-in. Refusing commands for
  that long (door locks, thermostats, security keypads) would cost more than it protects. Gating
  wildcard writes would also make broadcast writes (e.g. a security state sent to every keypad)
  all-or-nothing whenever any one matched device was pending.
- `write_metadata` is gated. `yoinkCustomizedDeviceData()` copies device and endpoint metadata from
  the snapshot `reconfigureDeviceTask()` takes when it starts, so a metadata write made while pending
  would be overwritten by the swap and silently lost. Refusing it with a retryable error is strictly
  better. Its error contract is unchanged otherwise: a backend failure still returns `FALSE` without an
  error, as `write_resource`/`execute_resource` do; a dedicated "write failed" code is out of scope.
- `read_resource` stays gated for the whole pending window, even though writes don't. It resolves the
  stored resource record and, except for never-cache readable resources (which the driver reads live),
  returns that record's stored value. The record, its value, and the endpoint and resource identities
  it resolves are exactly what the swap replaces.
- `change_resource_mode` and `open_commissioning_window` are not gated. The rebuilt device takes
  resource modes from the driver, so a mode change is lost on reconfiguration whenever it was made;
  gating doesn't help. Opening a commissioning window doesn't read or write device data.

### D6: Close the startup window
Readiness is split into two flags under `subsystemsAreReadyMutex`:
- `subsystemsReadyForDevices` (internal): subsystems can serve devices. Set first, and used by the
  startup reconfiguration's failure fallback, which can run during the pass. That fallback queues the
  normal `deviceSynchronizationTask()` instead of calling the driver itself, so the device stops being
  pending without waiting for the synchronization, and drivers that don't implement
  `synchronizeDevice()` are skipped as they are on every other synchronization path.
- `deviceOperationsReady` (published): what `deviceServiceIsReadyForDeviceOperation()` returns, and
  therefore the client gate, the status's `isReadyForDeviceOperation`/`isReadyForPairing`, and
  discovery's driver filtering.

`subsystemManagerReadyForDevicesCallback()` now runs:

```
set subsystemsReadyForDevices = true
startDeviceInitialization()        <-- marks every device needing reconfiguration pending (synchronously)
if not shutting down (checked under subsystemsAreReadyMutex):
    set deviceOperationsReady = true    <-- readiness becomes observable here, and only here
    notifyDriversForServiceStatusChange()
    sendDeviceServiceStatusEvent(ReadyForDeviceOperation)
```

No observer can see "ready" while a device awaiting startup reconfiguration is unflagged, whether it
waits for the event, polls the status, or calls a gated function directly; before that point it gets
`NOT_READY`. Setting a single flag later was rejected because the fallback above needs subsystem
readiness during the pass. Driver notification moved after the pass because drivers read the status
they are given (e.g. Zilker's M1 LTE driver starts boot discovery only when it reports
`isReadyForPairing`), so notifying earlier would hand them a not-ready status.

As a consequence, startup synchronization and zero-delay reconfiguration can call a driver's
`synchronizeDevice()`/`configureDevice()` before its `serviceStatusChanged()` sees the ready status;
previously that notification came first. No known driver depends on that order: in this repository
only the Zigbee common driver wires `serviceStatusChanged()` and no Zigbee driver implements the
callback it forwards to; in Zilker only the M1 LTE driver does, and it only starts discovery.

Shutdown can begin while the pass is running (a stop during startup). The pass stops scheduling once
`deviceServiceIsShuttingDown()` is true, and readiness is published only if shutdown has not begun.
That check runs under `subsystemsAreReadyMutex`, and shutdown sets its flag before clearing both
readiness flags under the same mutex, so readiness is never re-published after shutdown clears it. A
ready callback that arrives after shutdown has begun (e.g. a subsystem finishing a delayed
initialization) is ignored entirely, so it can't re-raise `subsystemsReadyForDevices` either.

The subsystem manager can invoke the ready callback more than once (from both subsystem
initialization and "all drivers started", and again if a subsystem re-initializes). That is
pre-existing, and no run-once guard was added because a repeated pass after a subsystem restart may be
wanted. A repeated pass is safe: a device whose snapshot says it needs reconfiguring is skipped if it
is already pending (an earlier pass is reconfiguring it, which covers synchronization; it stays pending
through the database swap), and otherwise re-checked against its current record before scheduling (so a
pass holding a snapshot taken before an earlier pass's reconfiguration finished does not reconfigure
it again), and readiness is re-published idempotently.

`startDeviceInitialization()` already holds every device from the database, so it decides with an
internal `DeviceNeedsReconfiguring(const icDevice *)` instead of re-fetching each device; the public
`deviceServiceDeviceNeedsReconfiguring(uuid)` wraps it. The per-device synchronization task is
renamed `deviceSynchronizationTask()`, since that is all it now does.

### D7: Announce when a reconfiguration finishes
Previously a reconfiguration was silent: the rebuilt device is persisted with events disabled
(`finalizeNewDevice(newDevice, false, false)`), so a client that read a device before or during
reconfiguration had no signal to re-read it. `reconfigureDeviceTask()` now emits
`device-configuration-completed` on success or `device-configuration-failed` on failure, with session
discovery type `B_CORE_DISCOVERY_TYPE_NONE` to mark it as outside any discovery session.
- Emitted after the reconfiguration context is removed, so a listener can query the device
  immediately without hitting `B_CORE_SERVICE_STATE_ERROR_DEVICE_RECONFIGURING`. The finishing steps (completion
  callback while still pending → context removal → event) live in one helper,
  `FinishReconfiguration()`, which makes that ordering explicit.
- Skipped when the service is shutting down (the reconfiguration was abandoned, not failed), and
  when the device record no longer exists (listeners compare the device class, which must not be NULL).
- Startup reconfigurations run with no delay, so one can finish while the D6 pass is still running
  and its event can arrive *before* `READY_FOR_DEVICE_OPERATION`. Nothing is lost: a client that waits
  for readiness and then queries sees the reconfigured data. The header's signal comment notes this.
- Reuses the existing configuration signals rather than adding a new signal type: no new API
  surface, and existing generic listeners receive it. Alternative considered: a dedicated
  `device-reconfigured` signal, rejected as new public surface that duplicates the existing events'
  payload. Only terminal events are sent; a `-started` event at scheduling time was left out because
  the gate already reports in-progress state on demand.

### D8: The pending state must not outlive the scheduled work
Reads are now refused while a device is pending, so a pending entry that is never cleared makes the
device permanently unreadable. Every exit path from `deviceServiceReconfigureDevice()` therefore
clears it:
- Scheduling failure: when `scheduleDelayTask()` reports failure (returns 0), the context is removed
  from the pending map and the task's reference released. Previously the entry stayed forever. A
  failed map insert likewise releases both references instead of leaking them. Limitation: a thread
  that fails to start after `scheduleDelayTask()` returns a handle is not detected, so in that rare
  case the device stays pending until it is removed or the service restarts.
- Zero-delay race (pre-existing use-after-free): the task owns one reference and the pending map
  another, and the scheduler held none, so a task that finished before `scheduleDelayTask()` returned
  (e.g. device removed, required resources missing) freed the context before `ctx->taskHandle` was
  written. The write was also unsynchronized with readers that take `ctx->mtx`. The scheduler now
  holds its own reference, and holds `ctx->mtx` from before the device becomes pending until the
  handle is stored. The startup path in D6 uses delay 0, so this change makes the race far more
  reachable.
- Removal right after scheduling (pre-existing): a removal that ran between the device becoming
  pending and the handle being stored read handle 0, could not cancel the not-yet-run task, and fell
  back to signalling and waiting for it. Because the scheduler now holds `ctx->mtx` across both steps,
  the removal waits for the handle and cancels the task.
- Cancellation acts only on a successful cancel (pre-existing): both cancel paths checked
  `isDelayTaskWaiting()` and then called `cancelDelayTask()`, ignoring whether the cancel took effect.
  A task that started in between lost its pending entry while still running, which reopened the gate
  mid-reconfiguration, let a removal return before the task re-persisted the device, and let
  shutdown stop waiting for it. `CancelReconfigurationTask()` now treats the cancel as successful only
  if `cancelDelayTask()` returned the task's reference (which is then released, fixing a leak as well);
  otherwise the task is told to terminate and, on removal, waited for.
- Handle reuse: the delayed-task library frees a task's record once the task finishes or is
  cancelled, and reuses handles. The handle in the context is therefore cleared under `ctx->mtx`
  whenever the task stops owning it: by `FinishReconfiguration()` before un-pending the device, and by
  `CancelReconfigurationTask()` after a successful cancel. `CancelReconfigurationTask()` cancels under
  that same mutex, so neither a second canceller (e.g. a removal racing shutdown) nor a canceller
  racing the task's completion can cancel with a freed or reused handle.
- Removal checks identity: `deviceServiceRemoveReconfigureDeviceContext()` removes the entry only if
  it is still the given context, so a finishing task can never un-pend a newer reconfiguration of the
  same device.
- No inserts once shutdown starts: the map insert refuses while shutting down. Shutdown sets its flag
  before snapshotting the map for cancellation, so a startup pass racing shutdown cannot add a
  reconfiguration that is neither cancelled nor signalled. A refused or duplicate insert is benign
  and logged at debug level.
- Visibility: a `device.reconfiguration.pending` gauge (observability metrics, no labels) reports the
  number of pending reconfigurations, so a device stuck pending (e.g. the thread-start limitation
  above) shows up in telemetry rather than only as refused reads.
- Timed waits actually wait (pre-existing): `bartoncommon`'s `incrementalCondTimedWait()` computes a
  `CLOCK_MONOTONIC` deadline and requires condition variables made with `initTimedWaitCond()`, but
  both reconfiguration condition variables used the default `CLOCK_REALTIME`, so every timed wait
  returned at once. Shutdown therefore never waited for a running reconfiguration, the removal
  fallback busy-spun for up to 60 s, and a sleepy device's reconfiguration busy-spun a core for up to
  30 min. Both now use `initTimedWaitCond()` (the shared one once, via `pthread_once`). Because the
  waits now block, two latent flaws were fixed with it: the removal fallback checks under the control
  lock that its context is still pending *before* each wait (otherwise a task finishing before the
  wait began would stall the removal for the full timeout), and the shutdown wait uses an overall
  deadline of 2 × 60 s instead of counting wake-ups (every completion broadcasts, so unrelated
  completions would otherwise end it early). Shutdown can now take up to 120 s longer when a
  reconfiguration is mid-flight, which is what the code always intended. Only sleepy-device waits
  honor the terminate signal; a non-sleepy `configureDevice()` runs to its own timeout. A host whose
  stop budget is shorter will kill the process during that wait. Zilker's watchdog, for example,
  allows 5 s unless `waitOnShutdown` is configured, and none of its local configs set it. That is
  safe: until the final swap the old record is intact on disk with `reconfigurationRequired` set
  (the task sets it first), so the next start reconfigures the device again. The swap itself deletes
  the device's file and re-saves the rebuilt device milliseconds later, with only in-memory work in
  between; a kill, crash or power loss in that instant loses the device. That exposure is pre-existing
  and independent of shutdown timing (making the swap atomic is a non-goal). The bound stays at 120 s,
  since Barton cannot know each host's budget; a longer budget only lets the reconfiguration finish
  instead of being retried.

### Thread safety
- `subsystemsReadyForDevices` and `deviceOperationsReady` are read and written under
  `subsystemsAreReadyMutex`.
- `pendingReconfiguration` is accessed under `reconfigurationControlMutex`; the public-layer gate
  uses the existing `deviceServiceIsReconfigurationPending()` accessor, which takes that lock, so no
  new locking is introduced in the client layer.
- All gating runs on the caller's thread; no GLib main-loop or Matter-event-loop interaction.
- D6 decides which devices need reconfiguration synchronously inside the subsystem-ready callback
  (previously on the initializer thread pool). The subsystem manager invokes that callback without
  holding its lock, and the in-tree drivers' `getDeviceClassVersion()` only returns a stored field,
  so this cannot deadlock. The cost is one database metadata read per device before readiness is
  published.
- D8 holds `ctx->mtx` while publishing the context to the pending map (which takes
  `reconfigurationControlMutex`) and across `scheduleDelayTask()`. The only lock order is therefore
  `ctx->mtx` → `reconfigurationControlMutex` → nothing, and `ctx->mtx` → the delayed-task lock. The
  one place that used to nest the other way, `cancelPendingReconfigurationTasks()`, now snapshots the
  pending contexts (taking references) under the control lock and inspects and cancels them after
  releasing it. The task's first action is to take `ctx->mtx`, so it simply waits until the handle is
  stored; `scheduleDelayTask()` never waits for the task. `CancelReconfigurationTask()` also calls
  `cancelDelayTask()` under `ctx->mtx`, the same direction; `bartoncommon` runs the task's callback
  with none of its own locks held, so there is no reverse edge.
- The D7 event is emitted after `reconfigureDeviceTask()` removes its context, which is also what
  releases shutdown's wait for pending reconfigurations (bounded at 120 s, D8). A shutdown beginning between the
  `deviceServiceIsShuttingDown()` check and the emit could reach `deviceEventProducerShutdown()`
  first only after stopping every driver and subsystem; even then the client object stays alive
  (the caller of `b_core_client_stop()` holds a reference), so the worst case is a GLib critical
  for a NULL emitter, not a crash.

### GObject / GIR implications
- New `GError`-returning signatures are introspectable; GIR annotations (`@err`,
  `(transfer ...)`) must be added so bindings generate `raises`/exception semantics.
- No new signals or properties. `device-configuration-completed`/`-failed` gain a new emission
  context, distinguishable by session discovery type `B_CORE_DISCOVERY_TYPE_NONE`.

## Risks / Trade-offs

- [Breaking API/ABI change] → Major version bump via a conventional commit with a `BREAKING CHANGE:`
  footer and a `!` in the PR title (cocogitto generates the CHANGELOG entry and the `SOVERSION`
  follows the major version); in-repo C consumers (reference app, event producer) are updated in the
  same change. Out-of-repo C consumers (e.g. Zilker) must add the new `GError **` argument to six
  functions. Python callers need no code changes; errors surface as `GLib.Error`.
- [Readiness is published slightly later] → D6 moves `isReadyForDeviceOperation` (and with it
  `isReadyForPairing` and discovery of rejectable drivers) to after the startup flagging pass. The
  pass does one metadata read per device, which is negligible for typical installations.
- [Check-then-act window] → The gate is evaluated before the backend call, so a reconfiguration that
  begins between the check and the read is not caught. This is inherent to a non-locking gate and is
  bounded to that instant; closing it fully would require holding a lock across the backend call or
  making the database swap atomic (an explicit non-goal).
- [Pending can last well beyond the database swap] → A device is "pending" from the moment
  reconfiguration is scheduled until it finishes: this includes post-OTA scheduling delays (60s) and,
  for sleepy Zigbee devices, waiting up to `RECONFIGURATION_TIMEOUT_SEC` (30 min) for a poll
  check-in. For that whole period the gated reads and `write_metadata` return
  `B_CORE_SERVICE_STATE_ERROR_DEVICE_RECONFIGURING`; list getters still include the device (D4),
  commands and resource writes still work (D5), and the D7 event tells clients when to retry.
- [In-process event handlers that use the public getters] → A handler running inside the device
  service that looks a device up through a gated getter to enrich an event will now get an error for a
  pending device, or before readiness, and may drop the event. Zilker's resource-updated IPC forwarder
  does this. Such handlers should use the internal `deviceServiceGet*` lookups, as this change does
  for the event producer (see Migration Plan).
- [Callers that pass `NULL` for `err`] → They still get `NULL`/`FALSE`, now also for "busy", which
  they cannot tell apart from "not found". No in-tree or known caller deletes data after a failed
  single-device lookup, but callers should pass an error to react properly.
- [Existing configuration-event listeners see a new emission context] → Listeners that assumed
  these events only occur during discovery will now also see them after reconfiguration. The session
  discovery type (`B_CORE_DISCOVERY_TYPE_NONE`) lets them tell the two apart. In-tree and known
  consumers either forward all events generically or only listen while a discovery is active.
  Out-of-repo bridges that collapse the session type to a boolean (e.g. Zilker's IPC forwarder maps
  only `RECOVERY` to `forRecovery`) lose that distinction and should forward it.
- [Terminated reconfigurations report as failed] → Removing a device mid-reconfiguration, or a
  driver cancelling it (e.g. a sleepy Zigbee device without a poll-control endpoint), makes the
  configure step return false, so clients see `device-configuration-failed`. That is accurate (the
  reconfiguration did not complete); for a removal it is followed by `device-removed`.
- [Determinism of the integration test] → `deviceServiceReconfigureDevice()` sets the
  pending-reconfiguration flag synchronously (before scheduling the deferred work), so a query
  issued immediately after it returns deterministically observes `B_CORE_SERVICE_STATE_ERROR_DEVICE_RECONFIGURING` — no timing
  race.
- [Integration test cannot restart the client or drive the startup path] → The Matter SDK is a
  one-shot C++ singleton (`Matter::Stop()` never reverses `chip::Platform::MemoryInit()` /
  `InitChipStack()`), so the client cannot be stopped and restarted in-process to re-run the startup
  reconfiguration path, and the harness already isolates each test in its own process for this
  reason. There is also no public API to trigger reconfiguration, and seeding a Matter device into
  storage fails because the subsystem drops a device with no fabric. The integration test therefore
  commissions a real matter.js device and calls the library's exported
  `deviceServiceReconfigureDevice()` directly (via `ctypes`) with a long delay: this marks the
  device pending synchronously (what the gate checks) while the actual reconfiguration work is
  deferred and cancelled at teardown, so the (Matter-driver-specific) reconfiguration round-trip
  never runs. This keeps the test local, deterministic, and free of new product API surface. The
  cost is that the test depends on that private symbol staying exported. It must stay exported
  anyway: Zilker's device IPC handler calls `deviceServiceReconfigureDevice()` directly.
- [Startup ordering, completion and the D8 paths need a unit-test harness] → The pending map only
  exists after `deviceServiceStart()`, which brings up the event system, drivers and subsystems
  (including Matter), and `--wrap` cannot intercept calls within `deviceService.c`. Every collaborator
  those paths use lives in another file, though, so `deviceServiceReconfigurationTest.c` wraps them:
  it starts the real service with drivers, subsystems and events faked, keeps devices in the real JSON
  database in a temporary directory, captures the subsystem-ready callback, and captures scheduled
  reconfiguration tasks instead of running them, so each test decides when they run or whether a
  cancel succeeds. It covers the D6 ordering (flagged before readiness is published; readiness withheld
  if shutdown begins mid-pass; repeated passes), D7 (un-pend before the event; startup failure
  fallback) and D8 (scheduling failure, shutdown insert refusal, cancel success, removal and shutdown
  blocking until a running task finishes). No production code was restructured for it. What it cannot
  show: a real driver's reconfiguration completing (the integration harness can't restart Barton
  in-process, and running one to completion crashes in the Matter SBMD driver for reasons unrelated to
  this change), and the condition-variable clock fix, whose only observable effect is that waits block
  instead of spinning; the timing assertions pass either way.

## Migration Plan

1. Land the new error enum and gating with updated signatures.
2. GIR/pygobject stubs regenerate on the install target; the reference app and event producer are
   updated in this change.
3. Commit with a `BREAKING CHANGE:` footer (and `!` in the PR title) so the release tooling performs
   the major version bump.
4. Rollback: revert the change; no persisted data/schema changes are involved.

Guidance for consumers:
- Pass a `GError **` to the six changed functions and handle `B_CORE_SERVICE_STATE_ERROR_NOT_READY` /
  `B_CORE_SERVICE_STATE_ERROR_DEVICE_RECONFIGURING` as "retry later", not "not found". Passing `NULL` compiles and
  runs but hides the reason.
- Code running inside the device-service process that enriches events (e.g. a resource-updated
  handler looking up the device or endpoint) should use the internal `deviceServiceGet*` lookups, which
  are never gated.
- Re-query a device on `device-configuration-completed`/`-failed` with session discovery type `NONE`.
- `b_core_client_stop()` can take up to 120 s longer when a reconfiguration is running, because it
  lets it finish; a host that kills the process sooner causes it to be retried at the next start (D8).

Zilker-specific follow-ups (out of scope for this repository):
- The resource-updated IPC forwarder resolves the device and endpoint through the gated public
  getters; switch it to the internal lookups so events are not dropped while a device is pending.
- The configuration-event forwarders read the device class into the UUID field and collapse the
  session discovery type to a recovery boolean, so the D7 events arrive mislabeled; forward the UUID
  and the full discovery type.
- The IPC layer has no "busy" status; map the new errors to a retryable code rather than "not found".
- Optionally allow the device service up to 120 s to stop (`waitOnShutdown` in the watchdog config),
  so a reconfiguration running at shutdown finishes instead of being retried at the next start.

## Open Questions

- None. Whether list getters should omit reconfiguring devices was resolved in favour of inclusion
  (D4), paired with the completion event (D7). Whether to gate list getters, resource writes and
  commands at all was resolved against it (D4, D5): each would harm clients that ignore the new
  errors more than it protects anyone. Whether every `NULL`/`FALSE` from a gated function should set
  `err` (new "not found"/"write failed" codes) was resolved against it (D3): the existing
  not-found/failure behavior is kept.
