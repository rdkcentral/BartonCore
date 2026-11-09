## ADDED Requirements

### Requirement: Concurrent session operations do not lose updates
The camera SBMD driver SHALL NOT erase, resurrect, or otherwise overwrite session state changed by another SBMD Transaction while a `stream` (or other deferred) SBMD Transaction is parked. Session identifiers SHALL be allocated atomically (sequence supplement) so concurrent `createSession` operations never receive the same identifier. Session state writes SHALL be per-session merge patches applied at commit against the current session map, and deferred steps SHALL read session state from supplements fetched at that step rather than from data carried in `context`.

#### Scenario: Session created during a parked stream survives
- **WHEN** session A's `stream` SBMD Transaction is parked awaiting `VideoStreamAllocateResponse` and `createSession` creates session B
- **THEN** after A's SBMD Transaction completes, session B SHALL still exist

#### Scenario: Session destroyed during a parked stream stays destroyed
- **WHEN** session A's `stream` SBMD Transaction is parked and `destroySession` destroys session B
- **THEN** after A's SBMD Transaction completes, session B SHALL NOT exist

#### Scenario: Concurrent createSession calls get distinct ids
- **WHEN** two `createSession` operations run concurrently on the same camera
- **THEN** they SHALL return different session identifiers and both sessions SHALL exist

#### Scenario: Own session destroyed during a parked stream
- **WHEN** session A's `stream` SBMD Transaction is parked and `destroySession` destroys session A
- **THEN** the `stream` operation SHALL fail, session A SHALL NOT exist, and any allocated video stream SHALL be deallocated exactly once

---

### Requirement: Failed stream operations release device resources
When a `stream` SBMD Transaction fails after the device allocated a video stream — including pre-send failures, command errors, timeouts, aborts, and commit failures — the camera SBMD driver's per-step `onError` handlers SHALL return `error` (not recover), and its `onTransactionFailure` handler SHALL send `VideoStreamDeallocate` (Camera AV Stream Management cluster `0x0551`) for the allocated `videoStreamID` exactly once, SHALL NOT leave the session in the `streaming` state, and SHALL update `webrtcError` on the `webrtc` endpoint with a failed status. If no `VideoStreamAllocate` succeeded, the driver SHALL NOT send `VideoStreamDeallocate`.

#### Scenario: SolicitOffer pre-send failure
- **WHEN** `VideoStreamAllocate` succeeds and sending `SolicitOffer` fails before transmission
- **THEN** `VideoStreamDeallocate` SHALL be sent once and the session SHALL NOT be `streaming`

#### Scenario: SolicitOffer timeout
- **WHEN** `VideoStreamAllocate` succeeds and `SolicitOffer` receives no response before its timeout
- **THEN** `VideoStreamDeallocate` SHALL be sent once and the session SHALL NOT be `streaming`

#### Scenario: Allocate fails
- **WHEN** `VideoStreamAllocate` fails
- **THEN** `VideoStreamDeallocate` SHALL NOT be sent, the session SHALL NOT be `streaming`, and `webrtcError` SHALL be updated
