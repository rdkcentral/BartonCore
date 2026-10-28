## Context

`BCoreClient` is the public GObject API through which clients query and mutate device state. Most
query functions (`b_core_client_get_device_by_id()`, `get_device_by_uri()`, `get_endpoint_by_id()`,
`get_endpoint_by_uri()`, `get_resource_by_uri()`, and the list getters) call directly into the
internal `deviceServiceGet*` backend and return a GObject or `GList*`, with no notion of service or
device state. Only `read_resource()` and `read_metadata()` currently expose a `GError **`.

Reconfiguration (`reconfigureDeviceTask()` in `core/src/deviceService.c`) rebuilds a device on its own
delayed-task thread: it constructs a new `icDevice`, then `jsonDatabaseRemoveDeviceById()` the old
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
hash map that `deviceServiceRemoveDevice()` already consults (via `waitForOrCancelPendingReconfiguration()`).

## Goals / Non-Goals

**Goals:**
- Public single-device reads never hand back data for a device that is mid-reconfiguration; instead
  they return a distinct, actionable error (`B_CORE_SERVICE_STATE_ERROR_DEVICE_RECONFIGURING`).
- The same reads return `B_CORE_SERVICE_STATE_ERROR_NOT_READY` before the service is ready for device
  operation.
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
layout rule is coded once in core rather than re-derived in the API layer. It is declared in the
core-internal `core/src/deviceUri.h`, not in the deprecated private headers.

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
outcomes by checking `err` first: set to a `BCoreServiceStateError` code means retry later (other
codes are the function's existing errors, such as `read_resource`'s and `read_metadata`'s own), and
unset with `NULL`/`FALSE` means not found or failed.

### D4: Single-device reads are gated; list getters are not
- Single-device reads (`get_device_by_id/uri`, `get_endpoint_by_id/uri`, `get_resource_by_uri`,
  `read_resource`, `read_metadata`) resolve the owning device UUID and check readiness; if the service
  is ready and the device is pending → `B_CORE_SERVICE_STATE_ERROR_DEVICE_RECONFIGURING`. Readiness takes
  precedence, so a call that fails both reports `NOT_READY`.
- List getters (`get_devices`, `get_devices_by_device_class`, `get_devices_by_subsystem`,
  `get_endpoints_by_profile`, `get_metadata_by_uri`, `query_resources_by_uri`) are not gated at all
  and keep their signatures. They include reconfiguring devices (except in the instant the swap replaces
  the record, a known limitation): such a device still exists (it can
  be removed, operated, and has metadata), so omitting it makes "busy" indistinguishable from
  "removed" with no accompanying `device-removed` event. Clients that reconcile their own records
  against a list would delete it, and because reconfiguration used to announce nothing, a client whose
  initial load missed the device would never learn it exists.
- List getters do not get a `NOT_READY` gate either. The failure mode is the same: a caller that
  ignores the error sees an empty list, which looks like "no devices". A client that loads the device
  list at startup without waiting for readiness, and treats a failed list as an empty one, would read
  zero devices and delete its copies of them. Before readiness the database is already loaded, so the
  list is accurate apart from details that a pending reconfiguration is about to change, which D7 makes
  recoverable.

### D5: Gate metadata writes, not resource writes or commands
- `write_resource` and `execute_resource` are not gated and keep their signatures. They act on the
  live device, and the data they change mostly survives the swap: the rebuilt device re-reads resource
  values from the device, and labels are re-read at swap time. A write that lands after the rebuilt
  device has read its initial values still reaches the device, but its stored value is replaced by the
  earlier reading until the device next reports (pre-existing; closing it needs the swap and the
  write path to be serialized, a separate change). A device is "pending" from the moment
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
  normal `DeviceSynchronizationTask()` instead of calling the driver itself, so the device stops being
  pending without waiting for the synchronization, and drivers that don't implement
  `synchronizeDevice()` are skipped as they are on every other synchronization path. If the thread
  pool won't take the task (its queue is full), the fallback runs the synchronization inline rather
  than drop it, since a dropped fallback would leave the device serving pre-restart data.
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
they are given (for example, a driver may start discovery only once the status reports
`isReadyForPairing`), so notifying earlier would hand them a not-ready status.

As a consequence, startup synchronization and zero-delay reconfiguration can call a driver's
`synchronizeDevice()`/`configureDevice()` before its `serviceStatusChanged()` sees the ready status;
previously that notification came first. No in-tree driver depends on that order: only the Zigbee
common driver wires `serviceStatusChanged()`, and no Zigbee driver implements the callback it
forwards to.

Shutdown can begin while the pass is running (a stop during startup). The pass stops scheduling once
`deviceServiceIsShuttingDown()` is true, and readiness is published only if shutdown has not begun.
That check runs under `subsystemsAreReadyMutex`, and shutdown sets its flag before clearing both
readiness flags under the same mutex, so readiness is never re-published after shutdown clears it.
`deviceServiceIsReadyForDeviceOperation()` also checks the shutting-down flag, so readiness ends the
moment shutdown begins rather than when shutdown later clears the flags. A
ready callback that arrives after shutdown has begun (e.g. a subsystem finishing a delayed
initialization) is ignored entirely, so it can't re-raise `subsystemsReadyForDevices` either. Because
the pass now runs on the ready-callback thread rather than the initializer thread pool (whose
destruction shutdown already waited for), shutdown counts ready callbacks in flight and waits for them
(up to 120 s, like the reconfiguration wait) before tearing down the thread pool and drivers they use.
A callback on shutdown's own thread (a ready-event handler that stops the service) is not waited for.
A repeated pass skips any device that already has a reconfiguration pending, whatever its snapshot
says, so a device pending for another reason (for example after a firmware upgrade) isn't synchronized
underneath that reconfiguration.

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
renamed `DeviceSynchronizationTask()`, since that is all it now does.

### D7: Announce when a reconfiguration finishes
Previously a reconfiguration was silent: the rebuilt device is persisted with events disabled
(`finalizeNewDevice(newDevice, false, false)`), so a client that read a device before or during
reconfiguration had no signal to re-read it. `reconfigureDeviceTask()` now emits, through
`FinishReconfiguration()`, `device-configuration-completed` on success or `device-configuration-failed`
on failure, with session discovery type `B_CORE_DISCOVERY_TYPE_NONE` to mark it as outside any
discovery session.
- Emitted after the device stops being reported as reconfiguring, so a listener can query the device
  immediately without hitting `B_CORE_SERVICE_STATE_ERROR_DEVICE_RECONFIGURING`, and can request a new
  reconfiguration, which is scheduled normally. In one locked step the context leaves the pending map and
  joins a list of finishing reconfigurations; it leaves that list once the event has been sent. Removal
  and shutdown wait for finishing reconfigurations as well as pending ones, so a removal can't emit
  `device-removed` ahead of this event and shutdown can't tear down the event producer while it is being
  sent. For the whole of a device's removal, new reconfigurations of it are refused, and the removal
  loops until none is pending or finishing, so none can run after it and re-save the device. A request
  refused this way is kept and requested again if the removal fails, unless the removal gave up (after
  its 60-second wait) on a reconfiguration that is still running. Reconfigurations of a device with no
  stored record are refused: the record is checked before the insert, and again right after it (outside
  the control lock, see Risks) with the insert backed out, so only a device deleted while the request is
  being made can show as reconfiguring, and only until it is backed out. Every request, including one for
  a device that already has a reconfiguration pending, goes through the same insert, so one made during a
  removal is always deferred rather than dropped as a duplicate. A removal or shutdown started from the
  event's own handler doesn't wait for that event, and a removal still waits at most 60 s in total.
  Because removal and shutdown wait for the event, a client handler that blocks on the thread doing the
  removal or stop holds that thread for up to 60 s or 120 s. The finishing steps (completion callback
  while still pending → move to the finishing list → event → leave the finishing list) live in one
  helper, `FinishReconfiguration()`, which makes that ordering explicit.
- Skipped for any reconfiguration that finishes after shutdown has begun (shutdown only ever waits for
  an event that was already being sent), and when the device record no longer exists at the time of
  sending, checked against the database rather than the task's snapshot. A failed save of the rebuilt
  device removes the record and makes the reconfiguration a failure (`finalizeNewDevice()`'s result is
  used), so it is never reported as success.
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
  reconfiguration that is neither cancelled nor signalled. A request for a device being removed is
  deferred (requested again if the removal fails), including when the device momentarily has no record
  because a reconfiguration is swapping it. A request for a device with no stored record is otherwise
  refused before the insert, after reading the record once more in case it reappeared meanwhile (a swap
  that finished), or backed out after the insert if the device was deleted meanwhile. A request without a
  device UUID is refused outright. A duplicate or shutdown refusal is benign and logged at debug level; a
  deferral or an unknown device is logged at info level. If a removal that gave up waiting for a
  still-running reconfiguration (after its 60-second wait) then fails, a request deferred during it is
  dropped with a warning rather than re-issued, since it would only duplicate the running one.
- Visibility: a `device.reconfiguration.pending` gauge (observability metrics, no labels) reports the
  number of pending reconfigurations (scheduled or running), so a device stuck pending (e.g. the
  thread-start limitation above) shows up in telemetry rather than only as refused reads.
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
  stop budget is shorter will kill the process during that wait. That is safe: until the final swap the
  old record is intact on disk with `reconfigurationRequired` set (the task sets it first), so the next
  start reconfigures the device again. The swap itself deletes
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
  so this cannot deadlock. The cost, before readiness is published, is the version checks for each
  device (plus a metadata read when its versions match), and for each device that needs
  reconfiguring a fresh copy of its record to re-check.
- D8 holds `ctx->mtx` while publishing the context to the pending map (which takes
  `reconfigurationControlMutex`) and across `scheduleDelayTask()`. The only lock order is therefore
  `ctx->mtx` → `reconfigurationControlMutex` → leaf locks only (the gauge's and logging's),
  `ctx->mtx` → the delayed-task lock, and `ctx->mtx` → the database lock (the check that the device has a
  stored record, made after the insert and never under `reconfigurationControlMutex`). The
  one place that used to nest the other way, `cancelPendingReconfigurationTasks()`, now snapshots the
  pending contexts (taking references) under the control lock and inspects and cancels them after
  releasing it. The task's first action is to take `ctx->mtx`, so it simply waits until the handle is
  stored; `scheduleDelayTask()` never waits for the task. `CancelReconfigurationTask()` also calls
  `cancelDelayTask()` under `ctx->mtx`, the same direction; `bartoncommon` runs the task's callback
  with none of its own locks held, so there is no reverse edge.
- The D7 event is sent while the task is on the finishing list, and shutdown's wait (bounded at 120 s,
  D8) covers finishing reconfigurations as well as pending ones, so shutdown can't reach
  `deviceEventProducerShutdown()` while the event is being sent. Only a task still running
  after the 120 s bound can outlive the wait, which is the pre-existing limitation of that bound.

### GObject / GIR implications
- New `GError`-returning signatures are introspectable; GIR annotations (`@err`,
  `(transfer ...)`) must be added so bindings generate `raises`/exception semantics.
- No new signals or properties. `device-configuration-completed`/`-failed` gain a new emission
  context, distinguishable by session discovery type `B_CORE_DISCOVERY_TYPE_NONE`.

## Risks / Trade-offs

- [Breaking API/ABI change] → Major version bump via a conventional commit with a `BREAKING CHANGE:`
  footer and a `!` in the PR title (cocogitto generates the CHANGELOG entry and the `SOVERSION`
  follows the major version); in-repo C consumers (reference app, event producer) are updated in the
  same change. Out-of-repo C consumers must add the new `GError **` argument to six
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
- [Event handlers that use the gated getters] → A signal handler that looks a device up through a
  gated getter to enrich an event will now get an error for a pending device, or before readiness, and
  may drop the event. Such handlers should use what the event carries (a `BCoreResource` includes its
  device UUID, endpoint ID and URI) or retry the lookup after the configuration event, as the guide
  explains. The private `deviceServiceGet*` functions are deprecated and are not a supported way around
  the gate; Barton's own event producer uses them only because it is part of the service.
- [Signal handlers running under the database lock] → The database sends `device-database-failure`
  while it holds its own lock (during initialization, reload and restore). That predates this change:
  a handler that calls back into Barton to read the database hangs, because the lock isn't recursive.
  This change keeps the database lookup it adds out of `reconfigurationControlMutex`, so it adds no
  deadlock between threads. It does make the deprecated private `deviceServiceReconfigureDevice()` read
  the database, so calling it from such a handler now hangs too. That isn't supported (the private
  headers are deprecated). Sending the signal after the database releases its lock would fix both, and
  is left to a separate change.
- [Callers that pass `NULL` for `err`] → They still get `NULL`/`FALSE`, now also for "busy", which
  they cannot tell apart from "not found". No in-tree or known caller deletes data after a failed
  single-device lookup, but callers should pass an error to react properly.
- [Existing configuration-event listeners see a new emission context] → Listeners that assumed
  these events only occur during discovery will now also see them after reconfiguration. The session
  discovery type (`B_CORE_DISCOVERY_TYPE_NONE`) lets them tell the two apart. In-tree and known
  consumers either forward all events generically or only listen while a discovery is active.
  Out-of-repo bridges that collapse the session type to a boolean lose that distinction and should
  forward it.
- [Terminated reconfigurations report as failed] → Removing a device mid-reconfiguration, or a
  driver cancelling it (e.g. a sleepy Zigbee device without a poll-control endpoint), makes the
  configure step return false, so clients see `device-configuration-failed`. That is accurate (the
  reconfiguration did not complete); for a removal it is followed by `device-removed`, unless the
  reconfiguration outlasts the removal's 60 s wait or the removal is made from that event's own handler.
- [Determinism of the integration test] → `deviceServiceReconfigureDevice()` sets the
  pending-reconfiguration flag synchronously (before scheduling the deferred work), so a query
  issued immediately after it returns deterministically observes
  `B_CORE_SERVICE_STATE_ERROR_DEVICE_RECONFIGURING`, with no timing race.
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
  cost is that the test depends on that private symbol staying exported. The in-tree Matter and
  Zigbee drivers call it after a firmware upgrade, so it stays exported anyway. The test uses it only
  to drive Barton's own state; it is not an interface clients should use.
- [Startup ordering, completion and the D8 paths need a unit-test harness] → The pending map only
  exists after `deviceServiceStart()`, which brings up the event system, drivers and subsystems
  (including Matter), and `--wrap` cannot intercept calls within `deviceService.c`. Every collaborator
  those paths use lives in another file, though, so `deviceServiceReconfigurationTest.c` wraps them: it
  starts the real service with drivers, subsystems and events faked, keeps devices in the real JSON
  database in a temporary directory, captures the subsystem-ready callback, and captures scheduled
  reconfiguration tasks instead of running them, so each test decides when they run or whether a cancel
  succeeds. It covers the D6 ordering (flagged before readiness is published; readiness withheld if
  shutdown begins mid-pass; repeated passes), D7 (un-pend before the event; startup failure fallback) and
  D8 (scheduling failure, shutdown insert refusal, cancel success, removal and shutdown blocking until a
  running task finishes). No production code was restructured for it, but it exposed one pre-existing
  bug: `stopMainLoop()` called `g_main_loop_quit()` directly, and a quit issued before the event thread
  entered `g_main_loop_run()` was lost, leaving `pthread_join()` waiting forever when the service was
  stopped right after starting. It now quits repeatedly until the event thread reports that its loop
  returned, which also works when the application iterates the default context itself (a quit sent as
  a source on that context could be dispatched by the application's loop first).
  What it cannot show: a real driver's reconfiguration completing (the integration harness can't restart
  Barton in-process, and running one to completion crashes in the Matter SBMD driver for reasons
  unrelated to this change), and the condition-variable clock fix, whose only observable effect is that
  waits block instead of spinning; the timing assertions pass either way.

## Migration Plan

1. Land the new error enum and gating with updated signatures.
2. GIR/pygobject stubs regenerate on the install target; the reference app and event producer are
   updated in this change.
3. Commit with a `BREAKING CHANGE:` footer (and `!` in the PR title) so the release tooling performs
   the major version bump.
4. Rollback: revert the change; no persisted data/schema changes are involved.

Guidance for consumers:
- Pass a `GError **` to the six changed functions and handle `B_CORE_SERVICE_STATE_ERROR_NOT_READY` /
  `B_CORE_SERVICE_STATE_ERROR_DEVICE_RECONFIGURING` as "retry later", not "not found". Passing `NULL`
  compiles and runs but hides the reason.
- Signal handlers that look up a device or endpoint with a gated function (for example to enrich a
  `resource-updated` event) get the same errors as any other caller. They should use what the event
  carries, or retry after the device's configuration event, rather than the deprecated private
  `deviceServiceGet*` functions.
- Re-query a device on `device-configuration-completed`/`-failed` with session discovery type `NONE`.
- `b_core_client_stop()` can take up to 120 s longer when a reconfiguration is running, because it
  lets it finish; a host that kills the process sooner causes it to be retried at the next start (D8).
- Bridges that forward these events or errors to other processes should forward the device UUID and
  the full session discovery type, and map the new errors to a retryable status rather than
  "not found".

## Open Questions

- None. Whether list getters should omit reconfiguring devices was resolved in favour of inclusion
  (D4), paired with the completion event (D7). Whether to gate list getters, resource writes and
  commands at all was resolved against it (D4, D5): each would harm clients that ignore the new
  errors more than it protects anyone. Whether every `NULL`/`FALSE` from a gated function should set
  `err` (new "not found"/"write failed" codes) was resolved against it (D3): the existing
  not-found/failure behavior is kept.
