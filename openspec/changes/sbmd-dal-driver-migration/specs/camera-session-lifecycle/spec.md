## ADDED Requirements

### Requirement: Concurrent session operations do not lose updates
The camera SBMD driver SHALL NOT erase, resurrect, or otherwise overwrite session state changed by another operation while a `stream` (or other deferred) operation is parked. Session state writes SHALL be applied per session against the current session map.

#### Scenario: Session created during a parked stream survives
- **WHEN** session A's `stream` operation is parked awaiting `VideoStreamAllocateResponse` and `createSession` creates session B
- **THEN** after A's operation completes, session B SHALL still exist

#### Scenario: Session destroyed during a parked stream stays destroyed
- **WHEN** session A's `stream` operation is parked and `destroySession` destroys session B
- **THEN** after A's operation completes, session B SHALL NOT exist

#### Scenario: Own session destroyed during a parked stream
- **WHEN** session A's `stream` operation is parked and `destroySession` destroys session A
- **THEN** the `stream` operation SHALL fail, session A SHALL NOT exist, and any allocated video stream SHALL be deallocated exactly once

---

### Requirement: Failed stream operations release device resources
When a `stream` operation fails after the device allocated a video stream — including pre-send failures, command errors, timeouts, and conflicts — the camera SBMD driver SHALL deallocate the stream on the device exactly once and SHALL NOT leave the session in the `streaming` state.

#### Scenario: SolicitOffer pre-send failure
- **WHEN** `VideoStreamAllocate` succeeds and sending `SolicitOffer` fails before transmission
- **THEN** `VideoStreamDeallocate` SHALL be sent once and the session SHALL NOT be `streaming`

#### Scenario: SolicitOffer timeout
- **WHEN** `VideoStreamAllocate` succeeds and `SolicitOffer` receives no response before its timeout
- **THEN** `VideoStreamDeallocate` SHALL be sent once and the session SHALL NOT be `streaming`
