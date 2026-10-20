## ADDED Requirements

### Requirement: Driver-registered cleanup hooks
The result builder SHALL provide a non-terminal op `onFailure(handler, context)` that registers a cleanup hook on the current operation. Multiple hooks MAY be registered across the hops of an operation. Registration SHALL NOT expose transaction or DAL internals to the driver.

#### Scenario: Register after a side effect
- **WHEN** an `onResponse` handler for `VideoStreamAllocate` returns `onFailure(deallocate, {videoStreamID: 5})` and a further `requestCommand`
- **THEN** the hook SHALL be associated with the operation and SHALL NOT run while the operation is in progress

---

### Requirement: Exactly-once invocation on failure
Registered hooks SHALL be invoked exactly once when the operation fails for any reason: `error` terminal, deferred command error, per-hop or overall timeout, unmergeable conflict, maximum deferral depth exceeded, pre-send failure, device removal, or driver deactivation while parked. Hooks SHALL NOT be invoked when the operation succeeds. Hooks SHALL run after the failing hop's `onError` handler, in reverse registration order.

#### Scenario: Timeout runs hook once
- **WHEN** a hook is registered and the next `requestCommand` times out
- **THEN** `onError` SHALL be invoked once with `type: "timeout"`, then the hook SHALL be invoked once with `args.failure.type == "timeout"`

#### Scenario: Success discards hooks
- **WHEN** a hook is registered and the operation completes with `success`
- **THEN** the hook SHALL NOT be invoked

#### Scenario: LIFO order
- **WHEN** hooks H1 then H2 are registered and the operation fails
- **THEN** H2 SHALL run before H1

#### Scenario: Device removed while parked
- **WHEN** a device is removed while an operation with a registered hook is parked
- **THEN** the hook SHALL be invoked once with `args.failure.type == "aborted"`

---

### Requirement: Cleanup hook execution context
A cleanup hook SHALL receive `args.handlerContext` (the registered context), `args.failure` (`{ type, message }`), and `args.supplements` read from the Data Access Layer at invocation time using the operation's supplement declaration. Data write ops returned by a hook SHALL be committed in a compensation transaction regardless of the failed operation's outcome. A hook MAY end with `success`, `error` (logged), or `sendCommand` (sent without parking); `requestCommand` and `readAttribute` SHALL be rejected and logged. A hook that throws SHALL be logged and SHALL NOT prevent remaining hooks from running.

#### Scenario: Hook sends a deallocation command
- **WHEN** a hook returns `device.sendCommand(CL_CAMERA_AV_STREAM_MGMT, CMD_VIDEO_STREAM_DEALLOCATE, payload)`
- **THEN** the command SHALL be sent once and the failed operation SHALL complete without waiting for its response

#### Scenario: Hook writes are committed
- **WHEN** a hook returns `mergeTransientData("sessions", {"3": null}, 3600)` and `success`
- **THEN** member `3` SHALL be removed from `sessions` even though the operation's own staged writes were discarded
